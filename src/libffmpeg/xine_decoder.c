/*
 * Copyright (C) 2001-2003 the xine project
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: xine_decoder.c,v 1.151 2004/01/16 16:03:54 andruil Exp $
 *
 * xine decoder plugin using ffmpeg
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <assert.h>

#define LOG_MODULE "ffmpeg_decoder"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "xine_internal.h"
#include "video_out.h"
#include "buffer.h"
#include "metronom.h"
#include "xineutils.h"

#ifdef _MSC_VER
/* ffmpeg has own definitions of those types */
#  undef int8_t
#  undef uint8_t
#  undef int16_t
#  undef uint16_t
#  undef int32_t
#  undef uint32_t
#  undef int64_t
#  undef uint64_t
#endif

#include "libavcodec/avcodec.h"
#include "libavcodec/dsputil.h"
#include "libavcodec/libpostproc/postprocess.h"

#ifdef _MSC_VER
#  undef malloc
#  undef free
#  undef realloc
#endif

#define ENABLE_DIRECT_RENDERING

#define SLICE_BUFFER_SIZE (1194 * 1024)
#define RV10_CHUNK_TAB_SIZE 128         /* from libreal/xine_decoder.c */
#define abs_float(x) ( ((x)<0) ? -(x) : (x) )

typedef struct ff_video_decoder_s ff_video_decoder_t;

typedef struct ff_video_class_s {
  video_decoder_class_t   decoder_class;

  ff_video_decoder_t     *ip;
  xine_t                 *xine;
} ff_video_class_t;

struct ff_video_decoder_s {
  video_decoder_t   video_decoder;

  ff_video_class_t *class;

  xine_stream_t    *stream;
  int               video_step;
  int               decoder_ok;

  xine_bmiheader    bih;
  unsigned char     *buf;
  int               bufsize;
  int               size;
  int               skipframes;

  AVFrame           *av_frame;
  AVCodecContext    *context;
  AVCodec           *codec;
  
  int                pp_available;
  int                pp_quality;
  int                pp_quality_changed;
  int                pp_flags;
  pp_context_t      *pp_context;
  pp_mode_t         *pp_mode;

  /* mpeg sequence header parsing, stolen from libmpeg2 */

  uint32_t          shift;
  uint8_t          *chunk_buffer;
  uint8_t          *chunk_ptr;
  uint8_t           code;

  int               is_continous;

  double            aspect_ratio;
  int               frame_flags;
  
  int               output_format;
  yuv_planes_t      yuv;
  AVPaletteControl  palette_control;
};

typedef struct {
  audio_decoder_class_t   decoder_class;
} ff_audio_class_t;

typedef struct ff_audio_decoder_s {
  audio_decoder_t   audio_decoder;

  xine_stream_t    *stream;

  int               output_open;
  int               audio_channels;
  int               audio_bits;
  int               audio_sample_rate;

  unsigned char    *buf;
  int               bufsize;
  int               size;

  AVCodecContext    *context;
  AVCodec           *codec;
  
  char              *decode_buffer;
  int               decoder_ok;

} ff_audio_decoder_t;


static pthread_once_t once_control = PTHREAD_ONCE_INIT;


#define VIDEOBUFSIZE 128*1024
#define AUDIOBUFSIZE VIDEOBUFSIZE

#ifdef ENABLE_DIRECT_RENDERING

/* called from ffmpeg to do direct rendering method 1 */
static int get_buffer(AVCodecContext *context, AVFrame *av_frame){
  ff_video_decoder_t * this = (ff_video_decoder_t *)context->opaque;
  vo_frame_t *img;
  int align, width, height;
        
  align=15;
    
  width  = (context->width +align)&~align;
  height = (context->height+align)&~align;

  if( (this->context->pix_fmt != PIX_FMT_YUV420P) ||
      (width != context->width) || (height != context->height) ) {
    xprintf(this->stream->xine, XINE_VERBOSITY_LOG, _("ffmpeg: unsupported frame format, DR1 disabled.\n"));

    this->context->get_buffer = avcodec_default_get_buffer;
    this->context->release_buffer = avcodec_default_release_buffer;
    return avcodec_default_get_buffer(context, av_frame);
  }

  img = this->stream->video_out->get_frame (this->stream->video_out,
					    width,
					    height,
					    this->aspect_ratio, 
					    this->output_format,
					    VO_BOTH_FIELDS|this->frame_flags);

  av_frame->opaque = img;

  av_frame->data[0]= img->base[0];
  av_frame->data[1]= img->base[1];
  av_frame->data[2]= img->base[2];

  av_frame->linesize[0] = img->pitches[0];
  av_frame->linesize[1] = img->pitches[1];
  av_frame->linesize[2] = img->pitches[2];

  /* We should really keep track of the ages of xine frames (see
   * avcodec_default_get_buffer in libavcodec/utils.c)
   * For the moment tell ffmpeg that every frame is new (age = bignumber) */
  av_frame->age = 256*256*256*64;

  av_frame->type= FF_BUFFER_TYPE_USER;

  return 0;
}

static void release_buffer(struct AVCodecContext *context, AVFrame *av_frame){
  vo_frame_t *img = (vo_frame_t *)av_frame->opaque;
    
  assert(av_frame->type == FF_BUFFER_TYPE_USER);
  assert(av_frame->opaque);  
  
  av_frame->data[0]= NULL;
  av_frame->data[1]= NULL;
  av_frame->data[2]= NULL;
  
  img->free(img);

  av_frame->opaque = NULL;
}

#endif

