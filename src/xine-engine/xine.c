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
 * $Id: xine.c,v 1.1 2001/04/18 22:36:05 f1rmb Exp $
 *
 * top-level xine functions
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#if defined (__linux__)
#include <endian.h>
#elif defined (__FreeBSD__)
#include <machine/endian.h>
#endif

#include "xine.h"
#include "xine_internal.h"
#include "audio_out.h"
#include "video_out.h"
#include "demuxers/demux.h"
#include "buffer.h"
#include "libac3/ac3.h"
#include "libmpg123/mpg123.h"
#include "libmpg123/mpglib.h"
#include "libmpeg2/mpeg2.h"
#ifdef ARCH_X86
#include "libw32dll/w32codec.h"
#endif
#include "libspudec/spudec.h"
#include "input/input_plugin.h"
#include "metronom.h"
#include "configfile.h"
#include "monitor.h"
#include "video_decoder.h"
#include "audio_decoder.h"

/* debugging purposes only */
uint32_t   xine_debug;

/*
#define TEST_FILE 
*/
#ifdef TEST_FILE
int        gTestFile=-1; 
#endif

/*
 * xine global variables
 */

void xine_notify_stream_finished (xine_t *this) {
  printf ("xine_notify_stream_finished\n");

  xine_stop (this);

  this->status_callback (this->status);
}

/*
 *
 */
void *xine_spu_loop (xine_t *this, void *dummy) {

  buf_element_t *pBuf;
  int bRunning = 1;

  while (bRunning) {
    
    pBuf = this->fifo_funcs->fifo_buffer_get (this->spu_fifo);
    
    switch (pBuf->nType) {
    case BUF_QUIT:
      bRunning = 0;
      break;
    case BUF_RESET:
      spudec_reset ();
      break;
    case BUF_SPU:
      spudec_decode(pBuf->pContent, pBuf->nSize, pBuf->nPTS);
      break;        
    }
    this->fifo_funcs->buffer_pool_free (pBuf);
  }

  return NULL;
}

/*
 *
 */
void xine_stop (xine_t *this) {

  pthread_mutex_lock (&this->xine_lock);

  if (!this->cur_input_plugin) 
    return;
  
  this->mnStatus = XINE_STOP;
  
  if(this->cur_demuxer_plugin) {
    this->cur_demuxer_plugin->demux_stop ();
    this->cur_demuxer_plugin = NULL;
  }

  // FIXME
  this->fifo_funcs->fifo_buffer_clear(this->mBufVideo);
  this->fifo_funcs->fifo_buffer_clear(this->mBufAudio);
  this->fifo_funcs->fifo_buffer_clear(this->spu_fifo);

  if (gAudioOut)
    gAudioOut->close ();

  metronom_reset();
  metronom_stop_clock ();

  vo_reset();

  this->cur_input_plugin->close();
  this->cur_input_plugin = NULL;

  pthread_mutex_unlock (&this->xine_lock);
}

/*
 * *****
 * Demuxers probing stuff
 */
static int try_demux_with_stages(xine_t *this, const char *MRL, 
				 int stage1, int stage2) {
  int s = 0, i;
  int stages[3] = {
    stage1, stage2, -1
  };

  if(stages[0] == -1) {
    fprintf(stderr, "%s(%d) wrong first stage = %d !!\n", 
	    __FUNCTION__, __LINE__, stage1);
    return 0;
  }

  while(stages[s] != -1) {
    for(i = 0; i < this->num_demuxer_plugins; i++) {
      if(this->demuxer_plugins[i].open(this->cur_input_plugin, 
				       MRL, stages[s]) == DEMUX_CAN_HANDLE) {
	
	this->cur_demuxer_plugin = &this->demux_plugins[i];
	
	xprintf(VERBOSE|DEMUX,"demuxer '%s' handle in stage '%s'.\n", 
		this->demux_plugins[i].get_identifier(),
		(stages[s] == STAGE_BY_CONTENT) ? "STAGE_BY_CONTENT"
		: ((stages[s] == STAGE_BY_EXTENSION) ? "STAGE_BY_EXTENSION"
		   : "UNKNOWN"));
	return 1;
      }
#ifdef DEBUG
      else
	xprintf(VERBOSE|DEMUX, "demuxer '%s' cannot handle in stage '%s'.\n", 
		this->demuxer_plugins[i].get_identifier(),
		(stages[s] == STAGE_BY_CONTENT) ? "STAGE_BY_CONTENT"
		: ((stages[s] == STAGE_BY_EXTENSION) ? "STAGE_BY_EXTENSION"
		   : "UNKNOWN"));
#endif
    }
    s++;
  }

  return 0;
}
/*
 * Try to find a demuxer which handle the MRL stream
 */
