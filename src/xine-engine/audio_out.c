/* 
 * Copyright (C) 2000-2002 the xine project
 * 
 * This file is part of xine, a free video player.
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
 * along with self program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: audio_out.c,v 1.52 2002/04/14 00:24:45 guenter Exp $
 * 
 * 22-8-2001 James imported some useful AC3 sections from the previous alsa driver.
 *   (c) 2001 Andy Lo A Foe <andy@alsaplayer.org>
 * 20-8-2001 First implementation of Audio sync and Audio driver separation.
 *   (c) 2001 James Courtier-Dutton James@superbug.demon.co.uk
 * 
 * General Programming Guidelines: -
 * New concept of an "audio_frame".
 * An audio_frame consists of all the samples required to fill every audio channel to a full amount of bits.
 * So, it does not mater how many bits per sample, or how many audio channels are being used, the number of audio_frames is the same.
 * E.g.  16 bit stereo is 4 bytes, but one frame.
 *       16 bit 5.1 surround is 12 bytes, but one frame.
 * The purpose of this is to make the audio_sync code a lot more readable, rather than having to multiply by the amount of channels all the time
 * when dealing with audio_bytes instead of audio_frames.
 *
 * The number of samples passed to/from the audio driver is also sent in units of audio_frames.
 * 
 * Currently, James has tested with OSS: Standard stereo out, SPDIF PCM, SPDIF AC3
 *                                 ALSA: Standard stereo out
 * No testing has been done of ALSA SPDIF AC3 or any 4,5,5.1 channel output.
 * Currently, I don't think resampling functions, as I cannot test it.
 */

#ifndef	__sun
/* required for swab() */
#define _XOPEN_SOURCE 500
#endif
/* required for FNDELAY decl */
#define _BSD_SOURCE 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <inttypes.h>

#include "xine_internal.h"
#include "xineutils.h"
#include "audio_out.h"
#include "resample.h"
#include "metronom.h"

/*
#define LOG
*/

#define NUM_AUDIO_BUFFERS       32
#define AUDIO_BUF_SIZE       32768

#define ZERO_BUF_SIZE         5000

/* By adding gap errors (difference between reported and expected
 * sound card clock) into metronom's vpts_offset we can use its 
 * smoothing algorithms to correct sound card clock drifts.
 * obs: previously this error was added to xine scr.
 *
 * audio buf ---> metronom --> audio fifo --> (buf->vpts - hw_vpts)
 *           (vpts_offset + error)                     gap
 *                    <---------- control --------------|
 *
 * Unfortunately audio fifo adds a large delay to our closed loop.
 *
 * These are designed to avoid updating the metronom too fast.
 * - it will only be updated 1 time per second (so it has a chance of
 *   distributing the error for several frames).
 * - it will only be updated 2 times for the whole audio fifo size
 *   length (so the control will wait to see the feedback effect)
 * - each update will be of gap/SYNC_GAP_RATE.
 *
 * Sound card clock correction can only provide smooth playback for
 * errors < 1% nominal rate. For bigger errors (bad streams) audio
 * buffers may be dropped or gaps filled with silence.
 */
#define SYNC_TIME_INVERVAL  (1 * 90000)
#define SYNC_BUF_INTERVAL   NUM_AUDIO_BUFFERS / 2
#define SYNC_GAP_RATE       4

struct audio_fifo_s {
  audio_buffer_t    *first;
  audio_buffer_t    *last;
  int                num_buffers;

  pthread_mutex_t    mutex;
  pthread_cond_t     not_empty;
};


static audio_fifo_t *fifo_new () {

  audio_fifo_t *fifo;

  fifo = (audio_fifo_t *) xine_xmalloc (sizeof (audio_fifo_t));

  if (!fifo) {
    printf ("audio_out: out of memory!\n");
    return NULL;
  }

  fifo->first       = NULL;
  fifo->last        = NULL;
  fifo->num_buffers = 0;
  pthread_mutex_init (&fifo->mutex, NULL);
  pthread_cond_init  (&fifo->not_empty, NULL);

  return fifo;
}

