/*
 * Copyright (C) 2000-2001 the xine project
 * 
 * Copyright (C) James Courtier-Dutton James@superbug.demon.co.uk - July 2001
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
 * $Id: xine_decoder.c,v 1.33 2001/11/15 23:18:04 guenter Exp $
 *
 * stuff needed to turn libspu into a xine decoder plugin
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "spu.h"
#include "buffer.h"
#include "events.h"
#include "xine_internal.h"
#include "video_out/alphablend.h"
#include "xine-engine/bswap.h"
#include "monitor.h"

static void spudec_print_overlay( vo_overlay_t *ovl );

#define LOG_DEBUG 1

#ifdef DEBUG

# ifdef	__GNUC__
#  define LOG(lvl, fmt...)	fprintf (stderr, fmt);
# else
#  define LOG(lvl, ...)		fprintf (stderr, __VA_ARGS__);
# endif

#else /* !DEBUG */

# ifdef __GNUC__
#  define LOG(lvl, fmt...)
# else
#  define LOG(lvl, ...)
# endif

#endif /* !DEBUG */

static clut_t __default_clut[] = {
  CLUT_Y_CR_CB_INIT(0x00, 0x80, 0x80),
  CLUT_Y_CR_CB_INIT(0xbf, 0x80, 0x80),
  CLUT_Y_CR_CB_INIT(0x10, 0x80, 0x80),
  CLUT_Y_CR_CB_INIT(0x28, 0x6d, 0xef),
  CLUT_Y_CR_CB_INIT(0x51, 0xef, 0x5a),
  CLUT_Y_CR_CB_INIT(0xbf, 0x80, 0x80),
  CLUT_Y_CR_CB_INIT(0x36, 0x80, 0x80),
  CLUT_Y_CR_CB_INIT(0x28, 0x6d, 0xef),
  CLUT_Y_CR_CB_INIT(0xbf, 0x80, 0x80),
  CLUT_Y_CR_CB_INIT(0x51, 0x80, 0x80),
  CLUT_Y_CR_CB_INIT(0xbf, 0x80, 0x80),
  CLUT_Y_CR_CB_INIT(0x10, 0x80, 0x80),
  CLUT_Y_CR_CB_INIT(0x28, 0x6d, 0xef),
  CLUT_Y_CR_CB_INIT(0x5c, 0x80, 0x80),
  CLUT_Y_CR_CB_INIT(0xbf, 0x80, 0x80),
  CLUT_Y_CR_CB_INIT(0x1c, 0x80, 0x80),
  CLUT_Y_CR_CB_INIT(0x28, 0x6d, 0xef)
};

#define NUM_SEQ_BUFFERS 50
#define MAX_OBJECTS 50
#define MAX_STREAMS 32
#define MAX_EVENTS 50
#define MAX_SHOWING 5

#define EVENT_NULL 0
#define EVENT_SHOW_SPU 1
#define EVENT_HIDE_SPU 2
#define EVENT_HIDE_MENU 3
#define EVENT_MENU_SPU 4
#define EVENT_MENU_BUTTON 5
#define EVENT_DELETE_RESOURCE 6 /* Maybe release handle will do this */
#define EVENT_SHOW_OSD 7 /* Not yet implemented */

typedef struct spu_object_s {
  int32_t	 handle; /* Used to match Show and Hide events. */
  uint32_t	 object_type; /* 0=Subtitle, 1=Menu */
  uint32_t       pts;         /* Needed for Menu button compares */
  vo_overlay_t  *overlay;  /* The image data. */
  uint32_t       palette_type; /* 1 Y'CrCB, 2 R'G'B' */
  uint32_t	*palette; /* If NULL, no palette contained in this event. */
} spu_object_t;

/* This will hold all details of an event item, needed for event queue to function */
typedef struct spu_overlay_event_s {
  uint32_t	 event_type;  /* Show SPU, Show OSD, Hide etc. */
  uint32_t	 vpts;  /* Time when event will action. 0 means action now */
/* Once video_out blend_yuv etc. can take rle_elem_t with Colour, blend and length information.
 * we can remove clut and blend from this structure.
 * This will allow for many more colours for OSD.
 */
  spu_object_t   object; /* The image data. */
} spu_overlay_event_t;

typedef struct spu_overlay_events_s {
  spu_overlay_event_t  *event;
  uint32_t	next_event;
} spu_overlay_events_t;

typedef struct spu_showing_s {
  int32_t	handle; /* -1 means not allocated */
} spu_showing_t;


typedef struct spudec_stream_state_s {
  spu_seq_t        ra_seq;
  uint32_t         ra_complete;
  uint32_t         stream_filter;
  spu_state_t      state;
  uint32_t         vpts;
  uint32_t         pts;
} spudec_stream_state_t;