static int find_demuxer(xine_t *this, const char *MRL) {

  this->cur_demuxer_plugin = NULL;

  switch(this->demux_strategy) {

  case DEMUX_DEFAULT_STRATEGY:
    if(try_demux_with_stages(this, MRL, STAGE_BY_CONTENT, STAGE_BY_EXTENSION))
      return 1;
    break;

  case DEMUX_REVERT_STRATEGY:
    if(try_demux_with_stages(this, MRL, STAGE_BY_EXTENSION, STAGE_BY_CONTENT))
      return 1;
    break;

  case DEMUX_CONTENT_STRATEGY:
    if(try_demux_with_stages(this, MRL, STAGE_BY_CONTENT, -1))
      return 1;
    break;

  case DEMUX_EXTENSION_STRATEGY:
    if(try_demux_with_stages(this, MRL, STAGE_BY_EXTENSION, -1))
      return 1;
    break;

  }
  
  return 0;
}

/*
 *
 */
static void xine_play_internal (xine_t *this, char *MRL, 
				int spos, off_t pos) {

  double     share ;
  off_t      len;
  int        i;

  xprintf (VERBOSE|LOOP, "xine open %s, start pos = %d\n",MRL, spos);

  if (this->status == XINE_PAUSE) {
    xine_pause();
    return;
  }

  if (this->status != XINE_STOP) {
    xine_stop ();
  }

  /*
   * find input plugin
   */
   
  this->cur_input_plugin = NULL;

  for (i = 0; i < this->num_input_plugins; i++) {
    if (this->input_plugins[i].open(MRL)) {
      this->cur_input_plugin = &this->input_plugins[i];
      break;
    }
  }

  if (!this->cur_input_plugin) {
    perror ("open input source");
    this->cur_demuxer_plugin = NULL;
    return;
  }
  
  /*
   * find demuxer plugin
   */

  if(!find_demuxer(this, MRL)) {
    printf ("error: couldn't find demuxer for >%s<\n", MRL);
    return;
  }
  
  vo_set_logo_mode (0);

  /*
   * Init SPU decoder with colour lookup table. 
   */

  if(this->cur_input_plugin->get_clut) 
    spudec_init(this->cur_input_plugin->get_clut());
  else 
    spudec_init(NULL);

  /*
   * metronom
   */

  metronom_reset();

  /*
   * start demuxer
   */
  
  if (spos) {
    len = this->cur_input_plugin->get_length ();
    share = (double) spos / 65535;
    pos = (off_t) (share * len) ;
  }

  this->cur_demuxer_plugin->demux_select_audio_channel (this->audio_channel);
  this->cur_demuxer_plugin->demux_select_spu_channel (this->spu_channel);
  this->cur_demuxer_plugin->demux_start (this->cur_input_plugin,
					 this->mBufVideo, //FIXME
					 this->mBufAudio, 
					 this->spu_fifo, pos);
  
  this->status = XINE_PLAY;
  this->cur_input_pos = pos;

  /*
   * remember MRL
   */

  strncpy (this->cur_mrl, MRL, 1024);

  /*
   * start clock
   */

  metronom_start_clock (0);
}

/*
 *
 */
void xine_play (xine_t *this, char *MRL, int spos) {

  pthread_mutex_lock (&this->xine_lock);

  if (this->status != XINE_PLAY)    
    xine_play_internal (this, MRL, spos, (off_t) 0);

  pthread_mutex_unlock (&this->xine_lock);
}

/*
 *
 */
static int xine_eject (xine_t *this, char *MRL) {
  int i;
  
  pthread_mutex_lock (&this->xine_lock);

  if(this->cur_input_plugin == NULL) {
    
    for (i = 0; i < this->num_input_plugins; i++) {
      if (this->input_pluginss[i].open(MRL)) {
	this->cur_input_plugin = &this->input_plugins[i];
	this->cur_input_plugin->close();
	break;
      }
    }
  }
  
  if (this->status == XINE_STOP
      && this->cur_input_plugin && this->cur_input_plugin->eject_media) {

    pthread_mutex_unlock (&this->xine_lock);

    return this->cur_input_plugin->eject_media ();
  }

  pthread_mutex_unlock (&this->xine_lock);
  return 0;
}

/*
 *
 */
void xine_exit (xine_t *this) {

  void *p;
  buf_element_t *pBuf;

  /*
   * stop decoder threads
   */

  if (this->cur_input_plugin)
    this->cur_input_plugin->demux_stop ();

  this->fifo_funcs->fifo_buffer_clear(this->spu_fifo);

  audio_decoder_shutdown ();
  video_decoder_shutdown ();

  this->status = XINE_QUIT;

  config_file_save ();
}

/*
 *
 */