static void fifo_append (audio_fifo_t *fifo,
			 audio_buffer_t *buf) {

  pthread_mutex_lock (&fifo->mutex);

  buf->next = NULL;

  if (!fifo->first) {

    fifo->first       = buf;
    fifo->last        = buf;
    fifo->num_buffers = 1;

  } else {

    fifo->last->next = buf;
    fifo->last       = buf;
    fifo->num_buffers++;

  }

  pthread_cond_signal (&fifo->not_empty);
  pthread_mutex_unlock (&fifo->mutex);
}

static audio_buffer_t *fifo_remove (audio_fifo_t *fifo) {

  audio_buffer_t *buf;

  pthread_mutex_lock (&fifo->mutex);

  while (!fifo->first) {
    pthread_cond_wait (&fifo->not_empty, &fifo->mutex);
  }

  buf = fifo->first;

  if (buf) {
    fifo->first = buf->next;

    if (!fifo->first) {

      fifo->last = NULL;
      fifo->num_buffers = 0;
      pthread_cond_init  (&fifo->not_empty, NULL);

    } else 
      fifo->num_buffers--;

  }
    
  pthread_mutex_unlock (&fifo->mutex);

  return buf;
}

/* 
 * This routine is currently not used, but I do not want to loose it.
 * I think "(c) 2001 Andy Lo A Foe <andy@alsaplayer.org>" originally added it
 * to ./xine-lib/src/audio_out/audio_alsa_out.c before the architecture changes
 * So it has moved to here.
 */

void write_pause_burst(ao_instance_t *this, uint32_t num_frames)
{ 
  int error = 0;
  unsigned char buf[8192];
  unsigned short *sbuf = (unsigned short *)&buf[0];
  
  sbuf[0] = 0xf872;
  sbuf[1] = 0x4e1f;
  
  if (error == 0)
    /* Audio ES Channel empty, wait for DD Decoder or pause */
    sbuf[2] = 0x0003;
  else
    /* user stop, skip or error */
    sbuf[2] = 0x0103;

  sbuf[3] = 0x0020;
  sbuf[4] = 0x0000;
  sbuf[5] = 0x0000;

  memset(&sbuf[6], 0, 6144 - 96);
  while (num_frames > 1536) {
    if(num_frames > 1536) {
      this->driver->write(this->driver, sbuf, 1536);
      num_frames -= 1536;
    } else {
      this->driver->write(this->driver, sbuf, num_frames);
      num_frames = 0;
    }
  }

}


static void ao_fill_gap (ao_instance_t *this, int64_t pts_len) {

  int num_frames ;

  num_frames = pts_len * this->frames_per_kpts / 1024;

  printf ("audio_out: inserting %d 0-frames to fill a gap of %lld pts\n",
	  num_frames, pts_len);

  if ((this->mode == AO_CAP_MODE_A52) || (this->mode == AO_CAP_MODE_AC5)) {
    write_pause_burst(this,num_frames);
    return; 
  }


  while (num_frames > 0) {
    if (num_frames > ZERO_BUF_SIZE) {
      this->driver->write(this->driver, this->zero_space, ZERO_BUF_SIZE);
      num_frames -= ZERO_BUF_SIZE;
    } else {
      this->driver->write(this->driver, this->zero_space, num_frames);
      num_frames = 0;
    }
  }
}