typedef struct spudec_decoder_s {
  spu_decoder_t    spu_decoder;
  ovl_src_t        ovl_src;

  xine_t          *xine;
/*  spu_seq_t	   seq_list[NUM_SEQ_BUFFERS]; */
  spu_seq_t       *cur_seq;
  spudec_stream_state_t spu_stream_state[MAX_STREAMS];
  pthread_mutex_t  spu_events_mutex;  
  spu_overlay_events_t     spu_events[MAX_EVENTS];
  spu_overlay_event_t      event;
  spu_object_t     object;  
  pthread_mutex_t  spu_objects_mutex;  
  spu_object_t     spu_objects[MAX_OBJECTS];
  pthread_mutex_t  spu_showing_mutex; /* Probably not needed */ 
  spu_showing_t    spu_showing[MAX_SHOWING];  
  spu_seq_t       *ra_seq;
  int              ra_complete;

  uint32_t         ovl_pts;
  uint32_t         buf_pts;
  spu_state_t      state;

  vo_instance_t   *vo_out;
  vo_overlay_t     overlay;
  int              ovl_caps;
  int              output_open;

} spudec_decoder_t;

static int spudec_can_handle (spu_decoder_t *this_gen, int buf_type) {
  int type = buf_type & 0xFFFF0000;
  return (type == BUF_SPU_PACKAGE || type == BUF_SPU_CLUT || type == BUF_SPU_SUBP_CONTROL) ;
}

/* FIXME: This function needs checking */
static void spudec_reset (spudec_decoder_t *this) {
  int i;
  this->ovl_pts = 0;
  this->buf_pts = 0;

  this->state.visible = 0;

//  this->seq_list[0].finished = 1;   /* mark as cur_seq */
//  for (i = 1; i < NUM_SEQ_BUFFERS; i++) {
//    this->seq_list[i].finished = 2; /* free for reassembly */
//  }
  for (i=0; i < MAX_STREAMS; i++) {
    this->spu_stream_state[i].stream_filter = 1; /* So it works with non-navdvd plugins */
    this->spu_stream_state[i].ra_complete = 1;
  }

  for (i=0; i < MAX_EVENTS; i++) {
    if (this->spu_events[i].event == NULL) {
      xprintf (VERBOSE|SPU, "MALLOC1: this->spu_events[%d].event %p, len=%d\n",
               i,
               this->spu_events[i].event,
               sizeof(spu_overlay_event_t));
      this->spu_events[i].event = xmalloc (sizeof(spu_overlay_event_t));
      xprintf (VERBOSE|SPU, "MALLOC2: this->spu_events[%d].event %p, len=%d\n",
               i,
               this->spu_events[i].event,
               sizeof(spu_overlay_event_t));
      this->spu_events[i].event->event_type = 0;  /* Empty slot */
    }
  }
  for (i=0; i < MAX_OBJECTS; i++) {
    this->spu_objects[i].handle = -1;
  }
  /* Initialise the menu object */
  this->spu_objects[1].handle=1;
  this->spu_objects[1].object_type=1;
  this->spu_objects[1].pts=0;
  xprintf (VERBOSE|SPU, "MALLOC1: this->spu_objects[1].overlay %p, len=%d\n",
            this->spu_objects[1].overlay, sizeof(vo_overlay_t));
  this->spu_objects[1].overlay = xmalloc (sizeof(vo_overlay_t));
  xprintf (VERBOSE|SPU, "MALLOC2: this->spu_objects[1].overlay %p, len=%d\n",
            this->spu_objects[1].overlay, sizeof(vo_overlay_t));
/* xmalloc does memset */
/*  memset(this->spu_objects[1].overlay,0,sizeof(vo_overlay_t));
 */
  
  pthread_mutex_init (&this->spu_events_mutex,NULL);
  pthread_mutex_init (&this->spu_objects_mutex,NULL);
  pthread_mutex_init (&this->spu_showing_mutex,NULL);

/* I don't think I need this.
  this->cur_seq = this->ra_seq = this->seq_list;
 */
}


static void spudec_init (spu_decoder_t *this_gen, vo_instance_t *vo_out) {

  spudec_decoder_t *this = (spudec_decoder_t *) this_gen;

  this->vo_out      = vo_out;
  this->ovl_caps    = vo_out->get_capabilities(vo_out);
  this->output_open = 0;

  spudec_reset(this);
/* FIXME:Do we really need a default clut? */
  memcpy(this->state.clut, __default_clut, sizeof(this->state.clut));
  this->state.need_clut = 1;
  vo_out->register_ovl_src(vo_out, &this->ovl_src);
}

/* allocate a handle from the object pool
 */
static int32_t spu_get_handle(spudec_decoder_t *this) {
  int n;
  n=0;
  do {
    n++;
  } while ( ( n<MAX_OBJECTS ) && ( this->spu_objects[n].handle > -1 ) );
  if (n >= MAX_OBJECTS) return -1;
  this->spu_objects[n].handle=n;
  return n;
}