static void init_video_codec (ff_video_decoder_t *this, xine_bmiheader *bih) {

  /* force (width % 8 == 0), otherwise there will be 
   * display problems with Xv. 
   */ 
  this->bih.biWidth = (this->bih.biWidth + 1) & (~1);

  this->av_frame = avcodec_alloc_frame();
  this->context = avcodec_alloc_context();
  this->context->opaque = this;
  this->context->width = this->bih.biWidth;
  this->context->height = this->bih.biHeight;
  this->context->stream_codec_tag = this->context->codec_tag = 
    _x_stream_info_get(this->stream, XINE_STREAM_INFO_VIDEO_FOURCC);

  /* some decoders (eg. dv) do not know the pix_fmt until they decode the
   * first frame. setting to -1 avoid enabling DR1 for them.
   */
  this->context->pix_fmt = -1;

  this->context->palctrl = &this->palette_control;

  if( bih && bih->biSize > sizeof(xine_bmiheader) ) {
    this->context->extradata_size = bih->biSize - sizeof(xine_bmiheader);
    this->context->extradata = malloc(this->context->extradata_size);
    memcpy( this->context->extradata, 
            (uint8_t *)bih + sizeof(xine_bmiheader),
            this->context->extradata_size ); 
  }

  if(bih)
    this->context->bits_per_sample = bih->biBitCount;
    
  if(this->codec->id == CODEC_ID_RV10) {
    this->context->sub_id = this->bih.biCompression;
    this->context->slice_offset = malloc(sizeof(int) * RV10_CHUNK_TAB_SIZE);
  }
  
  /* Some codecs (eg rv10) copy flags in init so it's necessary to set
   * this flag here in case we are going to use direct rendering */
  if(this->codec->capabilities & CODEC_CAP_DR1)
    this->context->flags |= CODEC_FLAG_EMU_EDGE;

  if (avcodec_open (this->context, this->codec) < 0) {
    xprintf (this->stream->xine, XINE_VERBOSITY_LOG, _("ffmpeg: couldn't open decoder\n"));
    free(this->context);
    this->context = NULL;
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_HANDLED, 0);
    return;
  }

  this->decoder_ok = 1;

  _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_WIDTH,    this->context->width);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_HEIGHT,   this->context->height);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_FRAME_DURATION, this->video_step);

  this->stream->video_out->open (this->stream->video_out, this->stream);

  if (this->buf)
    free (this->buf);
    
  this->buf = xine_xmalloc (VIDEOBUFSIZE);
  this->bufsize = VIDEOBUFSIZE;
  
  this->skipframes = 0;
  
  if((this->context->pix_fmt == PIX_FMT_RGBA32) ||
     (this->context->pix_fmt == PIX_FMT_RGB565) ||
     (this->context->pix_fmt == PIX_FMT_RGB555) ||
     (this->context->pix_fmt == PIX_FMT_PAL8)) {
    this->output_format = XINE_IMGFMT_YUY2;
    init_yuv_planes(&this->yuv, this->context->width, this->context->height);
  } else {
    this->output_format = XINE_IMGFMT_YV12;
#ifdef ENABLE_DIRECT_RENDERING
    if( this->context->pix_fmt == PIX_FMT_YUV420P &&
        this->codec->capabilities & CODEC_CAP_DR1 ) {
      this->context->get_buffer = get_buffer;
      this->context->release_buffer = release_buffer;
      xprintf(this->stream->xine, XINE_VERBOSITY_LOG, _("ffmpeg: direct rendering enabled\n"));
    }
#endif
  }
}

static void pp_quality_cb(void *user_data, xine_cfg_entry_t *entry) {
  ff_video_class_t   *class = (ff_video_class_t *) user_data;
  
  if(class->ip) {
    ff_video_decoder_t *this  = class->ip;
    
    if(this->pp_available) {
      this->pp_quality = entry->num_value;
      this->pp_quality_changed = 1;
    }
  }
}

static void pp_change_quality (ff_video_decoder_t *this) {
  if(this->pp_available && this->pp_quality) {
    if(!this->pp_context)
      this->pp_context = pp_get_context(this->context->width, this->context->height,
                                        this->pp_flags);
    if(this->pp_mode)
      pp_free_mode(this->pp_mode);
      
    this->pp_mode = pp_get_mode_by_name_and_quality("hb:a,vb:a,dr:a", 
                                                    this->pp_quality);
  } else {
    if(this->pp_mode) {
      pp_free_mode(this->pp_mode);
      this->pp_mode = NULL;
    }
    
    if(this->pp_context) {
      pp_free_context(this->pp_context);
      this->pp_context = NULL;
    }
  }
  
  this->pp_quality_changed = 0;
}

static void init_postprocess (ff_video_decoder_t *this) {
  uint32_t cpu_caps;
  xine_cfg_entry_t quality_entry;

  /* Read quality from config */
  if(xine_config_lookup_entry(this->class->xine, "codec.ffmpeg_pp_quality",
                              &quality_entry))
    this->pp_quality = quality_entry.num_value;
  else
    this->pp_quality = 0;
  
  /* Allow post processing on mpeg-4 (based) codecs */
  switch(this->codec->id) {
    case CODEC_ID_MPEG4:
    case CODEC_ID_MSMPEG4V1:
    case CODEC_ID_MSMPEG4V2:
    case CODEC_ID_MSMPEG4V3:
    case CODEC_ID_WMV1:
    case CODEC_ID_WMV2:
      this->pp_available = 1;
      break;
    default:
      this->pp_available = 0;
      break;
  }
  
  /* Detect what cpu accel we have */
  cpu_caps = xine_mm_accel();
  this->pp_flags = PP_FORMAT_420;
 
  if(cpu_caps & MM_ACCEL_X86_MMX)
    this->pp_flags |= PP_CPU_CAPS_MMX;
    
  if(cpu_caps & MM_ACCEL_X86_MMXEXT)
    this->pp_flags |= PP_CPU_CAPS_MMX2;
  
  if(cpu_caps & MM_ACCEL_X86_3DNOW)  
    this->pp_flags |= PP_CPU_CAPS_3DNOW;
   
  /* Set level */
  pp_change_quality(this);    
}

static void find_sequence_header (ff_video_decoder_t *this,
				  uint8_t * current, uint8_t * end){

  uint8_t code;

  if (this->decoder_ok)
    return;

  while (current != end) {

    uint32_t shift;
    uint8_t *chunk_ptr;
    uint8_t *limit;
    uint8_t  byte;
    
    code = this->code;
    
    /* copy chunk */
    
    shift     = this->shift;
    chunk_ptr = this->chunk_ptr;
    limit     = current + (this->chunk_buffer + SLICE_BUFFER_SIZE - chunk_ptr);
    if (limit > end)
      limit = end;
    
    while (1) {
      
      byte = *current++;
      if (shift != 0x00000100) {
	shift = (shift | byte) << 8;
	*chunk_ptr++ = byte;
	if (current < limit)
	  continue;
	if (current == end) {
	  this->chunk_ptr = chunk_ptr;
	  this->shift = shift;
	  current = 0;
	  break;
	} else {
	  /* we filled the chunk buffer without finding a start code */
	  this->code = 0xb4;	/* sequence_error_code */
	  this->chunk_ptr = this->chunk_buffer;
	  break;
	}
      }
      this->code = byte;
      this->chunk_ptr = this->chunk_buffer;
      this->shift = 0xffffff00;
      break;
    }

    if (current == NULL)
      return ;

    lprintf ("looking for sequence header... %02x\n", code);
  
    /* mpeg2_stats (code, this->chunk_buffer); */
    
    if (code == 0xb3) {	/* sequence_header_code */

      int width, height, frame_rate_code;

      lprintf ("found sequence header !\n");

      height = (this->chunk_buffer[0] << 16) | (this->chunk_buffer[1] << 8) 
	| this->chunk_buffer[2];

      width = ((height >> 12) + 15) & ~15;
      height = ((height & 0xfff) + 15) & ~15;

      this->bih.biWidth  = width;
      this->bih.biHeight = height;

      frame_rate_code = this->chunk_buffer[3] & 15;

      switch (frame_rate_code) {
      case 1: /* 23.976 fps */
	this->video_step      = 3754;  /* actually it's 3753.75 */
	break;
      case 2: /* 24 fps */
	this->video_step      = 3750;
	break;
      case 3: /* 25 fps */
	this->video_step      = 3600;
	break;
      case 4: /* 29.97 fps */
	this->video_step      = 3003;
	break;
      case 5: /* 30 fps */
	this->video_step      = 3000;
	break;
      case 6: /* 50 fps */
	this->video_step      = 1800;
	break;
      case 7: /* 59.94 fps */
	this->video_step      = 1502;  /* actually it's 1501.5 */
	break;
      case 8: /* 60 fps */
	this->video_step      = 1500;
	break;
      default:
	xprintf (this->stream->xine, XINE_VERBOSITY_LOG, _("ffmpeg: invalid/unknown frame rate code : %d \n"),
		frame_rate_code); 
	this->video_step      = 0;
      }

      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
        "mpeg-1 (ffmpeg)");

      /*
       * init codec
       */

      this->codec = avcodec_find_decoder (CODEC_ID_MPEG1VIDEO); 
      if (!this->codec) {
	xprintf (this->stream->xine, XINE_VERBOSITY_LOG, _("avcodec_find_decoder (CODEC_ID_MPEG1VIDEO) failed.\n"));
	abort();
      }

      this->is_continous = 1;
      init_video_codec (this, NULL);
    }
  }
}

