/*
 * decode.c
 * Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * xine-specific version by G. Bartsch
 *
 */

#include "config.h"

#include <stdio.h>
#include <string.h>	/* memcpy/memset, try to remove */
#include <stdlib.h>
#include <inttypes.h>

#include "video_out.h"
#include "mpeg2.h"
#include "mpeg2_internal.h"
#include "xineutils.h"
#include "xine_internal.h"
#include "events.h"

/*
#define LOG_PAN_SCAN
*/

/*
#define LOG
*/

#define BUFFER_SIZE (224 * 1024)

mpeg2_config_t config;

static void process_userdata(mpeg2dec_t *mpeg2dec, uint8_t *buffer);

void mpeg2_init (mpeg2dec_t * mpeg2dec, 
		 vo_instance_t * output)
{
  static int do_init = 1;

    if (do_init) {
	do_init = 0;
	config.flags = xine_mm_accel();
	idct_init ();
	motion_comp_init ();
    }

    if( !mpeg2dec->chunk_buffer )
      mpeg2dec->chunk_buffer = xine_xmalloc_aligned (16, BUFFER_SIZE + 4, 
						     (void**)&mpeg2dec->chunk_base);
    if( !mpeg2dec->picture )
      mpeg2dec->picture = xine_xmalloc_aligned (16, sizeof (picture_t),
						(void**)&mpeg2dec->picture_base);

    mpeg2dec->shift = 0xffffff00;
    mpeg2dec->is_sequence_needed = 1;
    mpeg2dec->frames_to_drop = 0;
    mpeg2dec->drop_frame = 0;
    mpeg2dec->in_slice = 0;
    mpeg2dec->output = output;
    mpeg2dec->chunk_ptr = mpeg2dec->chunk_buffer;
    mpeg2dec->code = 0xb4;
    mpeg2dec->seek_mode = 0;

    memset (mpeg2dec->picture, 0, sizeof (picture_t));

    /* initialize supstructures */
    header_state_init (mpeg2dec->picture);
}

static inline void get_frame_duration (mpeg2dec_t * mpeg2dec, vo_frame_t *frame)
{
  switch (mpeg2dec->picture->frame_rate_code) {
  case 1: /* 23.976 fps */
    frame->duration      = 3913;
    break;
  case 2: /* 24 fps */
    frame->duration      = 3750;
    break;
  case 3: /* 25 fps */
    frame->duration      = 3600;
    break;
  case 4: /* 29.97 fps */
    frame->duration      = 3003;
    break;
  case 5: /* 30 fps */
    frame->duration      = 3000;
    break;
  case 6: /* 50 fps */
    frame->duration      = 1800;
    break;
  case 7: /* 59.94 fps */
    frame->duration      = 1525;
    break;
  case 8: /* 60 fps */
    frame->duration      = 1509;
    break;
  default:
       /* printf ("invalid/unknown frame rate code : %d \n",
               frame->frame_rate_code); */
    frame->duration      = 3000;
  }
  
  if( mpeg2dec->picture->repeat_first_field ) {
    if( !mpeg2dec->picture->progressive_sequence &&
         mpeg2dec->picture->progressive_frame ) {
      /* decoder should output 3 fields, so adjust duration to
         count on this extra field time */
      frame->duration += frame->duration/2;     
    } else if( mpeg2dec->picture->progressive_sequence ) {
      /* for progressive sequences the output should repeat the
         frame 1 or 2 times depending on top_field_first flag. */
      frame->duration *= (mpeg2dec->picture->top_field_first)?3:2;
    }
  }
  
  /* printf("mpeg2dec: rff=%u\n",frame->repeat_first_field); */
} 