/* allocate a menu handle from the object pool
 */
static int32_t spu_get_menu_handle(spudec_decoder_t *this) {
  return 1;  /* This might be dynamic later */
}

/* free a handle from the object pool
 */
static void spu_free_handle(spudec_decoder_t *this, int32_t handle) {
  this->spu_objects[handle].handle = -1;
}

/* add an event to the events queue, sort the queue based on vpts.
 * This can be the API entry point for DVD subtitles.
 * In future this could also be the entry point for OSD as well as other Subtitle formats.
 * One calls this function with an event, the event contains an overlay and a vpts when to action/process it.
 * vpts of 0 means action the event now.
 * One also has a handle, so one can match show and hide events.
 * FIXME: Implement Event queue locking. A different thread calls spu_get_overlay, which removes events from the queue.
 */
static int32_t spu_add_event(spudec_decoder_t *this,  spu_overlay_event_t *event) {
  int found;
  uint32_t   last_event,this_event,new_event;
  new_event=0;
  /* We skip the 0 entry because that is used as a pointer to the first event.*/
  /* Find a free event slot */
  xprintf (VERBOSE|SPU, "284MUTEX1:spu_events lock");
  pthread_mutex_lock (&this->spu_events_mutex);
  xprintf (VERBOSE|SPU, "->ok\n");
  do {
    new_event++;
  } while ((new_event<MAX_EVENTS) && (this->spu_events[new_event].event->event_type > 0));
  if (new_event >= MAX_EVENTS) {
    pthread_mutex_unlock (&this->spu_events_mutex);
    return -1;
  }
  /* Find position in event queue to be added. */
  this_event=0;
  found=0;
  /* Find where in the current queue to insert the event. I.E. Sort it. */
  do {
    last_event=this_event;
    this_event=this->spu_events[last_event].next_event;
    if (this_event == 0) {
      found=1;
      break;
    }
    xprintf (VERBOSE|SPU, "this_event=%d vpts %d\n",this_event, this->spu_events[this_event].event->vpts);
    if (this->spu_events[this_event].event->vpts > event->vpts ) {
      found=2;
      break;
    }  
  } while ((this_event != 0) && (found == 0));
  if (last_event >= MAX_EVENTS) {
    pthread_mutex_unlock (&this->spu_events_mutex);
    fprintf(stderr, "No spare subtitle event slots\n");
    return -1;
  }
  /* memcpy everything except the actual image */
  this->spu_events[last_event].next_event=new_event;
  this->spu_events[new_event].next_event=this_event;
  if ( this->spu_events[new_event].event == NULL ) {
    fprintf(stderr,"COMPLAIN BIG TIME!\n");
  }
  this->spu_events[new_event].event->event_type=event->event_type;
  this->spu_events[new_event].event->vpts=event->vpts;
  this->spu_events[new_event].event->object.handle=event->object.handle;
  xprintf (VERBOSE|SPU, "323MALLOC1: this->spu_events[new_event=%d].event->object.overlay %p, len=%d\n",
            new_event,
            this->spu_events[new_event].event->object.overlay,
            sizeof(vo_overlay_t));
  this->spu_events[new_event].event->object.overlay = xmalloc (sizeof(vo_overlay_t));
  xprintf (VERBOSE|SPU, "328MALLOC2: this->spu_events[new_event=%d].event->object.overlay %p, len=%d\n",
            new_event,
            this->spu_events[new_event].event->object.overlay,
            sizeof(vo_overlay_t));
  memcpy(this->spu_events[new_event].event->object.overlay, 
    event->object.overlay, sizeof(vo_overlay_t));
  memset(event->object.overlay,0,sizeof(vo_overlay_t));
//  spudec_print_overlay( event->object.overlay );
//  spudec_print_overlay( this->spu_events[new_event].event->object.overlay );  
  pthread_mutex_unlock (&this->spu_events_mutex);
   
  return new_event;
}

/* empty the object queue
 * release all handles currently allocated.
 * IMPLEMENT ME.
 */
static void spu_free_all_handles(spudec_decoder_t *this) {
return;
}