static void *ao_loop (void *this_gen) {

  ao_instance_t  *this = (ao_instance_t *) this_gen;
  int64_t         hw_vpts;
  audio_buffer_t *buf;
  int64_t         gap;
  int             delay;
  int64_t         cur_time;
  int             num_output_frames ;
  int             paused_wait;
  int64_t         last_sync_time;
  int             bufs_since_sync;

  last_sync_time = bufs_since_sync = 0;
  
  while ((this->audio_loop_running) ||
	 (!this->audio_loop_running && this->out_fifo->first)) {

    buf = fifo_remove (this->out_fifo);
    bufs_since_sync++;

#ifdef LOG
    printf ("audio_out: got a buffer\n");
#endif

    do {
      delay = this->driver->delay(this->driver);

      /*
       * where, in the timeline is the "end" of the 
       * hardware audio buffer at the moment?
       */
    
      cur_time = this->metronom->get_current_time (this->metronom);
      hw_vpts = cur_time;
  
#ifdef LOG
      printf ("audio_out: current delay is %d, current time is %lld\n",
	      delay, cur_time);
#endif

      /* External A52 decoder delay correction */
      if ((this->mode==AO_CAP_MODE_A52) || (this->mode==AO_CAP_MODE_AC5)) 
        delay+=10; 
  
      hw_vpts += delay * 1024 / this->frames_per_kpts;
  
      /*
       * calculate gap:
       */
    
      gap = buf->vpts - hw_vpts;

      /* wait until user unpauses stream
         audio_paused == 1 means we are playing at a different speed
         them we must process buffers otherwise the entire engine will stop.
      */
      paused_wait = (this->audio_paused == 2) ||
        (this->audio_paused && gap > this->gap_tolerance);
      
      if ( paused_wait )
        xine_usec_sleep (50000);
    } while ( paused_wait );

#ifdef LOG
    printf ("audio_out: hw_vpts : %lld   buffer_vpts : %lld   gap : %lld\n",
	    hw_vpts, buf->vpts, gap);
#endif

    /*
     * output audio data synced to master clock
     */
    
    if (gap < (-1 * AO_MAX_GAP) || !buf->num_frames || 
        this->audio_paused ) {

      /* drop package */

#ifdef LOG
      printf ("audio_out: audio package (vpts = %lld, gap = %lld) dropped\n", 
	      buf->vpts, gap);
#endif

    } else {
      
      /* for small gaps ( tolerance < abs(gap) < AO_MAX_GAP ) 
       * feedback them into metronom's vpts_offset. 
       */
      if ( abs(gap) < AO_MAX_GAP && abs(gap) > this->gap_tolerance &&
           cur_time > (last_sync_time + SYNC_TIME_INVERVAL) && 
           bufs_since_sync >= SYNC_BUF_INTERVAL ) {
           
        this->metronom->set_option(this->metronom, METRONOM_ADJ_VPTS_OFFSET,
                                   -gap/SYNC_GAP_RATE );
        last_sync_time = cur_time;
        bufs_since_sync = 0;
      }
      
      /* for big gaps output silence */
      if ( gap > AO_MAX_GAP ) {
        ao_fill_gap (this, gap/2);
      }
  
      /*
       * resample and output audio data
       */

      num_output_frames = (double) buf->num_frames * this->frame_rate_factor;
      
#ifdef LOG
      printf ("audio_out: outputting %d frames\n", num_output_frames);
#endif
      
      if ((!this->do_resample) 
	  && (this->mode != AO_CAP_MODE_A52) 
	  && (this->mode != AO_CAP_MODE_AC5)) {

	this->driver->write (this->driver, buf->mem,
			     buf->num_frames );
      } else switch (this->mode) {
      case AO_CAP_MODE_MONO:
	audio_out_resample_mono (buf->mem, buf->num_frames,
				 this->frame_buffer, num_output_frames);
	this->driver->write(this->driver, this->frame_buffer, num_output_frames);
	break;
      case AO_CAP_MODE_STEREO:
	audio_out_resample_stereo (buf->mem, buf->num_frames,
				   this->frame_buffer, num_output_frames);
	this->driver->write(this->driver, this->frame_buffer, num_output_frames);
	break;
      case AO_CAP_MODE_4CHANNEL:
	audio_out_resample_4channel (buf->mem, buf->num_frames,
				     this->frame_buffer, num_output_frames);
	this->driver->write(this->driver, this->frame_buffer, num_output_frames);
	break;
      case AO_CAP_MODE_5CHANNEL:
	audio_out_resample_5channel (buf->mem, buf->num_frames,
				     this->frame_buffer, num_output_frames);
	this->driver->write(this->driver, this->frame_buffer, num_output_frames);
	break;
      case AO_CAP_MODE_5_1CHANNEL:
	audio_out_resample_6channel (buf->mem, buf->num_frames,
				     this->frame_buffer, num_output_frames);
	this->driver->write(this->driver, this->frame_buffer, num_output_frames);
	break;
      case AO_CAP_MODE_A52:
      case AO_CAP_MODE_AC5:
	this->driver->write(this->driver, buf->mem, buf->num_frames);
	break;
      }
      
    }

    fifo_append (this->free_fifo, buf);

  }

  pthread_exit(NULL);

  return NULL;
}