static inline int parse_chunk (mpeg2dec_t * mpeg2dec, int code,
			       uint8_t * buffer)
{
    picture_t * picture;
    int is_frame_done;
/*
    if (code >= 0xb0) { 
      printf ("libmpeg2: parse_chunk 0x%02x\n", code);   
    }
*/
    /* wait for sequence_header_code */
    if (mpeg2dec->is_sequence_needed) {
      if (code != 0xb3) {
        /* printf ("libmpeg2: waiting for sequence header\n");  */
	mpeg2dec->pts = 0;
	return 0;
      }
    } else if (mpeg2dec->is_frame_needed && (code != 0x00)) {
      /* printf ("libmpeg2: waiting for frame start\n");  */
      mpeg2dec->pts = 0;
      return 0;
    }

    stats_header (code, buffer);

    picture = mpeg2dec->picture;
    is_frame_done = mpeg2dec->in_slice && ((!code) || (code >= 0xb0));

    if (is_frame_done) {
	mpeg2dec->in_slice = 0;

	if ( picture->current_frame && ((picture->picture_structure == FRAME_PICTURE) ||
	     (picture->second_field)) ) {

	  if (!mpeg2dec->drop_frame)
	    picture->current_frame->bad_frame = 0;
#ifdef LOG
	    printf ("libmpeg2: drawing frame %d type %s: %s\n",
		    picture->current_frame->id,
		    picture->picture_coding_type == I_TYPE ? "I" :
		    picture->picture_coding_type == P_TYPE ? "P" : "B",
		    picture->current_frame->bad_frame ? "BAD" : "good");
#endif 
	    if (picture->picture_coding_type == B_TYPE) {

	      /* hack against wrong mpeg1 pts */
	      if (picture->mpeg1) 
		picture->current_frame->pts = 0;

	      if( !picture->current_frame->drawn )
	        mpeg2dec->frames_to_drop = picture->current_frame->draw (picture->current_frame);
	      picture->current_frame->drawn = 1;
	      
	      if( picture->current_frame == picture->backward_reference_frame )
	        picture->backward_reference_frame = NULL;
	        
	      picture->current_frame->free (picture->current_frame);
	      
	      picture->current_frame = NULL;
	      picture->throwaway_frame = NULL;
	    } else if (picture->forward_reference_frame && !picture->forward_reference_frame->drawn) {
              get_frame_duration(mpeg2dec, picture->forward_reference_frame);
	      mpeg2dec->frames_to_drop = picture->forward_reference_frame->draw (picture->forward_reference_frame);
	      picture->forward_reference_frame->drawn = 1;
	    }

#ifdef ARCH_X86
	    if (config.flags & MM_ACCEL_X86_MMX)
		emms ();
#endif
	}
    }

    switch (code) {
    case 0x00:	/* picture_start_code */
	if (header_process_picture_header (picture, buffer)) {
	    fprintf (stderr, "bad picture header\n");
	    exit (1);
	}

	mpeg2dec->is_frame_needed=0;

	if (!picture->second_field) {
	  /* find out if we want to skip this frame */
	  mpeg2dec->drop_frame = 0;
	  
	  /* picture->skip_non_intra_dct = (mpeg2dec->frames_to_drop>0) ; */
	  
	  switch (picture->picture_coding_type) {
	  case B_TYPE:
	    
	    if (mpeg2dec->frames_to_drop>1) {
#ifdef LOG
	      printf ("libmpeg2: dropping b-frame because frames_to_drop==%d\n",
		      mpeg2dec->frames_to_drop);
#endif
	      mpeg2dec->drop_frame = 1;
	    } else if (!picture->forward_reference_frame || picture->forward_reference_frame->bad_frame 
		       || !picture->backward_reference_frame || picture->backward_reference_frame->bad_frame) {
#ifdef LOG
	      printf ("libmpeg2: dropping b-frame because ref is bad (");
	      if (picture->forward_reference_frame)
		printf ("fw ref frame %d, bad %d;", picture->forward_reference_frame->id,
			picture->forward_reference_frame->bad_frame);
	      else
		printf ("fw ref frame not there;");
	      if (picture->backward_reference_frame)
		printf ("bw ref frame %d, bad %d)\n", picture->backward_reference_frame->id,
			picture->backward_reference_frame->bad_frame);
	      else
		printf ("fw ref frame not there)\n");
#endif
	      mpeg2dec->drop_frame = 1;
	    }
	    break;
	    
	  case P_TYPE:
	    
	    if (mpeg2dec->frames_to_drop>2) {
	      mpeg2dec->drop_frame = 1;
#ifdef LOG
	      printf ("libmpeg2: dropping p-frame because frames_to_drop==%d\n",
		      mpeg2dec->frames_to_drop);
#endif
	    } else if (!picture->backward_reference_frame || picture->backward_reference_frame->bad_frame) {
	      mpeg2dec->drop_frame = 1;
#ifdef LOG
	      if (picture->backward_reference_frame->bad_frame)
		printf ("libmpeg2: dropping p-frame because ref %d is bad\n", picture->backward_reference_frame->id);
	      else
		printf ("libmpeg2: dropping p-frame because no ref frame\n");
#endif
	    }
	    break;
	    
	  case I_TYPE:
#ifdef LOG
	    printf ("libmpeg2: I-Frame\n");
#endif
	    /* for the sake of dvd menus, never drop i-frames
	    if (mpeg2dec->frames_to_drop>4) {
	      mpeg2dec->drop_frame = 1;
	    }
	    */
	    break;
	  }
	}

	break;

    case 0xb2: /* user data code */
        process_userdata(mpeg2dec, buffer);
        break;

    case 0xb3:	/* sequence_header_code */
	if (header_process_sequence_header (picture, buffer)) {
  	    printf ("libmpeg2: bad sequence header\n");
	    /* exit (1); */
	} else if (mpeg2dec->is_sequence_needed 
	    || (picture->frame_width != picture->coded_picture_width)
	    || (picture->frame_height != picture->coded_picture_height)) {
            xine_frame_change_event_t notify_event;

	    xine_log (mpeg2dec->xine, XINE_LOG_FORMAT,
		      "libmpeg2: frame size is %d x %d, ratio is %d\n",
		      picture->frame_width, picture->frame_height,
		      picture->frame_rate_code);
            
	    printf ("libmpeg2: frame size has changed to from %d x %d to %d x %d\n",
		    picture->frame_width, picture->frame_height,
		    picture->coded_picture_width, picture->coded_picture_height);
	    
	    notify_event.event.type = XINE_EVENT_FRAME_CHANGE;
	    notify_event.width = picture->coded_picture_width;
	    notify_event.height = picture->coded_picture_height;
	    xine_send_event(mpeg2dec->xine, &notify_event.event);

	    if (picture->forward_reference_frame) 
	      picture->forward_reference_frame->free (picture->forward_reference_frame);
	  
	    if (picture->backward_reference_frame) 
	      picture->backward_reference_frame->free (picture->backward_reference_frame);

	    printf ("libmpeg2: old frames freed.\n"); 

	    mpeg2dec->is_sequence_needed = 0;
	    picture->forward_reference_frame = NULL;
	    picture->backward_reference_frame = NULL;

	    picture->frame_width = picture->coded_picture_width;
	    picture->frame_height = picture->coded_picture_height;
	}
	break;

    case 0xb5:	/* extension_start_code */
	if (header_process_extension (picture, buffer)) {
	  printf ("libmpeg2: bad extension\n");
	  exit (1);
	}
	break;

    case 0xb7:	/* sequence end code */
#ifdef LOG_PAN_SCAN
      printf ("libmpeg2: sequence end code not handled\n");
#endif
    case 0xb8:	/* group of pictures start code */
	if (header_process_group_of_pictures (picture, buffer)) {
	  printf ("libmpeg2: bad group of pictures\n");
	  exit (1);
	}
    default:
	if (code >= 0xb9)
	  printf ("libmpeg2: stream not demultiplexed ?\n");

	if (code >= 0xb0)
	    break;

	if (!(mpeg2dec->in_slice)) {
	    mpeg2dec->in_slice = 1;

	    if (picture->second_field) {
	      if (picture->current_frame)
		picture->current_frame->field(picture->current_frame, 
					      picture->picture_structure);
	      else
		mpeg2dec->drop_frame = 1;		
	    } else {
		if (picture->picture_coding_type == B_TYPE)
		    picture->throwaway_frame = picture->current_frame =
		        mpeg2dec->output->get_frame (mpeg2dec->output,
						     picture->coded_picture_width,
						     picture->coded_picture_height,
						     picture->aspect_ratio_information,
						     IMGFMT_YV12,
						     picture->picture_structure);
		else {
		    picture->current_frame =
		        mpeg2dec->output->get_frame (mpeg2dec->output,
						     picture->coded_picture_width,
						     picture->coded_picture_height,
						     picture->aspect_ratio_information,
						     IMGFMT_YV12,
						     (VO_PREDICTION_FLAG | picture->picture_structure));
		    if (picture->forward_reference_frame)
		      picture->forward_reference_frame->free (picture->forward_reference_frame);

		    picture->forward_reference_frame =
			picture->backward_reference_frame;
		    picture->backward_reference_frame = picture->current_frame;
		}
		picture->current_frame->bad_frame          = 1;
		picture->current_frame->drawn              = 0;
		picture->current_frame->pts                = mpeg2dec->pts;
                picture->current_frame->top_field_first    = picture->top_field_first;
                picture->current_frame->repeat_first_field = picture->repeat_first_field;

		get_frame_duration(mpeg2dec, picture->current_frame);

#ifdef LOG
		printf ("libmpeg2: decoding frame %d, type %s\n",
			picture->current_frame->id, picture->picture_coding_type == I_TYPE ? "I" :
			picture->picture_coding_type == P_TYPE ? "P" : "B");
#endif
		mpeg2dec->pts = 0;
	    }
	}

	if (!(mpeg2dec->drop_frame)) {
	  if (slice_process (picture, code, buffer) == 1) {
	    picture->current_frame->bad_frame = 0;
	  }

#ifdef ARCH_X86
	    if (config.flags & MM_ACCEL_X86_MMX)
		emms ();
#endif
	}
    }

    /* printf ("libmpeg2: parse_chunk %d completed\n", code);  */
    return is_frame_done;
}