static void ff_convert_frame(ff_video_decoder_t *this, vo_frame_t *img) {
  int         y;
  uint8_t    *dy, *du, *dv, *sy, *su, *sv;

  dy = img->base[0];
  du = img->base[1];
  dv = img->base[2];
  sy = this->av_frame->data[0];
  su = this->av_frame->data[1];
  sv = this->av_frame->data[2];

  if (this->context->pix_fmt == PIX_FMT_YUV410P) {

    yuv9_to_yv12(
     /* Y */
      this->av_frame->data[0],
      this->av_frame->linesize[0],
      img->base[0],
      img->pitches[0],
     /* U */
      this->av_frame->data[1],
      this->av_frame->linesize[1],
      img->base[1],
      img->pitches[1],
     /* V */
      this->av_frame->data[2],
      this->av_frame->linesize[2],
      img->base[2],
      img->pitches[2],
     /* width x height */
      this->context->width,
      this->context->height);

  } else if (this->context->pix_fmt == PIX_FMT_YUV411P) {

    yuv411_to_yv12(
     /* Y */
      this->av_frame->data[0],
      this->av_frame->linesize[0],
      img->base[0],
      img->pitches[0],
     /* U */
      this->av_frame->data[1],
      this->av_frame->linesize[1],
      img->base[1],
      img->pitches[1],
     /* V */
      this->av_frame->data[2],
      this->av_frame->linesize[2],
      img->base[2],
      img->pitches[2],
     /* width x height */
      this->context->width,
      this->context->height);

  } else if (this->context->pix_fmt == PIX_FMT_RGBA32) {
          
    int x, plane_ptr = 0;
    uint8_t *src;
            
    for(y = 0; y < this->context->height; y++) {
      src = sy;
      for(x = 0; x < this->context->width; x++) {
        uint8_t r, g, b;
              
        /* These probably need to be switched for big endian */
        b = *src; src++;
        g = *src; src++;
        r = *src; src += 2;

        this->yuv.y[plane_ptr] = COMPUTE_Y(r, g, b);
        this->yuv.u[plane_ptr] = COMPUTE_U(r, g, b);
        this->yuv.v[plane_ptr] = COMPUTE_V(r, g, b);
        plane_ptr++;
      }
      sy += this->av_frame->linesize[0];
    }
            
    yuv444_to_yuy2(&this->yuv, img->base[0], img->pitches[0]);
          
  } else if (this->context->pix_fmt == PIX_FMT_RGB565) {

    int x, plane_ptr = 0;
    uint8_t *src;
    uint16_t pixel16;

    for(y = 0; y < this->context->height; y++) {
      src = sy;
      for(x = 0; x < this->context->width; x++) {
        uint8_t r, g, b;
              
        /* a 16-bit RGB565 pixel is supposed to be stored in native-endian
         * byte order; the following should be endian-safe */
        pixel16 = *((uint16_t *)src);
        src += 2;
        b = (pixel16 << 3) & 0xFF;
        g = (pixel16 >> 3) & 0xFF;
        r = (pixel16 >> 8) & 0xFF;

        this->yuv.y[plane_ptr] = COMPUTE_Y(r, g, b);
        this->yuv.u[plane_ptr] = COMPUTE_U(r, g, b);
        this->yuv.v[plane_ptr] = COMPUTE_V(r, g, b);
        plane_ptr++;
      }
      sy += this->av_frame->linesize[0];
    }
            
    yuv444_to_yuy2(&this->yuv, img->base[0], img->pitches[0]);
          
  } else if (this->context->pix_fmt == PIX_FMT_RGB555) {
          
    int x, plane_ptr = 0;
    uint8_t *src;
    uint16_t pixel16;
            
    for(y = 0; y < this->context->height; y++) {
      src = sy;
      for(x = 0; x < this->context->width; x++) {
        uint8_t r, g, b;
              
        /* a 16-bit RGB555 pixel is supposed to be stored in native-endian
         * byte order; the following should be endian-safe */
        pixel16 = *((uint16_t *)src);
        src += 2;
        b = (pixel16 << 3) & 0xFF;
        g = (pixel16 >> 2) & 0xFF;
        r = (pixel16 >> 7) & 0xFF;

        this->yuv.y[plane_ptr] = COMPUTE_Y(r, g, b);
        this->yuv.u[plane_ptr] = COMPUTE_U(r, g, b);
        this->yuv.v[plane_ptr] = COMPUTE_V(r, g, b);
        plane_ptr++;
      }
      sy += this->av_frame->linesize[0];
    }
            
    yuv444_to_yuy2(&this->yuv, img->base[0], img->pitches[0]);
          
  } else if (this->context->pix_fmt == PIX_FMT_PAL8) {
          
    int x, plane_ptr = 0;
    uint8_t *src;
    uint8_t pixel;
    uint32_t *palette32 = (uint32_t *)su;  /* palette is in data[1] */
    uint32_t rgb_color;
    uint8_t r, g, b;
    uint8_t y_palette[256];
    uint8_t u_palette[256];
    uint8_t v_palette[256];

    for (x = 0; x < 256; x++) {
      rgb_color = palette32[x];
      b = rgb_color & 0xFF;
      rgb_color >>= 8;
      g = rgb_color & 0xFF;
      rgb_color >>= 8;
      r = rgb_color & 0xFF;
      y_palette[x] = COMPUTE_Y(r, g, b);
      u_palette[x] = COMPUTE_U(r, g, b);
      v_palette[x] = COMPUTE_V(r, g, b);
    }

    for(y = 0; y < this->context->height; y++) {
      src = sy;
      for(x = 0; x < this->context->width; x++) {
        pixel = *src++;

        this->yuv.y[plane_ptr] = y_palette[pixel];
        this->yuv.u[plane_ptr] = u_palette[pixel];
        this->yuv.v[plane_ptr] = v_palette[pixel];
        plane_ptr++;
      }
      sy += this->av_frame->linesize[0];
    }
            
    yuv444_to_yuy2(&this->yuv, img->base[0], img->pitches[0]);
          
  } else {
          
    for (y=0; y<this->context->height; y++) {
      xine_fast_memcpy (dy, sy, this->context->width);
  
      dy += img->pitches[0];
  
      sy += this->av_frame->linesize[0];
    }

    for (y=0; y<(this->context->height/2); y++) {
      
      if (this->context->pix_fmt != PIX_FMT_YUV444P) {
        
        xine_fast_memcpy (du, su, this->context->width/2);
        xine_fast_memcpy (dv, sv, this->context->width/2);
        
      } else {
        
        int x;
        uint8_t *src;
        uint8_t *dst;
      
        /* subsample */
        
        src = su; dst = du;
        for (x=0; x<(this->context->width/2); x++) {
          *dst = *src;
          dst++;
          src += 2;
        }
        src = sv; dst = dv;
        for (x=0; x<(this->context->width/2); x++) {
          *dst = *src;
          dst++;
          src += 2;
        }

      }
  
      du += img->pitches[1];
      dv += img->pitches[2];

      if (this->context->pix_fmt != PIX_FMT_YUV420P) {
        su += 2*this->av_frame->linesize[1];
        sv += 2*this->av_frame->linesize[2];
      } else {
        su += this->av_frame->linesize[1];
        sv += this->av_frame->linesize[2];
      }
    }
  }
}