/*
 * open the audio device for writing to, start audio output thread
 */

static int ao_open(ao_instance_t *this,
		   uint32_t bits, uint32_t rate, int mode) {
 
  int output_sample_rate, err;

  xine_log (this->xine, XINE_LOG_FORMAT,
	    "audio_out: stream audio format is %d kHz sampling rate, %d bits. mode is %d.\n",
	    rate, bits, mode);

  if ((output_sample_rate=this->driver->open(this->driver,bits,(this->force_rate ? this->force_rate : rate),mode)) == 0) {
    printf("audio_out: open failed!\n");
    return 0;
  }; 

  printf("audio_out: output sample rate %d\n", output_sample_rate);

  this->mode                  = mode;
  this->input_frame_rate      = rate;
  this->bits                  = bits;
  this->last_audio_vpts       = 0;
  this->output_frame_rate     = output_sample_rate;

  switch (this->resample_conf) {
  case 1: /* force off */
    this->do_resample = 0;
    break;
  case 2: /* force on */
    this->do_resample = 1;
    break;
  default: /* AUTO */
    this->do_resample = this->output_frame_rate != this->input_frame_rate;
  }

  /* HACK: we do not have resample functions for 8-bit audio */
  if (this->bits==8)
    this->do_resample = 0;

  if (this->do_resample) 
    printf("audio_out: will resample audio from %d to %d\n",
	   this->input_frame_rate, this->output_frame_rate);

  this->num_channels      = this->driver->num_channels(this->driver); 

  this->frame_rate_factor = (double) this->output_frame_rate / (double) this->input_frame_rate; 
  this->audio_step        = (uint32_t) 90000 * (uint32_t) 32768 / this->input_frame_rate;
  this->frames_per_kpts   = this->output_frame_rate * 1024 / 90000;
#ifdef LOG
  printf ("audio_out : audio_step %d pts per 32768 frames\n", this->audio_step);
#endif

  this->metronom->set_audio_rate(this->metronom, this->audio_step);

  /*
   * start output thread
   */

  if( this->audio_thread ) {
    printf("audio_out: pthread already running!\n");
  }
  
  this->audio_loop_running = 1;  
  if ((err = pthread_create (&this->audio_thread,
			     NULL, ao_loop, this)) != 0) {

    /* FIXME: how does this happen ? */

    printf ("audio_out: can't create thread (%s)\n", strerror(err));
    printf ("audio_out: sorry, this should not happen. please restart xine.\n");
    exit(1);

  } else
    printf ("audio_out: thread created\n");

  return this->output_frame_rate;
}

static audio_buffer_t *ao_get_buffer (ao_instance_t *this) {
  return fifo_remove (this->free_fifo);
}

static void ao_put_buffer (ao_instance_t *this, audio_buffer_t *buf) {

  int64_t pts;

  if (buf->num_frames == 0) {
    fifo_append (this->free_fifo, buf);
    return;
  }

  pts = buf->vpts;

  buf->vpts = this->metronom->got_audio_samples (this->metronom, pts, 
						 buf->num_frames);

#ifdef LOG
  printf ("audio_out: got buffer, pts=%lld, vpts=%lld\n",
	  pts, buf->vpts);
#endif

  if ( buf->vpts<this->last_audio_vpts) {

    /* reject buffer */
    printf ("audio_out: rejected buffer vpts=%lld, last_audio_vpts=%lld\n", 
	    buf->vpts, this->last_audio_vpts);

    fifo_append (this->free_fifo, buf);

  } else {

    fifo_append (this->out_fifo, buf);
    this->last_audio_vpts = buf->vpts;

  }
}