static void xine_pause (xine_t *this) {

  pthread_mutex_lock (&this->xine_lock);

  if (this->status == XINE_PAUSE) {

    xprintf (VERBOSE, "xine play %s from %Ld\n", 
	     this->cur_mrl, this->cur_input_pos);

    this->status = XINE_STOP;

    xine_play_internal (this, this->cur_mrl, 0, this->cur_input_pos);
    /* this->mnPausePos = 0; */

  } else if (this->status == XINE_PLAY) {

    if (!this->cur_input_plugin) {
      pthread_mutex_unlock (&this->xine_lock);
      return;
    }

    this->status = XINE_PAUSE;

    this->cur_demuxer_plugin->demux_stop ();
    this->cur_demuxer_plugin = NULL;

    //FIXME    
    this->fifo_funcs->fifo_buffer_clear(this->mBufVideo);
    this->fifo_funcs->fifo_buffer_clear(this->mBufAudio);
    this->fifo_funcs->fifo_buffer_clear(this->spu_fifo);
    
    if (gAudioOut)
      gAudioOut->close ();

    metronom_reset();
    metronom_stop_clock ();

    vo_reset ();

    this->cur_input_plugin->close();
  }

  pthread_mutex_unlock (&this->xine_lock);
}

/*
 *
 */
xine_t *xine_init (vo_instance_t *vo, ao_instance_t *ao,
		   gui_status_callback_func_t gui_status_callback,
		   int demux_strategy, uint32_t debug_lvl) {

  xine_t *this = xmalloc (sizeof (xine_t));
  int err;

  this->status_callback = gui_status_callback;
  this->demux_strategy  = demux_strategy;
  xine_debug            = debug_lvl;

#ifdef TEST_FILE
  gTestFile = open ("/tmp/test.mp3", O_WRONLY | O_CREAT, 0644); 
#endif

  /*
   * init lock
   */

  pthread_mutex_init (&this->xine_lock, NULL);

  /*
   * Init buffers
   */

  this->fifo_funcs = buffer_pool_init (2000, 4096);
  this->spu_fifo   = this->fifo_funcs->fifo_buffer_new ();

  /*
   * init demuxer
   */
  
  xine_load_demux_plugins();

  this->audio_channel = 0;
  this->spu_channel   = -1;
  this->cur_input_pos = 0;

  printf ("xine_init: demuxer initialized\n");

  /*
   * init and start decoder threads
   */

  this->mBufVideo = video_decoder_init (vo);

  this->mBufAudio = audio_decoder_init (ao);

  /*
   * init SPU decoder, start SPU thread
   */

  spudec_init(NULL); 

  if((err = pthread_create (&this->spu_thread, NULL, 
			    xine_spu_loop, NULL)) != 0) {
    fprintf(stderr, "pthread_create failed: return code %d.\n", err);
    exit(1);
  }
  else
    printf ("xine_init: SPU thread created\n");
  
  /*
   * load input plugins
   */
  
  xine_load_input_plugins ();
  
  printf ("xine_init: plugins loaded\n");
  
  return this;
}

/*
 *
 */
int xine_get_audio_channel (xine_t *this) {

  return this->audio_channel;
}

/*
 *
 */
void xine_select_audio_channel (xine_t *this, int nChannel) {

  pthread_mutex_lock (&this->xine_lock);

  this->audio_channel = nChannel;

  if (this->cur_demuxer_plugin) {
    this->cur_demuxer_plugin->demux_select_audio_channel (this->audio_channel);
  }

  pthread_mutex_unlock (&this->xine_lock);
}

/*
 *
 */
int xine_get_spu_channel (xine_t *this) {

  return this->spu_channel;
}

/*
 *
 */
void xine_select_spu_channel (xine_t *this, int nChannel) {

  pthread_mutex_lock (&this->xine_lock);

  this->spu_channel = (nChannel >= -1 ? nChannel : -1);

  if (this->cur_demuxer_plugin) 
    this->cur_demuxer_plugin->demux_select_spu_channel (this->spu_channel);

  pthread_mutex_unlock (&this->xine_lock);
}

/*
 *
 */
input_plugin_t* xine_get_input_plugin_list (xine_t *this, int *nInputPlugins) {

  *nInputPlugins = this->num_input_plugins;
  return this->input_plugins;
}

/*
 *
 */
int xine_get_current_position (xine_t *this) {

  off_t len;
  double share;
  
  pthread_mutex_lock (&this->xine_lock);

  if (!this->cur_input_plugin) {
    xprintf (VERBOSE|INPUT, "xine_get_current_position: no input source\n");
    pthread_mutex_unlock (&this->xine_lock);
    return 0;
  }
  
  /* pos = this->mCurInput->seek (0, SEEK_CUR); */
  len = this->cur_input_plugin->get_length ();

  share = (double) this->cur_input_pos / (double) len * 65535;

  pthread_mutex_unlock (&this->xine_lock);

  return (int) share;
}

/*
 *
 */
int xine_get_status(xine_t *this) {

  return this->status;
}