static void ff_decode_data (video_decoder_t *this_gen, buf_element_t *buf) {
  ff_video_decoder_t *this = (ff_video_decoder_t *) this_gen;
  int codec_type;

  lprintf ("processing packet type = %08x, buf : %p, buf->decoder_flags=%08x\n", 
	   buf->type, buf, buf->decoder_flags);

  codec_type = buf->type & 0xFFFF0000;

  if (buf->decoder_flags & BUF_FLAG_PREVIEW) {

    lprintf ("preview\n");

    if ( (buf->type & 0xFFFF0000) == BUF_VIDEO_MPEG ) {
      find_sequence_header (this, buf->content, buf->content+buf->size);
    }
    return;
  }

  if (buf->decoder_flags & BUF_FLAG_STDHEADER) {

    lprintf ("header\n");

    /* init package containing bih */

    memcpy ( &this->bih, buf->content, sizeof (xine_bmiheader));
    this->video_step = buf->decoder_info[1];

    /* init codec */

    this->codec = NULL;

    switch (codec_type) {
    case BUF_VIDEO_MSMPEG4_V1:
      this->codec = avcodec_find_decoder (CODEC_ID_MSMPEG4V1);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
	"Microsoft MPEG-4 v1 (ffmpeg)");
      break;
    case BUF_VIDEO_MSMPEG4_V2:
      this->codec = avcodec_find_decoder (CODEC_ID_MSMPEG4V2);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
	"Microsoft MPEG-4 v2 (ffmpeg)");
      break;
    case BUF_VIDEO_MSMPEG4_V3:
      this->codec = avcodec_find_decoder (CODEC_ID_MSMPEG4V3);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,   
	"Microsoft MPEG-4 v3 (ffmpeg)");
      break;
    case BUF_VIDEO_WMV7:
      this->codec = avcodec_find_decoder (CODEC_ID_WMV1);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
	"MS Windows Media Video 7 (ffmpeg)");
      break;
    case BUF_VIDEO_WMV8:
      this->codec = avcodec_find_decoder (CODEC_ID_WMV2);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
	"MS Windows Media Video 8 (ffmpeg)");
      break;
    case BUF_VIDEO_MPEG4 :
    case BUF_VIDEO_XVID :
    case BUF_VIDEO_DIVX5 :
      this->codec = avcodec_find_decoder (CODEC_ID_MPEG4);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
	"ISO MPEG-4 (ffmpeg)");
      break;
    case BUF_VIDEO_JPEG:
    case BUF_VIDEO_MJPEG:
      this->codec = avcodec_find_decoder (CODEC_ID_MJPEG);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
	"Motion JPEG (ffmpeg)");
      break;
    case BUF_VIDEO_I263:
      this->codec = avcodec_find_decoder (CODEC_ID_H263I);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
	"ITU H.263 (ffmpeg)");
      break;
    case BUF_VIDEO_H263:
      this->codec = avcodec_find_decoder (CODEC_ID_H263);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
	"H.263 (ffmpeg)");
      break;
    case BUF_VIDEO_RV10:
      this->codec = avcodec_find_decoder (CODEC_ID_RV10);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
	"Real Video 1.0 (ffmpeg)");
      break;
    case BUF_VIDEO_IV31:
      this->codec = avcodec_find_decoder (CODEC_ID_INDEO3);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Indeo Video 3.1 (ffmpeg)");
      break;
    case BUF_VIDEO_IV32:
      this->codec = avcodec_find_decoder (CODEC_ID_INDEO3);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Indeo Video 3.2 (ffmpeg)");
      break;
    case BUF_VIDEO_SORENSON_V1:
      this->codec = avcodec_find_decoder (CODEC_ID_SVQ1);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
	"Sorenson Video 1 (ffmpeg)");
      break;
    case BUF_VIDEO_SORENSON_V3:
      this->codec = avcodec_find_decoder (CODEC_ID_SVQ3);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Sorenson Video 3 (ffmpeg)");
      break;
    case BUF_VIDEO_DV:
      this->codec = avcodec_find_decoder (CODEC_ID_DVVIDEO);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
	"DV (ffmpeg)");
      break;
    case BUF_VIDEO_HUFFYUV:
      this->codec = avcodec_find_decoder (CODEC_ID_HUFFYUV);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"HuffYUV (ffmpeg)");
      break;
    case BUF_VIDEO_VP31:
      this->codec = avcodec_find_decoder (CODEC_ID_VP3);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"On2 VP3.1 (ffmpeg)");
      break;
    case BUF_VIDEO_4XM:
      this->codec = avcodec_find_decoder (CODEC_ID_4XM);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"4X Video (ffmpeg)");
      break;
    case BUF_VIDEO_CINEPAK:
      this->codec = avcodec_find_decoder (CODEC_ID_CINEPAK);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Cinepak (ffmpeg)");
      break;
    case BUF_VIDEO_MSVC:
      this->codec = avcodec_find_decoder (CODEC_ID_MSVIDEO1);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Microsoft Video 1 (ffmpeg)");
      break;
    case BUF_VIDEO_MSRLE:
      this->codec = avcodec_find_decoder (CODEC_ID_MSRLE);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Microsoft RLE (ffmpeg)");
      break;
    case BUF_VIDEO_RPZA:
      this->codec = avcodec_find_decoder (CODEC_ID_RPZA);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Apple Quicktime Video/RPZA (ffmpeg)");
      break;
    case BUF_VIDEO_CYUV:
      this->codec = avcodec_find_decoder (CODEC_ID_CYUV);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Creative YUV (ffmpeg)");
      break;
    case BUF_VIDEO_ROQ:
      this->codec = avcodec_find_decoder (CODEC_ID_ROQ);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Id Software RoQ (ffmpeg)");
      break;
    case BUF_VIDEO_IDCIN:
      this->codec = avcodec_find_decoder (CODEC_ID_IDCIN);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Id Software CIN (ffmpeg)");
      break;
    case BUF_VIDEO_WC3:
      this->codec = avcodec_find_decoder (CODEC_ID_XAN_WC3);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Xan (ffmpeg)");
      break;
    case BUF_VIDEO_VQA:
      this->codec = avcodec_find_decoder (CODEC_ID_WS_VQA);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Westwood Studios VQA (ffmpeg)");
      break;
    case BUF_VIDEO_INTERPLAY:
      this->codec = avcodec_find_decoder (CODEC_ID_INTERPLAY_VIDEO);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC,
	"Interplay MVE (ffmpeg)");
      break;
    default:
      xprintf (this->stream->xine, XINE_VERBOSITY_LOG, _("ffmpeg: unknown video format (buftype: 0x%08X)\n"),
	       buf->type & 0xFFFF0000);
      _x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, 
		       "unknown (ffmpeg)");
    }

    if (!this->codec) {
      xprintf (this->stream->xine, XINE_VERBOSITY_LOG, _("ffmpeg: couldn't find decoder\n"));
      return;
    }

    init_video_codec (this, (xine_bmiheader *)buf->content );
    init_postprocess (this);

  } else if (buf->decoder_flags & BUF_FLAG_SPECIAL) {

    /* take care of all the various types of special buffers */

    if (buf->decoder_info[1] == BUF_SPECIAL_STSD_ATOM) {

      this->context->extradata_size = buf->decoder_info[2];
      this->context->extradata = xine_xmalloc(buf->decoder_info[2]);
      memcpy(this->context->extradata, buf->decoder_info_ptr[2],
        buf->decoder_info[2]);

    } else if (buf->decoder_info[1] == BUF_SPECIAL_PALETTE) {

      int i;
      palette_entry_t *demuxer_palette;
      AVPaletteControl *decoder_palette;

      decoder_palette = (AVPaletteControl *)this->context->palctrl;
      demuxer_palette = (palette_entry_t *)buf->decoder_info_ptr[2];

      for (i = 0; i < buf->decoder_info[2]; i++) {
        decoder_palette->palette[i] = 
          (demuxer_palette[i].r << 16) |
          (demuxer_palette[i].g <<  8) |
          (demuxer_palette[i].b <<  0);
      }
      decoder_palette->palette_changed = 1;
    }

  } else if (this->decoder_ok) {

    if( this->size + buf->size > this->bufsize ) {
      this->bufsize = this->size + 2 * buf->size;
      xprintf(this->stream->xine, XINE_VERBOSITY_LOG, 
	      _("ffmpeg: increasing source buffer to %d to avoid overflow.\n"), this->bufsize);
      this->buf = realloc( this->buf, this->bufsize );
    }
   
    if(!(buf->decoder_flags & BUF_FLAG_FRAME_START)) {
      if(codec_type == BUF_VIDEO_RV10) {
        this->context->slice_offset[this->context->slice_count] = this->size;
        this->context->slice_count++;     
      }
    }

    xine_fast_memcpy (&this->buf[this->size], buf->content, buf->size);
    this->size += buf->size;

    if (buf->decoder_flags & BUF_FLAG_FRAMERATE)
      this->video_step = buf->decoder_info[0];

    if ( (buf->decoder_flags & (BUF_FLAG_FRAME_END))
	  || this->is_continous) {

      vo_frame_t *img;
      int         free_img;
      int         got_picture, len;
      int         offset;

      /* decode video frame(s) */


      /* flag for interlaced streams */
      this->frame_flags = 0;
      /* FIXME: which codecs can be interlaced?
         FIXME: check interlaced DCT and other codec specific info. */
      switch( codec_type ) {
        case BUF_VIDEO_DV:
          this->frame_flags |= VO_INTERLACED_FLAG;
          break;
        case BUF_VIDEO_MPEG:
          this->frame_flags |= VO_INTERLACED_FLAG;
          break;
        case BUF_VIDEO_MJPEG:
          this->frame_flags |= VO_INTERLACED_FLAG;
          break;
        case BUF_VIDEO_HUFFYUV:
          this->frame_flags |= VO_INTERLACED_FLAG;
          break;
      }

      /* skip decoding b frames if too late */
      this->context->hurry_up = (this->skipframes > 2) ? 1:0;

      offset = 0;
      while (this->size>0) {
        
        /* DV frames can be completely skipped */
        if( codec_type == BUF_VIDEO_DV && this->skipframes ) {
          len = this->size;
	  got_picture = 1;
	} else
          len = avcodec_decode_video (this->context, this->av_frame,
				      &got_picture, &this->buf[offset],
				      this->size);
	if (len<0) {
	  xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "ffmpeg: error decompressing frame\n");
	  this->size=0;
	  return;
	}

	this->size -= len;
	offset += len;

	if (!got_picture || !this->av_frame->data[0]) {
	  xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG, 
		  "ffmpeg: didn't get a picture, got %d bytes left\n", this->size);

	  if (this->size>0)
	    memmove (this->buf, &this->buf[offset], this->size);

	  return;
	}

	lprintf ("got a picture\n");

	this->aspect_ratio = av_q2d(this->context->sample_aspect_ratio) * 
            (double) this->context->width / (double) this->context->height;

	if(!this->av_frame->opaque) {
	  img = this->stream->video_out->get_frame (this->stream->video_out,
						    this->context->width,
						    this->context->height,
						    this->aspect_ratio, 
						    this->output_format,
						    VO_BOTH_FIELDS|this->frame_flags);
	  free_img = 1;
	} else {
	  img = (vo_frame_t*) this->av_frame->opaque;
	  free_img = 0;
	}

	if (len<0 || this->skipframes) {
	  if( !this->skipframes )
	    xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "ffmpeg: error decompressing frame\n");
	  img->bad_frame = 1;
	} else {
	  img->bad_frame = 0;

	  if(this->pp_quality_changed)
	    pp_change_quality(this);

	  if(this->pp_available && this->pp_quality) {

	    if(this->av_frame->opaque) {
	      img = this->stream->video_out->get_frame (this->stream->video_out,
						        img->width,
						        img->height,
						        this->aspect_ratio, 
						        this->output_format,
						        VO_BOTH_FIELDS|this->frame_flags);
	      free_img = 1;
	      img->bad_frame = 0;
	    }

	    pp_postprocess(this->av_frame->data, this->av_frame->linesize, 
			   img->base, img->pitches, 
			   img->width, img->height,
			   this->av_frame->qscale_table, this->av_frame->qstride,
			   this->pp_mode, this->pp_context, 
			   this->av_frame->pict_type);

	  } else if(!this->av_frame->opaque) {
	    ff_convert_frame(this, img);
	  }
	}
      
	img->pts      = buf->pts;
	buf->pts      = 0;
	img->duration = this->video_step;

	this->skipframes = img->draw(img, this->stream);
	if( this->skipframes < 0 )
	  this->skipframes = 0;

	if(free_img)
	  img->free(img);
      }
    }

    if(buf->decoder_flags & BUF_FLAG_FRAME_START) {
      if(codec_type == BUF_VIDEO_RV10) {
        this->context->slice_offset[0] = this->size;
        this->context->slice_count = 1;
      }
    }
    
  } else {
    lprintf ("data but decoder not initialized (headers missing)\n");
  }
}

