/*
 * Copyright (C) 2000-2001 the xine project
 * 
 * This file is part of xine, a unix video player.
 * 
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: audio_decoder.c,v 1.47 2001/10/18 18:50:53 guenter Exp $
 *
 *
 * functions that implement audio decoding
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>

#include "xine_internal.h"
#include "utils.h"
#include "monitor.h"



void *audio_decoder_loop (void *this_gen) {

  buf_element_t   *buf;
  xine_t          *this = (xine_t *) this_gen;
  int              running = 1;
  audio_decoder_t *decoder;
  static int	   prof_audio_decode = -1;

  if (prof_audio_decode == -1)
    prof_audio_decode = profiler_allocate_slot ("audio decoder/output");

  while (running) {

    /* printf ("audio_loop: waiting for package...\n");  */

    buf = this->audio_fifo->get (this->audio_fifo);

    /*
    printf ("audio_loop: got package pts = %d, type = %08x\n", 
	    buf->PTS, buf->type); 
    */

    if (buf->input_pos)
      this->cur_input_pos = buf->input_pos;
    
    if (buf->input_time)
      this->cur_input_time = buf->input_time;
    
    switch (buf->type) {
      
    case BUF_CONTROL_START:
      if (this->cur_audio_decoder_plugin) {
	this->cur_audio_decoder_plugin->close (this->cur_audio_decoder_plugin);
	this->cur_audio_decoder_plugin = NULL;
      }
      
      pthread_mutex_lock (&this->finished_lock);
      this->audio_finished = 0;
      pthread_mutex_unlock (&this->finished_lock);

      this->metronom->audio_stream_start (this->metronom);
      
      break;
      
    case BUF_CONTROL_END:

      this->metronom->audio_stream_end (this->metronom);
      
      if (this->cur_audio_decoder_plugin) {
	this->cur_audio_decoder_plugin->close (this->cur_audio_decoder_plugin);
	this->cur_audio_decoder_plugin = NULL;
      }
      
      pthread_mutex_lock (&this->finished_lock);

      if (!this->audio_finished && (buf->decoder_info[0]==0)) {
        this->audio_finished = 1;

        if (this->video_finished) {
          xine_notify_stream_finished (this);
        }
      }

      pthread_mutex_unlock (&this->finished_lock);

      /* future magic - coming soon
      lrb_flush (this->audio_temp);
      */

      break;
      
    case BUF_CONTROL_QUIT:
      if (this->cur_audio_decoder_plugin) {
	this->cur_audio_decoder_plugin->close (this->cur_audio_decoder_plugin);
	this->cur_audio_decoder_plugin = NULL;
      }
      running = 0;
      break;

    case BUF_VIDEO_FILL:
      this->metronom->got_audio_still (this->metronom);
      break;

    case BUF_CONTROL_NOP:
      break;

    case BUF_CONTROL_DISCONTINUITY:
      this->metronom->expect_audio_discontinuity (this->metronom);
      break;

    case BUF_CONTROL_AUDIO_CHANNEL:

      printf ("audio_decoder: switching to streamtype %08x\n",
	      buf->decoder_info[0]);
	if (this->audio_channel != (buf->decoder_info[0] & 0xff) ) {
	  /* close old audio decoder */
	  if (this->cur_audio_decoder_plugin) {
	    this->cur_audio_decoder_plugin->close (this->cur_audio_decoder_plugin);
	    this->cur_audio_decoder_plugin = NULL;
	  }
	}
      this->audio_channel = buf->decoder_info[0] & 0xff;

      /* future magic - coming soon
      lrb_feedback (this->audio_temp, this->audio_fifo);
      */

      break;

    default:

      while (this->audio_mute==2) {
	xine_usec_sleep (50000);
      }

      if (this->audio_mute)
	break;

      profiler_start_count (prof_audio_decode);

      if ( (buf->type & 0xFF000000) == BUF_AUDIO_BASE ) {
	
	/* now, decode this buffer if it's the right track */
	  
	if (this->audio_channel == (buf->type & 0xFF) ) {
	    
	  int streamtype = (buf->type>>16) & 0xFF;
	  decoder = this->audio_decoder_plugins [streamtype];
	  if (decoder) {
	    if (this->cur_audio_decoder_plugin != decoder) {
	      if (this->cur_audio_decoder_plugin) 
		this->cur_audio_decoder_plugin->close (this->cur_audio_decoder_plugin);

	      this->cur_audio_decoder_plugin = decoder;
	      this->cur_audio_decoder_plugin->init (this->cur_audio_decoder_plugin, this->audio_out);

	      printf ("audio_loop: using decoder >%s< \n",
		      decoder->get_identifier());

	    }
	    /* printf ("audio_loop: sending data to decoder\n");  */
	    decoder->decode_data (decoder, buf);
	    /* printf ("audio_loop: decoding is done\n");  */
	  }
	} else {
	  /*
	  printf ("audio_decoder: wrong channel\n");
	  lrb_add (this->audio_temp, buf);
	  continue;
	  */
	}
      } else
	printf ("audio_loop: unknown buffer type: %08x\n", buf->type);

      profiler_stop_count (prof_audio_decode);
    }
    
    buf->free_buffer (buf);
  }

  pthread_exit(NULL);
}

void audio_decoder_init (xine_t *this) {

  pthread_attr_t       pth_attrs;
  struct sched_param   pth_params;
  int                  err;

  if (this->audio_out == NULL) {
    this->audio_finished = 1;    
    this->audio_fifo     = NULL;
    return;
  }
  
  this->audio_fifo = fifo_buffer_new (1500, 8192);

  /* future magic - coming soon
  this->audio_temp = lrb_new (100, this->audio_fifo);
  */

  pthread_attr_init(&pth_attrs);
  pthread_attr_getschedparam(&pth_attrs, &pth_params);
  pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);
  pthread_attr_setschedparam(&pth_attrs, &pth_params);
  pthread_attr_setscope(&pth_attrs, PTHREAD_SCOPE_SYSTEM);
  
  if ((err = pthread_create (&this->audio_thread,
			     &pth_attrs, audio_decoder_loop, this)) != 0) {
    fprintf (stderr, "audio_decoder: can't create new thread (%s)\n",
	     strerror(err));
    exit (1);
  }
}

void audio_decoder_shutdown (xine_t *this) {

  buf_element_t *buf;
  void          *p;

  if (this->audio_fifo) {
    /* this->audio_fifo->clear(this->audio_fifo); */

    buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
    buf->type = BUF_CONTROL_QUIT;
    this->audio_fifo->put (this->audio_fifo, buf);
    
    pthread_join (this->audio_thread, &p); 
  }
  
  if(this->audio_out)
    this->audio_out->exit (this->audio_out);

}