static void spu_process (spudec_decoder_t *this, uint32_t stream_id) {
//  spu_overlay_event_t   *event;
//  spu_object_t  *object;
//  vo_overlay_t  *overlay;
  int handle;
  int pending = 1;
  this->cur_seq = &this->spu_stream_state[stream_id].ra_seq;

/* FIXME:Get Handle after we have found if "Forced display" is set or not. 
 */
    
  xprintf (VERBOSE|SPU, "Found SPU from stream %d pts=%d vpts=%d\n",stream_id, 
          this->spu_stream_state[stream_id].pts,
          this->spu_stream_state[stream_id].vpts); 
  this->state.cmd_ptr = this->cur_seq->buf + this->cur_seq->cmd_offs;
  this->state.next_pts = -1; /* invalidate timestamp */
  this->state.modified = 1; /* Only draw picture if = 1 on first event of SPU */
  this->state.visible = 0; /* 0 - No value, 1 - Show, 2 - Hide. */
  this->state.menu = 0; /* 0 - No value, 1 - Forced Display. */
  this->state.delay = 0;
  this->cur_seq->finished=0;
  handle=spu_get_handle(this);
  if (handle < 0) {
    printf("No SPU Handles left\n");
    return;
  }

  do {
    if (!this->spu_stream_state[stream_id].ra_seq.finished) {
      
      //spudec_nextseq(this);
/* Get do commands to build the event. */
      spu_do_commands(&this->state, this->cur_seq, &this->overlay);
      /* FIXME: Check for Forced-display or subtitle stream
       *        For subtitles, open event.
       *        For menus, store it for later.
       */

      if ((this->xine->spu_channel != stream_id) &&
           (this->state.menu == 0) ) {
        xprintf (VERBOSE|SPU, "Dropping SPU channel %d\n", stream_id);
        spu_free_handle(this, handle);
        return;
      }
      if ((this->state.modified) ) { 
        spu_draw_picture(&this->state, this->cur_seq, &this->overlay);
      }
      
      if (this->state.need_clut)
        spu_discover_clut(&this->state, &this->overlay);
      
      if (this->state.menu == 0) {
        /* Subtitle */
        this->event.object.handle = handle;
        /* FIXME: memcpy maybe. */
        xprintf (VERBOSE|SPU, "403MALLOC: this->event.object.overlay=%p\n",
                  this->event.object.overlay);
        this->event.object.overlay = malloc(sizeof(vo_overlay_t));
        memcpy(this->event.object.overlay, 
               &this->overlay,
               sizeof(vo_overlay_t));
        this->overlay.rle=NULL;
        xprintf (VERBOSE|SPU, "409MALLOC: this->event.object.overlay=%p\n",
                  this->event.object.overlay);
        this->event.event_type = this->state.visible;
        this->event.vpts = this->spu_stream_state[stream_id].vpts+(this->state.delay*1000); 
      } else {
        /* Menu */
        spu_free_handle(this, handle);
        this->event.object.handle = spu_get_menu_handle(this);
        /* FIXME: memcpy maybe. */
        xprintf (VERBOSE|SPU, "418MALLOC: this->event.object.overlay=%p\n",
                  this->event.object.overlay);
        this->event.object.overlay = malloc(sizeof(vo_overlay_t));
        memcpy(this->event.object.overlay, 
               &this->overlay,
               sizeof(vo_overlay_t));
        this->overlay.rle=NULL;
        xprintf (VERBOSE|SPU, "424MALLOC: this->event.object.overlay=%p\n",
                  this->event.object.overlay);
        this->event.event_type = EVENT_MENU_SPU;
        this->event.vpts = this->spu_stream_state[stream_id].vpts+(this->state.delay*1000); 
      }
      spu_add_event(this, &this->event);
    } else {
      pending = 0;
    }
  } while (pending);

}

static void spudec_decode_data (spu_decoder_t *this_gen, buf_element_t *buf) {
  uint32_t stream_id;
  spu_seq_t       *cur_seq;
  spudec_decoder_t *this = (spudec_decoder_t *) this_gen;
  stream_id = buf->type & 0x1f ;
  cur_seq = &this->spu_stream_state[stream_id].ra_seq;

  if (buf->type == BUF_SPU_CLUT) {
    if (buf->content[0]) { /* cheap endianess detection */
      memcpy(this->state.clut, buf->content, sizeof(uint32_t)*16);
    } else {
      int i;
      uint32_t *clut = (uint32_t*) buf->content;
      for (i = 0; i < 16; i++)
        this->state.clut[i] = bswap_32(clut[i]);
    }
    this->state.need_clut = 0;
    return;
  }
  
  if (buf->type == BUF_SPU_SUBP_CONTROL) {
    int i;
    uint32_t *subp_control = (uint32_t*) buf->content;
    for (i = 0; i < 32; i++) {
      this->spu_stream_state[i].stream_filter = subp_control[i]; 
    }
    return;
  }


  if (buf->decoder_info[0] == 0)  /* skip preview data */
    return;

  if ( this->spu_stream_state[stream_id].stream_filter == 0) 
    return;

  if (buf->PTS) {
    metronom_t *metronom = this->ovl_src.metronom;
    uint32_t vpts = metronom->got_spu_packet(metronom, buf->PTS, 0, buf->SCR);
    if (vpts < this->buf_pts) {
      /* FIXME: Don't do this yet, 
         because it will cause all sorts of 
         problems with malloc. 
       */
      /* spudec_reset(this); */
    }

    this->spu_stream_state[stream_id].vpts = vpts; /* Show timer */
    this->spu_stream_state[stream_id].pts = buf->PTS; /* Required to match up with NAV packets */
  }

/*  if (this->ra_complete) {
    spu_seq_t *tmp_seq = this->ra_seq + 1;
    if (tmp_seq >= this->seq_list + NUM_SEQ_BUFFERS)
      tmp_seq = this->seq_list;
    if (tmp_seq->finished > 1) {
      this->ra_seq = tmp_seq;
      this->ra_seq->PTS = this->buf_pts; 
    }
  }
 */
  stream_id = buf->type & 0x1f ;
  this->spu_stream_state[stream_id].ra_complete = 
     spu_reassembly(&this->spu_stream_state[stream_id].ra_seq,
                     this->spu_stream_state[stream_id].ra_complete,
                     buf->content,
                     buf->size);
  if(this->spu_stream_state[stream_id].ra_complete == 1) { 
    spu_process(this,stream_id);
  }
}

