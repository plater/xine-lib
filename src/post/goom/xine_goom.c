/*
 * Copyright (C) 2000-2003 the xine project
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
 * $Id: xine_goom.c,v 1.49 2004/02/12 18:25:07 mroi Exp $
 *
 * GOOM post plugin.
 *
 * first version by Mark Thomas
 * ported to post plugin architecture by Miguel Freitas
 * real work by goom author, JC Hoelt <jeko@free.fr>.
 */

#include <stdio.h>

#define LOG_MODULE "goom"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "config.h"
#include "xine_internal.h"
#include "xineutils.h"
#include "post.h"

#include "goom_config.h"
#include "goom_core.h"

#define NUMSAMPLES  512
#define FPS          10

#define GOOM_WIDTH  320
#define GOOM_HEIGHT 240

/* colorspace conversion methods */
const char * goom_csc_methods[]={
  "Fast but not photorealistic",
  "Slow but looks better (mmx)",
  NULL
};

typedef struct post_plugin_goom_s post_plugin_goom_t;

typedef struct post_class_goom_s post_class_goom_t;

struct post_class_goom_s {
  post_class_t class;

  post_plugin_goom_t *ip;
  xine_t             *xine;
};

struct post_plugin_goom_s {
  post_plugin_t post;

  /* private data */
  xine_video_port_t *vo_port;
  post_out_t         video_output;
  
  post_class_goom_t *class;
  
  /* private metronom for syncing the video */
  metronom_t        *metronom;
  
  int data_idx;
  gint16 data [2][512];
  audio_buffer_t buf;   /* dummy buffer just to hold a copy of audio data */
  
  int channels;
  int sample_rate;
  int sample_counter;
  int samples_per_frame;
  int width, height;
  int width_back, height_back;
  double ratio;
  int fps;
  int csc_method;

  yuv_planes_t yuv;
  
  /* frame skipping */
  int skip_frame;
};


/* plugin class initialization function */
static void *goom_init_plugin(xine_t *xine, void *);


/* plugin catalog information */
post_info_t goom_special_info = { 
  XINE_POST_TYPE_AUDIO_VISUALIZATION
};