static inline uint8_t * copy_chunk (mpeg2dec_t * mpeg2dec,
				    uint8_t * current, uint8_t * end)
{
    uint32_t shift;
    uint8_t * chunk_ptr;
    uint8_t * limit;
    uint8_t byte;

    shift = mpeg2dec->shift;
    chunk_ptr = mpeg2dec->chunk_ptr;
    limit = current + (mpeg2dec->chunk_buffer + BUFFER_SIZE - chunk_ptr);
    if (limit > end)
	limit = end;

    /* 
    printf ("libmpeg2: copy chunk current %08x\n", current );
    printf ("libmpeg2: copy chunk end     %08x\n", end); fflush(stdout);
    */

    while (1) {

	byte = *current++;
	if (shift != 0x00000100) {
	    shift = (shift | byte) << 8;
	    *chunk_ptr++ = byte;
	    if (current < limit)
		continue;
	    if (current == end) {
		mpeg2dec->chunk_ptr = chunk_ptr;
		mpeg2dec->shift = shift;
		return NULL;
	    } else {
		/* we filled the chunk buffer without finding a start code */
		mpeg2dec->code = 0xb4;	/* sequence_error_code */
		mpeg2dec->chunk_ptr = mpeg2dec->chunk_buffer;
		return current;
	    }
	}
	mpeg2dec->code = byte;
	mpeg2dec->chunk_ptr = mpeg2dec->chunk_buffer;
	mpeg2dec->shift = 0xffffff00;
	return current;
    }
}