static void spudec_close (spu_decoder_t *this_gen) {
  spudec_decoder_t *this = (spudec_decoder_t *) this_gen;
  
  this->vo_out->unregister_ovl_src(this->vo_out, &this->ovl_src);
}
/* This function is probably not needed now */
static void spudec_nextseq(spudec_decoder_t* this) {
  spu_seq_t *tmp_seq = this->cur_seq + 1;
/*  if (tmp_seq >= this->seq_list + NUM_SEQ_BUFFERS)
    tmp_seq = this->seq_list;
 */
 
  if (!tmp_seq->finished) { /* is the next seq ready for process? */
    this->cur_seq->finished = 2; /* ready for reassembly */
    this->cur_seq = tmp_seq;
    this->state.cmd_ptr = this->cur_seq->buf + this->cur_seq->cmd_offs;
    this->state.next_pts = -1; /* invalidate timestamp */
    this->state.modified = 1;
    this->state.visible = 0;
    this->state.menu = 0;
  }
}

static void spudec_print_overlay( vo_overlay_t *ovl ) {
  xprintf (VERBOSE|SPU, "OVERLAY to show\n");
  xprintf (VERBOSE|SPU, "\tx = %d y = %d width = %d height = %d\n",
	   ovl->x, ovl->y, ovl->width, ovl->height );
  xprintf (VERBOSE|SPU, "\tclut [%x %x %x %x]\n",
	   ovl->color[0], ovl->color[1], ovl->color[2], ovl->color[3]);
  xprintf (VERBOSE|SPU, "\ttrans [%d %d %d %d]\n",
	   ovl->trans[0], ovl->trans[1], ovl->trans[2], ovl->trans[3]);
  xprintf (VERBOSE|SPU, "\tclip top=%d bottom=%d left=%d right=%d\n",
	   ovl->clip_top, ovl->clip_bottom, ovl->clip_left, ovl->clip_right);
  return;
} 