plugin_info_t xine_plugin_info[] = {
  /* type, API, "name", version, special_info, init_function */  
  { PLUGIN_POST | PLUGIN_MUST_PRELOAD, 8, "goom", XINE_VERSION_CODE, &goom_special_info, &goom_init_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};


/* plugin class functions */
static post_plugin_t *goom_open_plugin(post_class_t *class_gen, int inputs,
					 xine_audio_port_t **audio_target,
					 xine_video_port_t **video_target);
static char          *goom_get_identifier(post_class_t *class_gen);
static char          *goom_get_description(post_class_t *class_gen);
static void           goom_class_dispose(post_class_t *class_gen);

/* plugin instance functions */
static void           goom_dispose(post_plugin_t *this_gen);

/* rewire function */
static int            goom_rewire_video(xine_post_out_t *output, void *data);

static int goom_port_open(xine_audio_port_t *this, xine_stream_t *stream,
		   uint32_t bits, uint32_t rate, int mode);

static void goom_port_close(xine_audio_port_t *this, xine_stream_t *stream );

static void goom_port_put_buffer (xine_audio_port_t *this, audio_buffer_t *buf, xine_stream_t *stream);

static void fps_changed_cb(void *data, xine_cfg_entry_t *cfg) {
  post_class_goom_t *class = (post_class_goom_t*) data;
  
  if(class->ip) {
    post_plugin_goom_t *this = class->ip;
    this->fps = cfg->num_value;

    if(this->sample_rate && this->fps)
      this->samples_per_frame = this->sample_rate / this->fps;
  }
}

static void width_changed_cb(void *data, xine_cfg_entry_t *cfg) {
  post_class_goom_t *class = (post_class_goom_t*) data;
  
  if(class->ip) {
    post_plugin_goom_t *this = class->ip;
    this->width = cfg->num_value;
  }
}

static void height_changed_cb(void *data, xine_cfg_entry_t *cfg) {
  post_class_goom_t *class = (post_class_goom_t*) data;
  
  if(class->ip) {
    post_plugin_goom_t *this = class->ip;
    this->height = cfg->num_value;
  }
}

static void csc_method_changed_cb(void *data, xine_cfg_entry_t *cfg) {
  post_class_goom_t *class = (post_class_goom_t*) data;
  
  if(class->ip) {
    post_plugin_goom_t *this = class->ip;
    this->csc_method = cfg->num_value;
  }
}

static void *goom_init_plugin(xine_t *xine, void *data)
{
  post_class_goom_t *this = (post_class_goom_t *)xine_xmalloc(sizeof(post_class_goom_t));
  config_values_t   *cfg;

  if (!this)
    return NULL;
  
  this->class.open_plugin     = goom_open_plugin;
  this->class.get_identifier  = goom_get_identifier;
  this->class.get_description = goom_get_description;
  this->class.dispose         = goom_class_dispose;
  this->ip                    = NULL;
  this->xine                  = xine;
  
  cfg = xine->config;

  cfg->register_num (cfg, "post.goom_fps", FPS,
                                 _("Frames per second to generate with Goom"),
                                 NULL, 10, fps_changed_cb, this);

  cfg->register_num (cfg, "post.goom_width", GOOM_WIDTH,
                                   _("Goom image width in pixels"),
                                   NULL, 20, width_changed_cb, this);
  
  cfg->register_num (cfg, "post.goom_height", GOOM_HEIGHT,
                                    _("Goom image height in pixels"),
                                    NULL, 20, height_changed_cb, this);
  

  cfg->register_enum (cfg, "post.goom_csc_method", 0,
                           (char **)goom_csc_methods,
                           _("Colorspace conversion method used by Goom"),
                           NULL, 20, csc_method_changed_cb, this);

  return &this->class;
}


static post_plugin_t *goom_open_plugin(post_class_t *class_gen, int inputs,
					 xine_audio_port_t **audio_target,
					 xine_video_port_t **video_target)
{
  post_plugin_goom_t *this  = (post_plugin_goom_t *)xine_xmalloc(sizeof(post_plugin_goom_t));
  post_class_goom_t  *class = (post_class_goom_t*) class_gen;
  post_in_t          *input;
  post_out_t         *output;
  post_out_t         *outputv;
  post_audio_port_t  *port;
  xine_cfg_entry_t    fps_entry, width_entry, height_entry, csc_method_entry;

  if (!this || !video_target || !video_target[0] || !audio_target || !audio_target[0]) {
    free(this);
    return NULL;
  }
  
  _x_post_init(&this->post, 1, 0);
  
  /*
   * Lookup config entries.
   */
  this->class = class;
  class->ip   = this;
  this->vo_port = video_target[0];
  
  this->metronom = _x_metronom_init(1, 0, class->xine);

  lprintf("goom_open_plugin\n");

  if(xine_config_lookup_entry(class->xine, "post.goom_fps",
                              &fps_entry)) 
    fps_changed_cb(class, &fps_entry);

  if(xine_config_lookup_entry(class->xine, "post.goom_width",
                              &width_entry)) 
    width_changed_cb(class, &width_entry);

  if(xine_config_lookup_entry(class->xine, "post.goom_height",
                              &height_entry)) 
    height_changed_cb(class, &height_entry);

  if(xine_config_lookup_entry(class->xine, "post.goom_csc_method",
                              &csc_method_entry)) 
    csc_method_changed_cb(class, &csc_method_entry);

  this->width_back  = this->width;
  this->height_back = this->height;

  goom_init (this->width_back, this->height_back, 0);

  this->ratio = (double)this->width_back/(double)this->height_back;

  this->sample_counter = 0;
  this->buf.mem = NULL;
  this->buf.mem_size = 0;  

  port = _x_post_intercept_audio_port(&this->post, audio_target[0], &input, &output);
  port->new_port.open       = goom_port_open;
  port->new_port.close      = goom_port_close;
  port->new_port.put_buffer = goom_port_put_buffer;
  
  outputv                  = &this->video_output;
  outputv->xine_out.name   = "generated video";
  outputv->xine_out.type   = XINE_POST_DATA_VIDEO;
  outputv->xine_out.data   = (xine_video_port_t **)&this->vo_port;
  outputv->xine_out.rewire = goom_rewire_video;
  outputv->post            = &this->post;
  xine_list_append_content(this->post.output, outputv);
  
  this->post.xine_post.audio_input[0] = &port->new_port;

  this->post.dispose = goom_dispose;

  return &this->post;
}

static char *goom_get_identifier(post_class_t *class_gen)
{
  return "goom";
}

static char *goom_get_description(post_class_t *class_gen)
{
  return "What a GOOM";
}

static void goom_class_dispose(post_class_t *class_gen)
{
  free(class_gen);
}


static void goom_dispose(post_plugin_t *this_gen)
{
  post_plugin_goom_t *this   = (post_plugin_goom_t *)this_gen;

  if (_x_post_dispose(this_gen)) {
    this->class->ip = NULL;

    goom_close();

    this->metronom->exit(this->metronom);

    if(this->buf.mem)
      free(this->buf.mem);
    free(this);
  }
}


static int goom_rewire_video(xine_post_out_t *output_gen, void *data)
{
  post_out_t *output = (post_out_t *)output_gen;
  xine_video_port_t *old_port = *(xine_video_port_t **)output_gen->data;
  xine_video_port_t *new_port = (xine_video_port_t *)data;
  post_plugin_goom_t *this = (post_plugin_goom_t *)output->post;
  
  if (!data)
    return 0;
  /* register our stream at the new output port */
  old_port->close(old_port, NULL);
  new_port->open(new_port, NULL);
  /* reconnect ourselves */
  this->vo_port = new_port;
  return 1;
}

static int goom_port_open(xine_audio_port_t *port_gen, xine_stream_t *stream,
		   uint32_t bits, uint32_t rate, int mode) {

  post_audio_port_t  *port = (post_audio_port_t *)port_gen;
  post_plugin_goom_t *this = (post_plugin_goom_t *)port->post;

  _x_post_rewire(&this->post);
  _x_post_inc_usage(port);
  
  if (stream)
    port->stream = stream;
  else
    port->stream = POST_NULL_STREAM;
  port->bits = bits;
  port->rate = rate;
  port->mode = mode;
  
  this->channels = _x_ao_mode2channels(mode);
  this->sample_rate = rate;
  this->samples_per_frame = rate / this->fps;
  this->data_idx = 0;
  init_yuv_planes(&this->yuv, this->width, this->height);
  this->skip_frame = 0;
  
  this->vo_port->open(this->vo_port, NULL);
  this->metronom->set_master(this->metronom, stream->metronom);

  return port->original_port->open(port->original_port, stream, bits, rate, mode );
}

static void goom_port_close(xine_audio_port_t *port_gen, xine_stream_t *stream ) {

  post_audio_port_t  *port = (post_audio_port_t *)port_gen;
  post_plugin_goom_t *this = (post_plugin_goom_t *)port->post;

  free_yuv_planes(&this->yuv);

  port->stream = NULL;
  
  this->vo_port->close(this->vo_port, NULL);
  this->metronom->set_master(this->metronom, NULL);
 
  port->original_port->close(port->original_port, stream );
  
  _x_post_dec_usage(port);
}

static void goom_port_put_buffer (xine_audio_port_t *port_gen, 
                             audio_buffer_t *buf, xine_stream_t *stream) {
  
  post_audio_port_t  *port = (post_audio_port_t *)port_gen;
  post_plugin_goom_t *this = (post_plugin_goom_t *)port->post;
  vo_frame_t         *frame;
  uint8_t *goom_frame, *goom_frame_end;
  int16_t *data;
  int8_t *data8;
  int samples_used = 0;
  int64_t pts = buf->vpts;
  int i, j;
  uint8_t *dest_ptr;
  int width, height;
  
  /* make a copy of buf data for private use */
  if( this->buf.mem_size < buf->mem_size ) {
    this->buf.mem = realloc(this->buf.mem, buf->mem_size);
    this->buf.mem_size = buf->mem_size;
  }
  memcpy(this->buf.mem, buf->mem, 
         buf->num_frames*this->channels*((port->bits == 8)?1:2));
  this->buf.num_frames = buf->num_frames;
  
  /* pass data to original port */
  port->original_port->put_buffer(port->original_port, buf, stream);  
  
  /* we must not use original data anymore, it should have already being moved
   * to the fifo of free audio buffers. just use our private copy instead.
   */
  buf = &this->buf; 

  this->sample_counter += buf->num_frames;
  
  j = (this->channels >= 2) ? 1 : 0;

  do {
    
    if( port->bits == 8 ) {
      data8 = (int8_t *)buf->mem;
      data8 += samples_used * this->channels;
  
      /* scale 8 bit data to 16 bits and convert to signed as well */
      for( i = 0; i < buf->num_frames && this->data_idx < NUMSAMPLES;
           i++, this->data_idx++, data8 += this->channels ) {
        this->data[0][this->data_idx] = ((int16_t)data8[0] << 8) - 0x8000;
        this->data[1][this->data_idx] = ((int16_t)data8[j] << 8) - 0x8000;
      }
    } else {
      data = buf->mem;
      data += samples_used * this->channels;
  
      for( i = 0; i < buf->num_frames && this->data_idx < NUMSAMPLES;
           i++, this->data_idx++, data += this->channels ) {
        this->data[0][this->data_idx] = data[0];
        this->data[1][this->data_idx] = data[j];
      }
    }
  
    if( this->sample_counter >= this->samples_per_frame &&
        this->data_idx == NUMSAMPLES ) {
      this->data_idx = 0;
      samples_used += this->samples_per_frame;

      frame = this->vo_port->get_frame (this->vo_port, this->width_back, this->height_back,
                                        this->ratio, XINE_IMGFMT_YUY2,
                                        VO_BOTH_FIELDS);
      
      frame->extra_info->invalid = 1;
      frame->bad_frame = 0;
      frame->duration = 90000 * this->samples_per_frame / this->sample_rate;
      frame->pts = pts;
      this->metronom->got_video_frame(this->metronom, frame);
      
      this->sample_counter -= this->samples_per_frame;

      if (!this->skip_frame) {
        /* Try to be fast */
        goom_frame = (uint8_t *)goom_update (this->data, 0, 0, NULL, NULL);

        dest_ptr = frame -> base[0];
        goom_frame_end = goom_frame + 4 * (this->width_back * this->height_back);

        if ((this->csc_method == 1) && 
            (xine_mm_accel() & MM_ACCEL_X86_MMX)) {
          int plane_ptr = 0;

          while (goom_frame < goom_frame_end) {
            uint8_t r, g, b;
      
            /* don't take endianness into account since MMX is only available
             * on Intel processors */
            b = *goom_frame; goom_frame++;
            g = *goom_frame; goom_frame++;
            r = *goom_frame; goom_frame += 2;

            this->yuv.y[plane_ptr] = COMPUTE_Y(r, g, b);
            this->yuv.u[plane_ptr] = COMPUTE_U(r, g, b);
            this->yuv.v[plane_ptr] = COMPUTE_V(r, g, b);
            plane_ptr++;
          }

          yuv444_to_yuy2(&this->yuv, frame->base[0], frame->pitches[0]);

        } else {

          while (goom_frame < goom_frame_end) {
            uint8_t r1, g1, b1, r2, g2, b2;

#ifdef __BIG_ENDIAN__
            goom_frame ++;
            r1 = *goom_frame; goom_frame++;
            g1 = *goom_frame; goom_frame++;
            b1 = *goom_frame; goom_frame += 2;
            r2 = *goom_frame; goom_frame++;
            g2 = *goom_frame; goom_frame++;
            b2 = *goom_frame; goom_frame++;
#else
            b1 = *goom_frame; goom_frame++;
            g1 = *goom_frame; goom_frame++;
            r1 = *goom_frame; goom_frame += 2;
            b2 = *goom_frame; goom_frame++;
            g2 = *goom_frame; goom_frame++;
            r2 = *goom_frame; goom_frame += 2;
#endif
      
            *dest_ptr = COMPUTE_Y(r1, g1, b1);
            dest_ptr++;
            *dest_ptr = COMPUTE_U(r1, g1, b1);
            dest_ptr++;
            *dest_ptr = COMPUTE_Y(r2, g2, b2);
            dest_ptr++;
            *dest_ptr = COMPUTE_V(r2, g2, b2);
            dest_ptr++;
          }
        }

        this->skip_frame = frame->draw(frame, NULL);
      } else {
        frame->bad_frame = 1;
        frame->draw(frame, NULL);
	this->skip_frame--;
      }
      frame->free(frame);
      
      width  = this->width;
      height = this->height;
      if ((width != this->width_back) || (height != this->height_back)) {
          goom_close();
          goom_init (this->width, this->height, 0);
          this->width_back = width;
          this->height_back = height;
	  this->ratio = (double)width/(double)height;
	  free_yuv_planes(&this->yuv);
	  init_yuv_planes(&this->yuv, this->width, this->height);
       }
    }
  } while( this->sample_counter >= this->samples_per_frame );
}