int mpeg2_decode_data (mpeg2dec_t * mpeg2dec, uint8_t * current, uint8_t * end,
		       uint64_t pts)
{
    int ret;
    uint8_t code;

    ret = 0;
    if (mpeg2dec->seek_mode) {
      mpeg2dec->chunk_ptr = mpeg2dec->chunk_buffer;
      mpeg2dec->code = 0xb4;
      mpeg2dec->seek_mode = 0;
      mpeg2dec->shift = 0xffffff00;
      mpeg2dec->is_frame_needed = 1;
    }

    if (pts)
      mpeg2dec->pts = pts;

    while (current != end) {
	code = mpeg2dec->code;
	current = copy_chunk (mpeg2dec, current, end);
	if (current == NULL)
	    return ret;
	ret += parse_chunk (mpeg2dec, code, mpeg2dec->chunk_buffer);
    }

    return ret;
}

void mpeg2_flush (mpeg2dec_t * mpeg2dec) {

  picture_t *picture = mpeg2dec->picture;

  if (!picture)
    return;

  if (picture->backward_reference_frame && !picture->backward_reference_frame->drawn
      && !picture->backward_reference_frame->bad_frame) {

    vo_frame_t *img;
    
    printf ("libmpeg2: blasting out backward reference frame %d on flush\n",
	    picture->backward_reference_frame->id);
    
    picture->backward_reference_frame->drawn = 1;
    
    /* output a copy instead of the frame used by decoder */
    img = picture->backward_reference_frame->instance->duplicate_frame(picture->backward_reference_frame->instance, 
								       picture->backward_reference_frame);
    img->pts = 0;
    img->bad_frame = 0;
    img->drawn = 1; 
    
    img->draw(img);
    img->free(img);

  }

}