static void ff_flush (video_decoder_t *this_gen) {
  lprintf ("ff_flush\n");
}

static void ff_reset (video_decoder_t *this_gen) {
  ff_video_decoder_t *this = (ff_video_decoder_t *) this_gen;

  lprintf ("ff_reset\n");

  this->size = 0;
  if(this->context)
    avcodec_flush_buffers(this->context);
}

static void ff_discontinuity (video_decoder_t *this_gen) {
  
  lprintf ("ff_discontinuity\n");
}

void avcodec_register_all(void)
{
    static int inited = 0;
    
    if (inited != 0)
	return;
    inited = 1;

    /* decoders */
    register_avcodec(&h263_decoder);
    register_avcodec(&mpeg4_decoder);
    register_avcodec(&msmpeg4v1_decoder);
    register_avcodec(&msmpeg4v2_decoder);
    register_avcodec(&msmpeg4v3_decoder);
    register_avcodec(&wmv1_decoder);
    register_avcodec(&wmv2_decoder);
    register_avcodec(&h263i_decoder);
    register_avcodec(&rv10_decoder);
    register_avcodec(&svq1_decoder);
    register_avcodec(&svq3_decoder);
    register_avcodec(&wmav1_decoder);
    register_avcodec(&wmav2_decoder);
    register_avcodec(&indeo3_decoder);
    register_avcodec(&mpeg1video_decoder);
    register_avcodec(&dvvideo_decoder);
    register_avcodec(&pcm_s16le_decoder);
    register_avcodec(&mjpeg_decoder);
    register_avcodec(&mjpegb_decoder);
    register_avcodec(&mp2_decoder);
    register_avcodec(&mp3_decoder);
    register_avcodec(&mace3_decoder);
    register_avcodec(&mace6_decoder);
    register_avcodec(&huffyuv_decoder);
    register_avcodec(&cyuv_decoder);
    register_avcodec(&h264_decoder);
    register_avcodec(&vp3_decoder);
    register_avcodec(&fourxm_decoder);
    register_avcodec(&ra_144_decoder);
    register_avcodec(&ra_288_decoder);
    register_avcodec(&adpcm_ms_decoder);
    register_avcodec(&adpcm_ima_qt_decoder);
    register_avcodec(&adpcm_ima_wav_decoder);
    register_avcodec(&adpcm_ima_dk3_decoder);
    register_avcodec(&adpcm_ima_dk4_decoder);
    register_avcodec(&adpcm_ima_ws_decoder);
    register_avcodec(&adpcm_xa_decoder);
    register_avcodec(&adpcm_4xm_decoder);
    register_avcodec(&pcm_alaw_decoder);
    register_avcodec(&pcm_mulaw_decoder);
    register_avcodec(&roq_dpcm_decoder);
    register_avcodec(&interplay_dpcm_decoder);
    register_avcodec(&cinepak_decoder);
    register_avcodec(&msvideo1_decoder);
    register_avcodec(&msrle_decoder);
    register_avcodec(&rpza_decoder);
    register_avcodec(&roq_decoder);
    register_avcodec(&idcin_decoder);
    register_avcodec(&xan_wc3_decoder);
    register_avcodec(&vqa_decoder);
    register_avcodec(&interplay_video_decoder);
}