/* FIXME:Some optimization needs to happen here. */
static void spu_process_event( spudec_decoder_t *this, int vpts ) {
  int32_t      handle;
  uint32_t  this_event;
//  uint32_t     vpts;
//  uint32_t     pts;
//  int i;
//  vo_overlay_t overlay;
  xprintf (VERBOSE|SPU, "557MUTEX1:spu_events lock");
  pthread_mutex_lock (&this->spu_events_mutex);
  xprintf (VERBOSE|SPU, " -> ok.\n");
  this_event=this->spu_events[0].next_event;
  if ((!this_event) || (vpts < this->spu_events[this_event].event->vpts) ) {
    pthread_mutex_unlock (&this->spu_events_mutex);
    return;
  }

  handle=this->spu_events[this_event].event->object.handle;
  switch( this->spu_events[this_event].event->event_type ) {
    case EVENT_SHOW_SPU:
      xprintf (VERBOSE|SPU, "SHOW SPU NOW\n");
      if (this->spu_events[this_event].event->object.overlay != NULL) {
        this->spu_objects[handle].handle = handle;
        xprintf (VERBOSE|SPU, "POINTER1: this->spu_objects[handle=%d].overlay=%p\n",
                  handle,
                  this->spu_objects[handle].overlay);
        this->spu_objects[handle].overlay = this->spu_events[this_event].event->object.overlay;
        xprintf (VERBOSE|SPU, "POINTER2: this->spu_objects[handle=%d].overlay=%p\n",
                  handle,
                  this->spu_objects[handle].overlay);
        this->spu_events[this_event].event->object.overlay = NULL;
      }
      this->spu_showing[1].handle = handle;
      break;

    case EVENT_HIDE_SPU:
      xprintf (VERBOSE|SPU, "HIDE SPU NOW\n");
      this->spu_showing[1].handle = -1;
      if(this->spu_objects[handle].overlay->rle) {
        xprintf (VERBOSE|SPU, "FREE1: this->spu_objects[%d].overlay->rle %p\n",
          handle,
          this->spu_objects[handle].overlay->rle);
        free(this->spu_objects[handle].overlay->rle);
        this->spu_objects[handle].overlay->rle = NULL;
        xprintf (VERBOSE|SPU, "FREE2: this->spu_objects[%d].overlay->rle %p\n",
          handle,
          this->spu_objects[handle].overlay->rle);
      }
      xprintf (VERBOSE|SPU, "RLE clear=%08X\n",(uint32_t)this->spu_objects[handle].overlay->rle);
      if (this->spu_objects[handle].overlay) {
        xprintf (VERBOSE|SPU, "FREE1: this->spu_objects[%d].overlay %p\n",
          handle,
          this->spu_objects[handle].overlay);
        free(this->spu_objects[handle].overlay);
        this->spu_objects[handle].overlay = NULL;
        xprintf (VERBOSE|SPU, "FREE2: this->spu_objects[%d].overlay %p\n",
          handle,
          this->spu_objects[handle].overlay);
      }
      spu_free_handle( this, handle );
      break;

    case EVENT_HIDE_MENU:
      xprintf (VERBOSE|SPU, "HIDE MENU NOW %d\n",handle);
      this->spu_showing[1].handle = -1;
      if(this->spu_objects[handle].overlay->rle) {
        xprintf (VERBOSE|SPU, "FREE1: this->spu_objects[%d].overlay->rle %p\n",
          handle,
          this->spu_objects[handle].overlay->rle);
        free(this->spu_objects[handle].overlay->rle);
        this->spu_objects[handle].overlay->rle = NULL;
        xprintf (VERBOSE|SPU, "FREE2: this->spu_objects[%d].overlay->rle %p\n",
          handle,
          this->spu_objects[handle].overlay->rle);
      }
      
      /* FIXME: maybe free something here */
      /* spu_free_handle( this, handle ); */
      break;

    case EVENT_MENU_SPU:
      xprintf (VERBOSE|SPU, "MENU SPU NOW\n");
      if (this->spu_events[this_event].event->object.overlay != NULL) {
        vo_overlay_t *overlay = this->spu_objects[handle].overlay;
        vo_overlay_t *event_overlay = this->spu_events[this_event].event->object.overlay;
        xprintf (VERBOSE|SPU, "event_overlay\n");
        spudec_print_overlay(event_overlay);
        xprintf (VERBOSE|SPU, "overlay\n");
        spudec_print_overlay(overlay);

        this->spu_objects[handle].handle = handle; /* This should not change for menus */
        /* If rle is not empty, free it first */
        if(overlay && overlay->rle) {
          xprintf (VERBOSE|SPU, "FREE1: overlay->rle %p\n",
            overlay->rle);
          free (overlay->rle);
          overlay->rle = NULL;
          xprintf (VERBOSE|SPU, "FREE2: overlay->rle %p\n",
            overlay->rle);
        }
	overlay->rle = event_overlay->rle;
	overlay->data_size = event_overlay->data_size;
	overlay->num_rle = event_overlay->num_rle;
	overlay->x = event_overlay->x;
	overlay->y = event_overlay->y;
	overlay->width = event_overlay->width;
	overlay->height = event_overlay->height;
	overlay->rgb_clut = event_overlay->rgb_clut;
        if((event_overlay->color[0] +
            event_overlay->color[1] +
            event_overlay->color[2] +
            event_overlay->color[3]) > 0 ) {
          xprintf (VERBOSE|SPU, "mixing clut\n");
          overlay->color[0] = event_overlay->color[0];
          overlay->color[1] = event_overlay->color[1];
          overlay->color[2] = event_overlay->color[2];
          overlay->color[3] = event_overlay->color[3];
        }
        if((event_overlay->trans[0] +
            event_overlay->trans[1] +
            event_overlay->trans[2] +
            event_overlay->trans[3]) > 0 ) {
          xprintf (VERBOSE|SPU, "mixing trans\n");
          overlay->trans[0] = event_overlay->trans[0];
          overlay->trans[1] = event_overlay->trans[1];
          overlay->trans[2] = event_overlay->trans[2];
          overlay->trans[3] = event_overlay->trans[3];
        }
        this->spu_showing[1].handle = handle;
        xprintf (VERBOSE|SPU, "overlay after\n");
        spudec_print_overlay(overlay);
        xprintf (VERBOSE|SPU, "FREE1: event_overlay %p\n",
          this->spu_events[this_event].event->object.overlay = NULL);
        /* The null test was done at the start of this case statement */
        free (this->spu_events[this_event].event->object.overlay);
        this->spu_events[this_event].event->object.overlay = NULL;
        xprintf (VERBOSE|SPU, "FREE2: event_ovlerlay %p\n",
          this->spu_events[this_event].event->object.overlay);
      }
      break;

    case EVENT_MENU_BUTTON:
      xprintf (VERBOSE|SPU, "MENU BUTTON NOW\n");
      if (this->spu_events[this_event].event->object.overlay != NULL) {
        vo_overlay_t *overlay = this->spu_objects[handle].overlay;
        vo_overlay_t *event_overlay = this->spu_events[this_event].event->object.overlay;
        xprintf (VERBOSE|SPU, "event_overlay\n");
        spudec_print_overlay(event_overlay);
        xprintf (VERBOSE|SPU, "overlay\n");
        spudec_print_overlay(overlay);
        this->spu_objects[handle].handle = handle; /* This should not change for menus */
        overlay->clip_top = event_overlay->clip_top;
        overlay->clip_bottom = event_overlay->clip_bottom;
        overlay->clip_left = event_overlay->clip_left;
        overlay->clip_right = event_overlay->clip_right;
        //overlay->rgb_clut = event_overlay->rgb_clut;  /* May needed later for OSD */
        if((event_overlay->color[0] +
            event_overlay->color[1] +
            event_overlay->color[2] +
            event_overlay->color[3]) > 0 ) {
          xprintf (VERBOSE|SPU, "mixing clut\n");
          overlay->color[0] = event_overlay->color[0];
          overlay->color[1] = event_overlay->color[1];
          overlay->color[2] = event_overlay->color[2];
          overlay->color[3] = event_overlay->color[3];
        }
        if((event_overlay->trans[0] +
            event_overlay->trans[1] +
            event_overlay->trans[2] +
            event_overlay->trans[3]) > 0 ) {
          xprintf (VERBOSE|SPU, "mixing trans\n");
          overlay->trans[0] = event_overlay->trans[0];
          overlay->trans[1] = event_overlay->trans[1];
          overlay->trans[2] = event_overlay->trans[2];
          overlay->trans[3] = event_overlay->trans[3];
        }
        this->spu_showing[1].handle = handle;
        xprintf (VERBOSE|SPU, "overlay after\n");
        spudec_print_overlay(overlay);
        xprintf (VERBOSE|SPU, "FREE1: event_overlay %p\n",
          this->spu_events[this_event].event->object.overlay = NULL);
        /* The null test was done at the start of this case statement */
        free (this->spu_events[this_event].event->object.overlay);
        this->spu_events[this_event].event->object.overlay = NULL;
        xprintf (VERBOSE|SPU, "FREE2: event_ovlerlay %p\n",
          this->spu_events[this_event].event->object.overlay);
      }
      break;

    default:
      xprintf (VERBOSE|SPU, "Unhandled event type\n");
      break;

  }
  this->spu_events[0].next_event = this->spu_events[this_event].next_event;    
  this->spu_events[this_event].next_event = 0;
  this->spu_events[this_event].event->event_type = 0;
  pthread_mutex_unlock (&this->spu_events_mutex);
}
  