void mpeg2_close (mpeg2dec_t * mpeg2dec)
{
    picture_t *picture = mpeg2dec->picture;

    /*
    {
	static uint8_t finalizer[] = {0,0,1,0xb4};
	mpeg2_decode_data (mpeg2dec, finalizer, finalizer+4, 0);
    }
    */

    /* 
      dont remove any picture->*->free() below. doing so will cause buffer 
      leak, and we only have about 15 of them.
    */ 
    if (picture->forward_reference_frame) {
      picture->forward_reference_frame->free (picture->forward_reference_frame);
    }
    
    if (picture->throwaway_frame) {
      printf ("libmpeg2: blasting out throwaway frame on close\n");
      picture->throwaway_frame->pts = 0;
      get_frame_duration(mpeg2dec, picture->throwaway_frame);
      picture->throwaway_frame->draw (picture->throwaway_frame);
      picture->throwaway_frame->free (picture->throwaway_frame);
    }

    if (picture->backward_reference_frame) {
      printf ("libmpeg2: blasting out backward reference frame on close\n");
      picture->backward_reference_frame->pts = 0;
      get_frame_duration(mpeg2dec, picture->backward_reference_frame);
      if( !picture->backward_reference_frame->drawn)
        picture->backward_reference_frame->draw (picture->backward_reference_frame);
      picture->backward_reference_frame->free (picture->backward_reference_frame);
    }

    if ( mpeg2dec->chunk_buffer ) {
      free (mpeg2dec->chunk_base);
      mpeg2dec->chunk_buffer = NULL;
    }
    
    if ( mpeg2dec->picture ) {
      free (mpeg2dec->picture_base);
      mpeg2dec->picture = NULL;
    }
}

void mpeg2_find_sequence_header (mpeg2dec_t * mpeg2dec,
				 uint8_t * current, uint8_t * end){

  uint8_t code;
  picture_t *picture = mpeg2dec->picture;

  mpeg2dec->seek_mode = 1;

  while (current != end) {
    code = mpeg2dec->code;
    current = copy_chunk (mpeg2dec, current, end);
    if (current == NULL)
      return ;

    /* printf ("looking for sequence header... %02x\n", code);  */

    stats_header (code, mpeg2dec->chunk_buffer);

    if (code == 0xb3) {	/* sequence_header_code */

      if (header_process_sequence_header (picture, mpeg2dec->chunk_buffer)) {
	printf ("libmpeg2: bad sequence header\n");
	continue;
      }
	  
      if (mpeg2dec->is_sequence_needed) {
        xine_frame_change_event_t notify_event;
	
	mpeg2dec->is_sequence_needed = 0;
	picture->frame_width  = picture->coded_picture_width;
	picture->frame_height = picture->coded_picture_height;

	xine_log (mpeg2dec->xine, XINE_LOG_FORMAT,
		  "libmpeg2: frame size is %d x %d\n",
		  picture->frame_width, picture->frame_height);
            
	printf ("libmpeg2: frame size %d x %d\n",
		picture->frame_width, picture->frame_height);
            
	notify_event.event.type = XINE_EVENT_FRAME_CHANGE;
	notify_event.width = picture->coded_picture_width;
	notify_event.height = picture->coded_picture_height;
	xine_send_event(mpeg2dec->xine, &notify_event.event);
      }
    } else if (code == 0xb5) {	/* extension_start_code */
      if (header_process_extension (picture, mpeg2dec->chunk_buffer)) {
	printf ("libmpeg2: bad extension\n");
	continue ;
      }
    }
  }
}

/* Find the end of the userdata field in an MPEG-2 stream */
static uint8_t *find_end(uint8_t *buffer)
{
  uint8_t *current = buffer;
  while(1) {
    if (current[0] == 0 && current[1] == 0 && current[2] == 1)
      break;
    current++;
  }
  return current;
}

static void process_userdata(mpeg2dec_t *mpeg2dec, uint8_t *buffer)
{
  /* check if user data denotes closed captions */
  if (buffer[0] == 'C' && buffer[1] == 'C') {
    xine_closed_caption_event_t event;
    uint8_t *end = find_end(buffer);

    event.event.type = XINE_EVENT_CLOSED_CAPTION;
    event.buffer = &buffer[2];
    event.buf_len = end - &buffer[2];
    event.pts = mpeg2dec->pts;
    xine_send_event(mpeg2dec->xine, &event.event);
  }
}