static void ff_dispose (video_decoder_t *this_gen) {
  ff_video_decoder_t *this = (ff_video_decoder_t *) this_gen;

  lprintf ("ff_dispose\n");

  if (this->decoder_ok) {
    avcodec_close (this->context);

    this->stream->video_out->close(this->stream->video_out, this->stream);
    this->decoder_ok = 0;
  }

  if(this->context && this->context->slice_offset)
    free(this->context->slice_offset);

  if(this->context && this->context->extradata)
    free(this->context->extradata);

  if((this->context) && 
   ((this->context->pix_fmt == PIX_FMT_RGBA32) ||
    (this->context->pix_fmt == PIX_FMT_RGB565) ||
    (this->context->pix_fmt == PIX_FMT_RGB555) ||
    (this->context->pix_fmt == PIX_FMT_PAL8)))
    free_yuv_planes(&this->yuv);
  
  if( this->context )
    free( this->context );

  if( this->av_frame )
    free( this->av_frame );
  
  if (this->buf)
    free(this->buf);
  this->buf = NULL;
  
  if(this->pp_context)
    pp_free_context(this->pp_context);
    
  if(this->pp_mode)
    pp_free_mode(this->pp_mode);
  
  free (this->chunk_buffer);
  free (this_gen);
}

static video_decoder_t *ff_video_open_plugin (video_decoder_class_t *class_gen, xine_stream_t *stream) {

  ff_video_decoder_t  *this ;

  lprintf ("open_plugin\n");

  this = (ff_video_decoder_t *) xine_xmalloc (sizeof (ff_video_decoder_t));

  this->video_decoder.decode_data         = ff_decode_data;
  this->video_decoder.flush               = ff_flush;
  this->video_decoder.reset               = ff_reset;
  this->video_decoder.discontinuity       = ff_discontinuity;
  this->video_decoder.dispose             = ff_dispose;
  this->size				  = 0;

  this->stream                            = stream;
  this->class                             = (ff_video_class_t *) class_gen;
  this->class->ip                         = this;

  this->chunk_buffer = xine_xmalloc (SLICE_BUFFER_SIZE + 4);

  this->decoder_ok    = 0;
  this->buf           = NULL;

  this->shift         = 0xffffff00;
  this->code          = 0xb4;
  this->chunk_ptr     = this->chunk_buffer;

  this->is_continous  = 0;
  this->aspect_ratio = 0;

  this->pp_quality  = 0;
  this->pp_context  = NULL;
  this->pp_mode     = NULL;

  return &this->video_decoder;
}

/*
 * ffmpeg plugin class
 */

static char *ff_video_get_identifier (video_decoder_class_t *this) {
  return "ffmpeg video";
}

static char *ff_video_get_description (video_decoder_class_t *this) {
  return "ffmpeg based video decoder plugin";
}

static void ff_video_dispose_class (video_decoder_class_t *this) {
  free (this);
}

static void init_once_routine(void) {
  avcodec_init();
  avcodec_register_all();
}

static void *init_video_plugin (xine_t *xine, void *data) {

  ff_video_class_t *this;
  config_values_t  *config;
  
  this = (ff_video_class_t *) xine_xmalloc (sizeof (ff_video_class_t));

  this->decoder_class.open_plugin     = ff_video_open_plugin;
  this->decoder_class.get_identifier  = ff_video_get_identifier;
  this->decoder_class.get_description = ff_video_get_description;
  this->decoder_class.dispose         = ff_video_dispose_class;
  this->ip                            = NULL;
  this->xine                          = xine;

  pthread_once( &once_control, init_once_routine );
  
  /* Configuration for post processing quality - default to mid (3) for the
   * moment */
  config = xine->config;
  xine->config->register_range(config, "codec.ffmpeg_pp_quality", 3, 
    0, PP_QUALITY_MAX,  _("ffmpeg mpeg-4 postprocessing quality"), NULL,
    10, pp_quality_cb, this);  
  
  return this;
}