/* This is called from video_out and should return one image.
 * Repeated calls will display the next image.
 * until the last image is reached, when a NULL is returned
 * and the pointer starts from the beginning again.
 * For now, a max of 5 diffent images can be displayed at once.
 * Maybe we should add a callback instead of calling the function all the time.
 * We could rename this then to "Render overlay"
 * This function needs an API change, to add a callback function to do the actual overlay.
 * In this way we can get multiple objects displayed at the same time.
 * i.e. Subtitles and OSD.
 * Currently, only one object can be displayed at the same time.
 * The first checkin of this code should add no more functionality than the current code.
 * It was just be checked in so others can test it.
 */

static vo_overlay_t* spudec_get_overlay(ovl_src_t *ovl_src, int vpts) {
  int32_t  handle;
  spudec_decoder_t *this = (spudec_decoder_t*) ovl_src->src_gen;

  /* Look at next event, if current video vpts > first event on queue, process the event 
   * else just continue 
   */
  spu_process_event( this, vpts );
  /* Scan through 5 entries and display any present. 
   * Currently, only 1 entry is scanned, until API can change.
   */
  handle=this->spu_showing[1].handle; /* handle is only valid if the object is currently visable. */
  if (handle < 0) return NULL;
  return this->spu_objects[handle].overlay;
}