static void ao_close(ao_instance_t *this) {

  audio_buffer_t *audio_buffer;

  printf ("audio_out: stopping thread...\n");

  if (this->audio_loop_running) {
    void *p;

    this->audio_loop_running = 0;
    this->audio_paused = 0;

    audio_buffer = fifo_remove(this->free_fifo);
    audio_buffer->num_frames = 0;
    fifo_append (this->out_fifo, audio_buffer);

    pthread_join (this->audio_thread, &p);
    this->audio_thread = 0;
  }

  printf ("audio_out: thread stopped, closing driver\n");

  this->driver->close(this->driver);  
}

static void ao_exit(ao_instance_t *this) {

  audio_buffer_t *buf, *next;

  this->driver->exit(this->driver);

  free (this->frame_buffer);
  free (this->zero_space);

  buf = this->free_fifo->first;

  while (buf != NULL) {

    next = buf->next;

    free (buf->mem);
    free (buf);

    buf = next;
  }

  buf = this->out_fifo->first;

  while (buf != NULL) {

    next = buf->next;

    free (buf->mem);
    free (buf);

    buf = next;
  }

  free (this->free_fifo);
  free (this->out_fifo);
  free (this);
}

static uint32_t ao_get_capabilities (ao_instance_t *this) {
  uint32_t result;
  result=this->driver->get_capabilities(this->driver);  
  return result;
}

static int ao_get_property (ao_instance_t *this, int property) {

  return(this->driver->get_property(this->driver, property));
}

static int ao_set_property (ao_instance_t *this, int property, int value) {

  return(this->driver->set_property(this->driver, property, value));
}

static int ao_control (ao_instance_t *this, int cmd, ...) {

  va_list args;
  void *arg;
  int rval;

  va_start(args, cmd);
  arg = va_arg(args, void*);
  rval = this->driver->control(this->driver, cmd, arg);
  va_end(args);

  return rval;
}

ao_instance_t *ao_new_instance (ao_driver_t *driver, xine_t *xine) {
 
  config_values_t *config = xine->config;
  ao_instance_t   *this;
  int              i;
  static     char *resample_modes[] = {"auto", "off", "on", NULL};

  this = xine_xmalloc (sizeof (ao_instance_t)) ;

  this->driver                = driver;
  this->metronom              = xine->metronom;
  this->xine                  = xine;

  this->open                  = ao_open;
  this->get_buffer            = ao_get_buffer;
  this->put_buffer            = ao_put_buffer;
  this->close                 = ao_close;
  this->exit                  = ao_exit;
  this->get_capabilities      = ao_get_capabilities;
  this->get_property          = ao_get_property;
  this->set_property          = ao_set_property;
  this->control		      = ao_control;
  this->audio_loop_running    = 0;
  this->audio_paused          = 0;
  /* FIXME: is 4* good enough for all resample cases?? */
  this->frame_buffer          = xine_xmalloc (4 * AUDIO_BUF_SIZE);
  this->zero_space            = xine_xmalloc (ZERO_BUF_SIZE * 2 * 6);
  this->gap_tolerance         = driver->get_gap_tolerance (this->driver);

  this->resample_conf = config->register_enum (config, "audio.resample_mode", 0,
					       resample_modes, "adjust whether resampling is done or not",
					       NULL, NULL, NULL);
  this->force_rate    = config->register_range (config, "audio.force_rate", 0,
						0, 96000, "if !=0 always resample to given rate",
						NULL, NULL, NULL);

  /*
   * pre-allocate memory for samples
   */

  this->free_fifo        = fifo_new ();
  this->out_fifo         = fifo_new ();

  for (i=0; i<NUM_AUDIO_BUFFERS; i++) {

    audio_buffer_t *buf;

    buf = (audio_buffer_t *) malloc (sizeof (audio_buffer_t));
    buf->mem = malloc (AUDIO_BUF_SIZE);
    buf->mem_size = AUDIO_BUF_SIZE;

    fifo_append (this->free_fifo, buf);
  }

  
  return this;
}