static void ff_audio_decode_data (audio_decoder_t *this_gen, buf_element_t *buf) {

  ff_audio_decoder_t *this = (ff_audio_decoder_t *) this_gen;
  int bytes_consumed;
  int decode_buffer_size;
  int offset;
  int out;
  audio_buffer_t *audio_buffer;
  int bytes_to_send;

  if (buf->decoder_flags & BUF_FLAG_STDHEADER) {

    int codec_type;
    xine_waveformatex *audio_header = (xine_waveformatex *)buf->content;

    codec_type = buf->type & 0xFFFF0000;
    this->codec = NULL;

    switch (codec_type) {
    case BUF_AUDIO_WMAV1:
      this->codec = avcodec_find_decoder (CODEC_ID_WMAV1);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,
	"MS Windows Media Audio 1 (ffmpeg)");
      break;
    case BUF_AUDIO_WMAV2:
      this->codec = avcodec_find_decoder (CODEC_ID_WMAV2);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"MS Windows Media Audio 2 (ffmpeg)");
      break;
    /* FIXME DV Audio has disappeared from libffmpeg
    case BUF_AUDIO_DV:
      this->codec = avcodec_find_decoder (CODEC_ID_DVAUDIO);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"DV Audio (ffmpeg)");
      break;
    */
    case BUF_AUDIO_14_4:
      this->codec = avcodec_find_decoder (CODEC_ID_RA_144);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"Real 14.4 (ffmpeg)");
      break;
    case BUF_AUDIO_28_8:
      this->codec = avcodec_find_decoder (CODEC_ID_RA_288);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"Real 28.8 (ffmpeg)");
      break;
    case BUF_AUDIO_MPEG:
      this->codec = avcodec_find_decoder (CODEC_ID_MP3LAME);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"MP3 (ffmpeg)");
      break;
    case BUF_AUDIO_MSADPCM:
      this->codec = avcodec_find_decoder (CODEC_ID_ADPCM_MS);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"MS ADPCM (ffmpeg)");
      break;
    case BUF_AUDIO_QTIMAADPCM:
      this->codec = avcodec_find_decoder (CODEC_ID_ADPCM_IMA_QT);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"QT IMA ADPCM (ffmpeg)");
      break;
    case BUF_AUDIO_MSIMAADPCM:
      this->codec = avcodec_find_decoder (CODEC_ID_ADPCM_IMA_WAV);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"MS IMA ADPCM (ffmpeg)");
      break;
    case BUF_AUDIO_DK3ADPCM:
      this->codec = avcodec_find_decoder (CODEC_ID_ADPCM_IMA_DK3);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"Duck DK3 ADPCM (ffmpeg)");
      break;
    case BUF_AUDIO_DK4ADPCM:
      this->codec = avcodec_find_decoder (CODEC_ID_ADPCM_IMA_DK4);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"Duck DK4 ADPCM (ffmpeg)");
      break;
    case BUF_AUDIO_VQA_IMA:
      this->codec = avcodec_find_decoder (CODEC_ID_ADPCM_IMA_WS);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"Westwood Studios IMA (ffmpeg)");
      break;
    case BUF_AUDIO_XA_ADPCM:
      this->codec = avcodec_find_decoder (CODEC_ID_ADPCM_XA);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"CD-ROM/XA ADPCM (ffmpeg)");
      break;
    case BUF_AUDIO_4X_ADPCM:
      this->codec = avcodec_find_decoder (CODEC_ID_ADPCM_4XM);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"4X ADPCM (ffmpeg)");
      break;
    case BUF_AUDIO_MULAW:
      this->codec = avcodec_find_decoder (CODEC_ID_PCM_MULAW);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"mu-law logarithmic PCM (ffmpeg)");
      break;
    case BUF_AUDIO_ALAW:
      this->codec = avcodec_find_decoder (CODEC_ID_PCM_ALAW);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"A-law logarithmic PCM (ffmpeg)");
      break;
    case BUF_AUDIO_ROQ:
      this->codec = avcodec_find_decoder (CODEC_ID_ROQ_DPCM);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"RoQ DPCM (ffmpeg)");
      break;
    case BUF_AUDIO_INTERPLAY:
      this->codec = avcodec_find_decoder (CODEC_ID_INTERPLAY_DPCM);
      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,  
	"Interplay DPCM (ffmpeg)");
      break;
    }

    if (!this->codec) {
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "could not open ffmpeg decoder for buf type 0x%X\n",
        codec_type);
      return;
    }

    this->context = avcodec_alloc_context();

    this->context->sample_rate = this->audio_sample_rate = buf->decoder_info[1];
    this->audio_bits = buf->decoder_info[2];
    this->context->channels = this->audio_channels = buf->decoder_info[3];
    this->context->block_align = audio_header->nBlockAlign;
    this->context->bit_rate = audio_header->nAvgBytesPerSec * 8;
    this->context->codec_id = this->codec->id;
    this->context->codec_tag = _x_stream_info_get(this->stream, XINE_STREAM_INFO_AUDIO_FOURCC);
    if( audio_header->cbSize > 0 ) {
      this->context->extradata = malloc(audio_header->cbSize);
      this->context->extradata_size = audio_header->cbSize;
      memcpy( this->context->extradata, 
              (uint8_t *)audio_header + sizeof(xine_waveformatex),
              audio_header->cbSize ); 
    }

    this->buf = xine_xmalloc(AUDIOBUFSIZE);
    this->bufsize = AUDIOBUFSIZE;
    this->size = 0;

    this->decode_buffer = xine_xmalloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

    if (avcodec_open (this->context, this->codec) < 0) {
      xprintf (this->stream->xine, XINE_VERBOSITY_LOG, _("ffmpeg: couldn't open decoder\n"));
      _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_HANDLED, 0);
      return;
    }

    this->decoder_ok = 1;

    return;

  } else if ((buf->decoder_flags & BUF_FLAG_SPECIAL) &&
             (buf->decoder_info[1] == BUF_SPECIAL_STSD_ATOM)) {

    this->context->extradata_size = buf->decoder_info[2];
    this->context->extradata = xine_xmalloc(buf->decoder_info[2]);
    memcpy(this->context->extradata, buf->decoder_info_ptr[2],
      buf->decoder_info[2]);

  } else if (this->decoder_ok && !(buf->decoder_flags & BUF_FLAG_SPECIAL)) {

    if (!this->output_open) {
      this->output_open = this->stream->audio_out->open(this->stream->audio_out,
        this->stream, this->audio_bits, this->audio_sample_rate,
        (this->audio_channels == 2) ? AO_CAP_MODE_STEREO : AO_CAP_MODE_MONO);
    }

    /* if the audio still isn't open, bail */
    if (!this->output_open)
      return;

    if( buf->decoder_flags & BUF_FLAG_PREVIEW )
      return;

    if( this->size + buf->size > this->bufsize ) {
      this->bufsize = this->size + 2 * buf->size;
      xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
	      _("ffmpeg: increasing source buffer to %d to avoid overflow.\n"), this->bufsize);
      this->buf = realloc( this->buf, this->bufsize );
    }

    xine_fast_memcpy (&this->buf[this->size], buf->content, buf->size);
    this->size += buf->size;

    if (buf->decoder_flags & BUF_FLAG_FRAME_END)  { /* time to decode a frame */

      offset = 0;
      while (this->size>0) {
        bytes_consumed = avcodec_decode_audio (this->context, 
                                               (int16_t *)this->decode_buffer,
                                               &decode_buffer_size, 
                                               &this->buf[offset],
                                               this->size);

        if (bytes_consumed<0) {
          xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "ffmpeg: error decompressing audio frame\n");
          this->size=0;
          return;
        }

        /* dispatch the decoded audio */
        out = 0;
        while (out < decode_buffer_size) {
          audio_buffer = 
            this->stream->audio_out->get_buffer (this->stream->audio_out);
          if (audio_buffer->mem_size == 0) {
            xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, 
		     "ffmpeg: Help! Allocated audio buffer with nothing in it!\n");
            return;
          }

          if ((decode_buffer_size - out) > audio_buffer->mem_size)
            bytes_to_send = audio_buffer->mem_size;
          else
            bytes_to_send = decode_buffer_size - out;

          /* fill up this buffer */
          xine_fast_memcpy(audio_buffer->mem, &this->decode_buffer[out],
            bytes_to_send);
          /* byte count / 2 (bytes / sample) / channels */
          audio_buffer->num_frames = bytes_to_send / 2 / this->audio_channels;

          audio_buffer->vpts = buf->pts;
          buf->pts = 0;  /* only first buffer gets the real pts */
          this->stream->audio_out->put_buffer (this->stream->audio_out,
	    audio_buffer, this->stream);

          out += bytes_to_send;
        }

        this->size -= bytes_consumed;
        offset += bytes_consumed;
      }

      /* reset internal accumulation buffer */
      this->size = 0;
    }
  }
}