static void spudec_event_listener(void *this_gen, xine_event_t *event_gen) {
  spudec_decoder_t *this  = (spudec_decoder_t *) this_gen;
  xine_spu_event_t *event = (xine_spu_event_t *) event_gen;

  if((!this) || (!event)) {
    return;
  }

  switch (event->event.type) {
  case XINE_EVENT_SPU_BUTTON:
    {
      spu_overlay_event_t *overlay_event = NULL;
      vo_overlay_t        *overlay = NULL;
      spu_button_t        *but = event->data;
      xprintf (VERBOSE|SPU, "MALLOC1: overlay_event %p, len=%d\n",
               overlay_event,
               sizeof(spu_overlay_event_t));
      overlay_event = xmalloc (sizeof(spu_overlay_event_t));
      xprintf (VERBOSE|SPU, "MALLOC2: overlay_event %p, len=%d\n",
               overlay_event,
               sizeof(spu_overlay_event_t));
      xprintf (VERBOSE|SPU, "MALLOC1: overlay %p, len=%d\n",
               overlay,
               sizeof(vo_overlay_t));
      overlay = xmalloc (sizeof(vo_overlay_t));
      xprintf (VERBOSE|SPU, "MALLOC2: overlay %p, len=%d\n",
               overlay,
               sizeof(vo_overlay_t));
      overlay_event->object.overlay=overlay;

      xprintf (VERBOSE|SPU, "BUTTON\n");
      xprintf (VERBOSE|SPU, "\tshow=%d\n",but->show);
      xprintf (VERBOSE|SPU, "\tclut [%x %x %x %x]\n",
	   but->color[0], but->color[1], but->color[2], but->color[3]);
      xprintf (VERBOSE|SPU, "\ttrans [%d %d %d %d]\n",
	   but->trans[0], but->trans[1], but->trans[2], but->trans[3]);
      xprintf (VERBOSE|SPU, "\tleft = %d right = %d top = %d bottom = %d\n",
	   but->left, but->right, but->top, but->bottom );
      if (!this->state.menu) return;
      
      if (but->show) {
        overlay_event->object.handle = spu_get_menu_handle(this);
        overlay_event->object.overlay=overlay;
        overlay_event->event_type = EVENT_MENU_BUTTON;
        overlay_event->vpts = 0; /* Activate it NOW */
        overlay->clip_top = but->top;
        overlay->clip_bottom = but->bottom;
        overlay->clip_left = but->left;
        overlay->clip_right = but->right;
        overlay->color[0] = this->state.clut[but->color[0]];
        overlay->color[1] = this->state.clut[but->color[1]];
        overlay->color[2] = this->state.clut[but->color[2]];
        overlay->color[3] = this->state.clut[but->color[3]];
        overlay->trans[0] = but->trans[0];
        overlay->trans[1] = but->trans[1];
        overlay->trans[2] = but->trans[2];
        overlay->trans[3] = but->trans[3];
        spu_add_event(this, overlay_event);
      } else {
        overlay_event->object.handle = spu_get_menu_handle(this);
        overlay_event->event_type = EVENT_HIDE_MENU;
        overlay_event->vpts = 0; /* Activate it NOW */
        spu_add_event(this, overlay_event);
      }
    }
    break;
  case XINE_EVENT_SPU_CLUT:
    {
    /* FIXME: This function will need checking before it works. */
      spu_cltbl_t *clut = event->data;
      if (clut) {
        memcpy(this->state.clut, clut->clut, sizeof(int32_t)*16);
        this->state.need_clut = 0;
      }
    }
    break;
  /* What is this for?
   * This event is for GUI -> NAVDVD plugin
   * SPUDEC will have to use a different EVENT
   * if it needs this for CLUT auto detect.
   * ->that was a patch to redetect clut on spu channel change.
   *   not very important, i will take a look when i have time. [MF]
   */
  /* FIXME
  case XINE_UI_GET_SPU_LANG:
    {
      this->state.need_clut = 1;
    }
    break;
  */

  }
}

static char *spudec_get_id(void) {
  return "spudec";
}

spu_decoder_t *init_spu_decoder_plugin (int iface_version, xine_t *xine) {

  spudec_decoder_t *this ;

  if (iface_version != 4) {
    fprintf(stderr,
     "libspudec: Doesn't support plugin API version %d.\n"
     "libspudec: This means there is a version mismatch between XINE and\n"
     "libspudec: this plugin.\n", iface_version);
    return NULL;
  }

  this = (spudec_decoder_t *) xmalloc (sizeof (spudec_decoder_t));
/* xmalloc does memset */
/*  memset (this, 0, sizeof(*this)); */

  this->spu_decoder.interface_version   = 4;
  this->spu_decoder.can_handle          = spudec_can_handle;
  this->spu_decoder.init                = spudec_init;
  this->spu_decoder.decode_data         = spudec_decode_data;
  this->spu_decoder.close               = spudec_close;
  this->spu_decoder.get_identifier      = spudec_get_id;
  this->spu_decoder.priority            = 1;

  this->ovl_src.src_gen                 = this;
  this->ovl_src.get_overlay             = spudec_get_overlay;
  this->xine                            = xine;
 
  xine_register_event_listener(xine, spudec_event_listener, this);

  return (spu_decoder_t *) this;
}