static void ff_audio_reset (audio_decoder_t *this_gen) {
  ff_audio_decoder_t *this = (ff_audio_decoder_t *) this_gen;
  
  this->size = 0;

  /* try to reset the wma decoder */
  if( this->context ) {  
    avcodec_close (this->context);
    avcodec_open (this->context, this->codec);
  }
}

static void ff_audio_discontinuity (audio_decoder_t *this_gen) {
}

static void ff_audio_dispose (audio_decoder_t *this_gen) {

  ff_audio_decoder_t *this = (ff_audio_decoder_t *) this_gen;
  
  if( this->context )
    avcodec_close (this->context);

  if (this->output_open)
    this->stream->audio_out->close (this->stream->audio_out, this->stream);
  this->output_open = 0;

  free(this->buf);
  free(this->decode_buffer);

  if(this->context && this->context->extradata)
    free(this->context->extradata);

  if(this->context)
    free(this->context);

  free (this_gen);
}

static audio_decoder_t *ff_audio_open_plugin (audio_decoder_class_t *class_gen, xine_stream_t *stream) {

  ff_audio_decoder_t *this ;

  this = (ff_audio_decoder_t *) xine_xmalloc (sizeof (ff_audio_decoder_t));

  this->audio_decoder.decode_data         = ff_audio_decode_data;
  this->audio_decoder.reset               = ff_audio_reset;
  this->audio_decoder.discontinuity       = ff_audio_discontinuity;
  this->audio_decoder.dispose             = ff_audio_dispose;

  this->output_open = 0;
  this->audio_channels = 0;
  this->stream = stream;
  this->buf = NULL;
  this->size = 0;
  this->decoder_ok = 0;
  
  return &this->audio_decoder;
}

static char *ff_audio_get_identifier (audio_decoder_class_t *this) {
  return "ffmpeg audio";
}

static char *ff_audio_get_description (audio_decoder_class_t *this) {
  return "ffmpeg based audio decoder plugin";
}

static void ff_audio_dispose_class (audio_decoder_class_t *this) {
  free (this);
}

static void *init_audio_plugin (xine_t *xine, void *data) {

  ff_audio_class_t *this ;

  this = (ff_audio_class_t *) xine_xmalloc (sizeof (ff_audio_class_t));

  this->decoder_class.open_plugin     = ff_audio_open_plugin;
  this->decoder_class.get_identifier  = ff_audio_get_identifier;
  this->decoder_class.get_description = ff_audio_get_description;
  this->decoder_class.dispose         = ff_audio_dispose_class;

  pthread_once( &once_control, init_once_routine );

  return this;
}

/*
 * exported plugin catalog entry
 */

static uint32_t supported_video_types[] = { 
  BUF_VIDEO_MSMPEG4_V1, 
  BUF_VIDEO_MSMPEG4_V2,
  BUF_VIDEO_MSMPEG4_V3, 
  BUF_VIDEO_WMV7, 
  BUF_VIDEO_MPEG4,
  BUF_VIDEO_XVID, 
  BUF_VIDEO_DIVX5, 
  BUF_VIDEO_MJPEG,
  BUF_VIDEO_H263, 
  BUF_VIDEO_RV10,
  BUF_VIDEO_IV31,
  BUF_VIDEO_IV32,
  BUF_VIDEO_SORENSON_V1,
  BUF_VIDEO_SORENSON_V3,
  BUF_VIDEO_JPEG, 
  BUF_VIDEO_MPEG, 
  BUF_VIDEO_DV,
  BUF_VIDEO_HUFFYUV,
  BUF_VIDEO_VP31,
  BUF_VIDEO_4XM,
  BUF_VIDEO_CINEPAK,
  BUF_VIDEO_MSVC,
  BUF_VIDEO_MSRLE,
  BUF_VIDEO_RPZA,
  BUF_VIDEO_CYUV,
  BUF_VIDEO_ROQ,
  BUF_VIDEO_IDCIN,
  BUF_VIDEO_WC3,
  BUF_VIDEO_VQA,
  BUF_VIDEO_INTERPLAY,
  0 
};

static uint32_t experimental_video_types[] = { 
  BUF_VIDEO_WMV8,
  0 
};

static uint32_t supported_audio_types[] = { 
  BUF_AUDIO_WMAV1,
  BUF_AUDIO_WMAV2,
  BUF_AUDIO_DV,
  BUF_AUDIO_14_4,
  BUF_AUDIO_28_8,
  BUF_AUDIO_MULAW,
  BUF_AUDIO_ALAW,
  BUF_AUDIO_MSADPCM,
  BUF_AUDIO_QTIMAADPCM,
  BUF_AUDIO_MSIMAADPCM,
  BUF_AUDIO_DK3ADPCM,
  BUF_AUDIO_DK4ADPCM,
  BUF_AUDIO_XA_ADPCM,
  BUF_AUDIO_ROQ,
  BUF_AUDIO_INTERPLAY,
  BUF_AUDIO_VQA_IMA,
  BUF_AUDIO_4X_ADPCM,
  /* BUF_AUDIO_MPEG, */
  0
};

static decoder_info_t dec_info_ffmpeg_video = {
  supported_video_types,   /* supported types */
  5                        /* priority        */
};

static decoder_info_t dec_info_ffmpeg_experimental_video = {
  experimental_video_types,   /* supported types */
  0                           /* priority        */
};

static decoder_info_t dec_info_ffmpeg_audio = {
  supported_audio_types,   /* supported types */
  5                        /* priority        */
};

plugin_info_t xine_plugin_info[] = {
  /* type, API, "name", version, special_info, init_function */  
  { PLUGIN_VIDEO_DECODER | PLUGIN_MUST_PRELOAD, 18, "ffmpegvideo", XINE_VERSION_CODE, &dec_info_ffmpeg_video, init_video_plugin },
  { PLUGIN_VIDEO_DECODER, 18, "ffmpeg-wmv8", XINE_VERSION_CODE, &dec_info_ffmpeg_experimental_video, init_video_plugin },
  { PLUGIN_AUDIO_DECODER, 15, "ffmpegaudio", XINE_VERSION_CODE, &dec_info_ffmpeg_audio, init_audio_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
