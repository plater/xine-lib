/* 
 * Copyright (C) 2000-2004 the xine project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 *
 * $Id: video_out_pgx32.c,v 1.3 2004/03/16 00:32:22 komadori Exp $
 *
 * video_out_pgx32.c, Sun PGX32 output plugin for xine
 *
 * written and currently maintained by
 *   Robin Kay <komadori [at] gekkou [dot] co [dot] uk>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/fbio.h>
#include <sys/visual_io.h>
#include <sys/mman.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "xine_internal.h"
#include "alphablend.h"
#include "bswap.h"
#include "vo_scale.h"
#include "xineutils.h"

/* gfxp register defines */

#define GFXP_VRAM_MMAPLEN 0x00800000
#define GFXP_REGS_MMAPLEN 0x00020000
#define GFXP_REGSBASE 0x00800000

#define FIFO_SPACE 0x0003

#define RASTERISER_MODE 0x1014
#define SCISSOR_MODE 0x1030
#define AREA_STIPPLE_MODE 0x1034
#define WINDOW_ORIGIN 0x1039

#define RECT_ORIGIN 0x101A
#define RECT_SIZE 0x101B

#define DY 0x1005

#define TEXTURE_ADDR_MODE 0x1070
#define SSTART 0x1071
#define DSDX 0x1072
#define DSDY_DOM 0x1073
#define TSTART 0x1074
#define DTDX 0x1075
#define DTDY_DOM 0x1076

#define TEXTURE_BASE_ADDR 0x10B0
#define TEXTURE_MAP_FORMAT 0x10B1
#define TEXTURE_DATA_FORMAT 0x10B2
#define TEXTURE_READ_MODE 0x10CE
#define TEXTURE_COLOUR_MODE 0x10D0

#define SHADING_MODE 0x10FC
#define ALPHA_BLENDING_MODE 0x1102
#define DITHERING_MODE 0x1103
#define LOGICAL_OP_MODE 0x1105
#define STENCIL_MODE 0x1131

#define READ_MODE 0x1150
#define WRITE_MODE 0x1157
#define WRITE_MASK 0x1158

#define YUV_MODE 0x11E0

#define RENDER 0x1007
#define RENDER_BEGIN 0x00000000006020C0L

static const int pitch_code_table[33][2] =
{
  {0,    0000},
  {32,   0001},
  {64,   0011},
  {96,   0111},
  {128,  0112},
  {160,  0122},
  {192,  0222},
  {224,  0123},
  {256,  0223},
  {288,  0133},
  {320,  0233},
  {384,  0333},
  {416,  0134},
  {448,  0234},
  {512,  0334},
  {544,  0144},
  {576,  0244},
  {640,  0344},
  {768,  0444},
  {800,  0145},
  {832,  0245},
  {896,  0345},
  {1024, 0445},
  {1056, 0155},
  {1088, 0255},
  {1152, 0355},
  {1280, 0455},
  {1536, 0555},
  {1568, 0156},
  {1600, 0256},
  {1664, 0356},
  {1792, 0456},
  {2048, 0556}
};

/* Structures */

typedef struct {
  video_driver_class_t vo_driver_class;

  xine_t *xine;
  config_values_t *config;

  pthread_mutex_t mutex;
  int instance_count;
} pgx32_driver_class_t;

typedef struct {
  vo_frame_t vo_frame;

  uint32_t *packedbuf, *stripe_dst;
  int width, height, format, pitch, pitch_code, packedlen, lines_remaining;
  double ratio;
} pgx32_frame_t;

typedef struct {   
  vo_driver_t vo_driver;
  vo_scale_t vo_scale;

  pgx32_driver_class_t *class;

  pgx32_frame_t *current;

  int fbfd, fb_width, fb_height, fb_depth, screen_pitch_code;
  uint8_t *vbase;

  Display *display;
  int screen, depth;
  Drawable drawable;
  GC gc;
  Visual *visual;

  int delivered_format, deinterlace_en;
} pgx32_driver_t;

/*
 * Dispose of any allocated image data within a pgx32_frame_t
 */

static void dispose_frame_internals(pgx32_frame_t *frame)
{
  if (frame->vo_frame.base[0]) {
    free(frame->vo_frame.base[0]);
    frame->vo_frame.base[0] = NULL;
  }
  if (frame->vo_frame.base[1]) {
    free(frame->vo_frame.base[1]);
    frame->vo_frame.base[1] = NULL;
  }
  if (frame->vo_frame.base[2]) {
    free(frame->vo_frame.base[2]);
    frame->vo_frame.base[2] = NULL;
  }
  if (frame->packedbuf) {
    free(frame->packedbuf);
    frame->packedbuf = NULL;
  }
}

/*
 * XINE VIDEO DRIVER FUNCTIONS
 */

static void pgx32_frame_proc_frame(vo_frame_t *frame_gen)
{
  pgx32_frame_t *frame = (pgx32_frame_t *)frame_gen;

  frame->vo_frame.proc_called = 1;

  switch (frame->format) {
    case XINE_IMGFMT_YUY2: {
      int i, x;
      uint32_t *src, *dst, pixel;

      src = (uint32_t *)(void *)frame->vo_frame.base[0];
      dst = frame->stripe_dst;

      for (i=0; i<frame->height; i++) {
        for(x=0; x<frame->vo_frame.pitches[0]/4; x++) {
          pixel = *(src++);
          *(dst++) = (pixel >> 16) | ((pixel & 0xffff) << 16);
        }
        dst += (frame->pitch-frame->width)/2;
      }
    }
    break;

    case XINE_IMGFMT_YV12: {
      int i, x;
      uint16_t *yptr, y;
      uint8_t *uptr, *vptr, u, v;
      uint32_t *dst;

      yptr = (uint16_t *)(void *)frame->vo_frame.base[0];
      uptr = frame->vo_frame.base[1];
      vptr = frame->vo_frame.base[2];
      dst = frame->stripe_dst;

      for (i=0; i<frame->height; i++) {
        if (i & 1) {
          uptr -= frame->vo_frame.pitches[1];
          vptr -= frame->vo_frame.pitches[2];
        }

        for(x=0; x<frame->vo_frame.pitches[0]/2; x++) {
          y = *(yptr++);
          u = *(uptr++);
          v = *(vptr++);
          *(dst++) = ((y & 0x00ff) << 24) | (v << 16) | (y & 0xff00) | u;
        }
        dst += (frame->pitch-frame->width)/2;
      }
    }
    break;
  }
}

static void pgx32_frame_proc_slice(vo_frame_t *frame_gen, uint8_t **src)
{
  pgx32_frame_t *frame = (pgx32_frame_t *)frame_gen;

  frame->vo_frame.proc_called = 1;

  switch (frame->format) {
    case XINE_IMGFMT_YUY2: {
      int i, x, len;
      uint32_t *yptr, *dst, pixel;

      yptr = (uint32_t *)(void *)src[0];
      dst = frame->stripe_dst;

      len = (frame->lines_remaining > 16) ? 16 : frame->lines_remaining;
      frame->lines_remaining -= len;

      for (i=0; i<len; i++) {
        for(x=0; x<frame->vo_frame.pitches[0]/4; x++) {
          pixel = *(yptr++);
          *(dst++) = (pixel >> 16) | ((pixel & 0xffff) << 16);
        }
        dst += (frame->pitch-frame->width)/2;
      }

      frame->stripe_dst = dst;
    }
    break;

    case XINE_IMGFMT_YV12: {
      int i, x, len;
      uint16_t *yptr, y;
      uint8_t *uptr, *vptr, u, v;
      uint32_t *dst;

      yptr = (uint16_t *)(void *)src[0];
      uptr = src[1];
      vptr = src[2];
      dst = frame->stripe_dst;

      len = (frame->lines_remaining > 16) ? 16 : frame->lines_remaining;
      frame->lines_remaining -= len;

      for (i=0; i<len; i++) {
        if (i & 1) {
          uptr -= frame->vo_frame.pitches[1];
          vptr -= frame->vo_frame.pitches[2];
        }

        for(x=0; x<frame->vo_frame.pitches[0]/2; x++) {
          y = *(yptr++);
          u = *(uptr++);
          v = *(vptr++);
          *(dst++) = ((y & 0x00ff) << 24) | (v << 16) | (y & 0xff00) | u;
        }
        dst += (frame->pitch-frame->width)/2;
      }

      frame->stripe_dst = dst;
    }
    break;
  }
}

static void pgx32_frame_field(vo_frame_t *frame_gen, int which_field)
{
  /*pgx32_frame_t *frame = (pgx32_frame_t *)frame_gen;*/
}

static void pgx32_frame_dispose(vo_frame_t *frame_gen)
{
  pgx32_frame_t *frame = (pgx32_frame_t *)frame_gen;

  dispose_frame_internals(frame);
  free(frame);
}

static uint32_t pgx32_get_capabilities(vo_driver_t *this_gen)
{
  /*pgx32_driver_t *this = (pgx32_driver_t *)(void *)this_gen;*/

  return VO_CAP_YV12 |
         VO_CAP_YUY2;
}

static vo_frame_t *pgx32_alloc_frame(vo_driver_t *this_gen)
{
  /*pgx32_driver_t *this = (pgx32_driver_t *)(void *)this_gen;*/
  pgx32_frame_t *frame;

  frame = (pgx32_frame_t *) xine_xmalloc(sizeof(pgx32_frame_t));
  if (!frame) {
    return NULL;
  }

  pthread_mutex_init(&frame->vo_frame.mutex, NULL);

  frame->vo_frame.proc_frame = pgx32_frame_proc_frame;
  frame->vo_frame.proc_slice = pgx32_frame_proc_slice;
  frame->vo_frame.field      = pgx32_frame_field;
  frame->vo_frame.dispose    = pgx32_frame_dispose;

  return (vo_frame_t *)frame;
}

static void pgx32_update_frame_format(vo_driver_t *this_gen, vo_frame_t *frame_gen, uint32_t width, uint32_t height, double ratio, int format, int flags)
{
  pgx32_driver_t *this = (pgx32_driver_t *)(void *)this_gen;
  pgx32_frame_t *frame = (pgx32_frame_t *)frame_gen;

  if ((width != frame->width) ||
      (height != frame->height) ||
      (ratio != frame->ratio) ||
      (format != frame->format)) {
    int i, planes;

    dispose_frame_internals(frame);

    frame->width = width;
    frame->height = height;
    frame->ratio = ratio;
    frame->format = format;

    frame->pitch = 2048;
    for (i=0; i<sizeof(pitch_code_table)/sizeof(pitch_code_table[0]); i++) {
      if ((pitch_code_table[i][0] >= frame->width) && (pitch_code_table[i][0] <= frame->pitch)) {
        frame->pitch = pitch_code_table[i][0];
        frame->pitch_code = pitch_code_table[i][1];
      }
    }

    frame->packedlen = frame->pitch * 2 * height;
    if (!(frame->packedbuf = memalign(8, frame->packedlen))) {
      xprintf(this->class->xine, XINE_VERBOSITY_DEBUG, "video_out_pgx32: frame packed buffer malloc failed\n");
      _x_abort();
    }

    planes = 0;

    switch (format) {
      case XINE_IMGFMT_YUY2:
        planes = 1;
        frame->vo_frame.pitches[0] = ((width + 1) / 2) * 4;
        frame->vo_frame.base[0] = memalign(8, frame->vo_frame.pitches[0] * height);
      break;

      case XINE_IMGFMT_YV12:
        planes = 3;
        frame->vo_frame.pitches[0] = ((width + 1) / 2) * 2;
        frame->vo_frame.pitches[1] = (width + 1) / 2;
        frame->vo_frame.pitches[2] = (width + 1) / 2;
        frame->vo_frame.base[0] = memalign(8, frame->vo_frame.pitches[0] * height);
        frame->vo_frame.base[1] = memalign(8, frame->vo_frame.pitches[1] * ((height + 1) / 2));
        frame->vo_frame.base[2] = memalign(8, frame->vo_frame.pitches[2] * ((height + 1) / 2));
      break;
    }

    for (i=0;i<planes;i++) {
      if (!frame->vo_frame.base[i]) {
        xprintf(this->class->xine, XINE_VERBOSITY_DEBUG, "video_out_pgx32: frame plane malloc failed\n");
        _x_abort();
      }
    }
  }

  frame->stripe_dst = frame->packedbuf;
  frame->lines_remaining = frame->height;
}

static void pgx32_display_frame(vo_driver_t *this_gen, vo_frame_t *frame_gen)
{
  pgx32_driver_t *this = (pgx32_driver_t *)(void *)this_gen;
  pgx32_frame_t *frame = (pgx32_frame_t *)frame_gen;
  volatile uint64_t *vregs = (void *)(this->vbase+GFXP_REGSBASE);

  if ((frame->width != this->vo_scale.delivered_width) ||
      (frame->height != this->vo_scale.delivered_height) ||
      (frame->ratio != this->vo_scale.delivered_ratio) ||
      (frame->format != this->delivered_format)) {
    this->vo_scale.delivered_width  = frame->width;
    this->vo_scale.delivered_height = frame->height;
    this->vo_scale.delivered_ratio  = frame->ratio;
    this->delivered_format          = frame->format;

    this->vo_scale.force_redraw = 1;
    _x_vo_scale_compute_ideal_size(&this->vo_scale);
  }

  if (_x_vo_scale_redraw_needed(&this->vo_scale)) {  
    int i;

    _x_vo_scale_compute_output_size(&this->vo_scale);

    XLockDisplay(this->display);
    XSetForeground(this->display, this->gc, BlackPixel(this->display, this->screen));
    for (i=0;i<4;i++) {
      XFillRectangle(this->display, this->drawable, this->gc, this->vo_scale.border[i].x, this->vo_scale.border[i].y, this->vo_scale.border[i].w, this->vo_scale.border[i].h);
    }
    XUnlockDisplay(this->display);
  }

  memcpy((this->vbase+GFXP_VRAM_MMAPLEN)-frame->packedlen, frame->packedbuf, frame->packedlen);

  XLockDisplay(this->display);
  XGrabServer(this->display);
  XSync(this->display, False);

  while(le2me_64(vregs[FIFO_SPACE]) < 34) {}

  vregs[RASTERISER_MODE] = 0;
  vregs[SCISSOR_MODE] = 0;
  vregs[AREA_STIPPLE_MODE] = 0;
  vregs[WINDOW_ORIGIN] = 0;

  vregs[RECT_ORIGIN] = le2me_64((this->vo_scale.gui_win_x + this->vo_scale.output_xoffset) | ((this->vo_scale.gui_win_y + this->vo_scale.output_yoffset) << 16));
  vregs[RECT_SIZE] = le2me_64(this->vo_scale.output_width | (this->vo_scale.output_height << 16));

  vregs[DY] = le2me_64(1 << 16);

  vregs[TEXTURE_ADDR_MODE] = le2me_64(1);
  vregs[SSTART] = 0;
  vregs[DSDX] = le2me_64((frame->width << 20) / this->vo_scale.output_width);
  vregs[DSDY_DOM] = 0;
  vregs[TSTART] = 0;
  vregs[DTDX] = 0;
  vregs[DTDY_DOM] = le2me_64((frame->height << 20) / this->vo_scale.output_height);

  vregs[TEXTURE_BASE_ADDR] = le2me_64((GFXP_VRAM_MMAPLEN-frame->packedlen) >> 1);
  vregs[TEXTURE_MAP_FORMAT] = le2me_64((1 << 19) | frame->pitch_code);

  vregs[TEXTURE_DATA_FORMAT] = le2me_64(0x63);
  vregs[TEXTURE_READ_MODE] = le2me_64((1 << 17) | (11 << 13) | (11 << 9) | 1);
  vregs[TEXTURE_COLOUR_MODE] = le2me_64((0 << 4) | (3 << 1) | 1);

  vregs[SHADING_MODE] = 0;
  vregs[ALPHA_BLENDING_MODE] = 0;
  vregs[DITHERING_MODE] = le2me_64((1 << 10) | 1);
  vregs[LOGICAL_OP_MODE] = 0;
  vregs[STENCIL_MODE] = 0;

  vregs[READ_MODE] = le2me_64(this->screen_pitch_code);
  vregs[WRITE_MODE] = le2me_64(1);
  vregs[WRITE_MASK] = le2me_64(0x00ffffff);

  vregs[YUV_MODE] = le2me_64(1);

  vregs[RENDER] = le2me_64(RENDER_BEGIN);

  vregs[TEXTURE_READ_MODE] = 0;
  vregs[TEXTURE_ADDR_MODE] = 0;
  vregs[DITHERING_MODE] = 0;
  vregs[TEXTURE_COLOUR_MODE] = 0;
  vregs[YUV_MODE] = 0;

  XUngrabServer(this->display);
  XFlush(this->display);
  XUnlockDisplay(this->display);

  if (this->current != NULL) {
    this->current->vo_frame.free(&this->current->vo_frame);
  }
  this->current = frame;
}

#define blend(a, b, trans) (((a)*(trans) + (b)*(15-(trans))) / 15)

static void pgx32_overlay_blend(vo_driver_t *this_gen, vo_frame_t *frame_gen, vo_overlay_t *overlay)
{
  /*pgx32_driver_t *this = (pgx32_driver_t *)(void *)this_gen;*/
  pgx32_frame_t *frame = (pgx32_frame_t *)frame_gen;

  if (overlay->rle) {
    int i, j, x, y, len, width;
    int use_clip_palette;
    uint16_t *dst;
    clut_t clut;
    uint8_t trans;

    dst = (uint16_t *)frame->packedbuf + (overlay->y * frame->pitch) + overlay->x;

    for (i=0, x=0, y=0; i<overlay->num_rle; i++) {
      len = overlay->rle[i].len;

      while (len > 0) {
        use_clip_palette = 0;
        if (len > overlay->width) {
          width = overlay->width;
          len -= overlay->width;
        }
        else {
          width = len;
          len = 0;
        }

        if ((y >= overlay->clip_top) && (y <= overlay->clip_bottom) && (x <= overlay->clip_right)) {
          if ((x < overlay->clip_left) && (x + width - 1 >= overlay->clip_left)) {
            width -= overlay->clip_left - x;
            len += overlay->clip_left - x;
          }
          else if (x > overlay->clip_left)  {
            use_clip_palette = 1;
            if (x + width - 1 > overlay->clip_right) {
              width -= overlay->clip_right - x;
              len += overlay->clip_right - x;
            }
          }
        }

        if (use_clip_palette) {
          clut = *(clut_t *)&overlay->clip_color[overlay->rle[i].color];
          trans = overlay->clip_trans[overlay->rle[i].color];
        }
        else {
          clut = *(clut_t *)&overlay->color[overlay->rle[i].color];
          trans = overlay->trans[overlay->rle[i].color];
        }

        for (j=0; j<width; j++) {
          if ((overlay->x + x + j) & 1) {
            *(dst-1) = (blend(clut.y, (*(dst-1) >> 8), trans) << 8) | blend(clut.cr, (*(dst-1) & 0xff), trans);
          }
          else {
            *(dst+1) = (blend(clut.y, (*(dst+1) >> 8), trans) << 8) | blend(clut.cb, (*(dst+1) & 0xff), trans);
          }
          dst++;
        }

        x += width;
        if (x == overlay->width) {
          x = 0;
          y++;
          dst += frame->pitch - overlay->width;
        }
      }
    }
  }
}

static int pgx32_get_property(vo_driver_t *this_gen, int property)
{
  pgx32_driver_t *this = (pgx32_driver_t *)(void *)this_gen;

  switch (property) {
    case VO_PROP_INTERLACED:
      return this->deinterlace_en;
    break;

    case VO_PROP_ASPECT_RATIO:
      return this->vo_scale.user_ratio;
    break;

    default:
      return 0;
    break;
  }
}

static int pgx32_set_property(vo_driver_t *this_gen, int property, int value)
{
  pgx32_driver_t *this = (pgx32_driver_t *)(void *)this_gen;

  switch (property) {
    case VO_PROP_INTERLACED: {
      this->deinterlace_en = value;
      this->vo_scale.force_redraw = 1;
    }
    break;

    case VO_PROP_ASPECT_RATIO: {
      if (value >= XINE_VO_ASPECT_NUM_RATIOS) {
        value = XINE_VO_ASPECT_AUTO;
      }
      this->vo_scale.user_ratio = value;
      this->vo_scale.force_redraw = 1;
      _x_vo_scale_compute_ideal_size(&this->vo_scale);
    }
    break;
  }
  return value;
}

static void pgx32_get_property_min_max(vo_driver_t *this_gen, int property, int *min, int *max)
{
  /*pgx32_driver_t *this = (pgx32_driver_t *)(void *)this_gen;*/

  switch (property) {
    default:
      *min = 0;
      *max = 0;
    break;
  }
}

static int pgx32_gui_data_exchange(vo_driver_t *this_gen, int data_type, void *data)
{
  pgx32_driver_t *this = (pgx32_driver_t *)(void *)this_gen;

  switch (data_type) {
    case XINE_GUI_SEND_DRAWABLE_CHANGED: {
      XWindowAttributes win_attrs;

      XLockDisplay(this->display);
      this->drawable = (Drawable)data;
      XGetWindowAttributes(this->display, this->drawable, &win_attrs);
      this->depth  = win_attrs.depth;
      this->visual = win_attrs.visual;
      XFreeGC(this->display, this->gc);
      this->gc = XCreateGC(this->display, this->drawable, 0, NULL);
      XUnlockDisplay(this->display);
    }
    break;

    case XINE_GUI_SEND_EXPOSE_EVENT: {
      this->vo_scale.force_redraw = 1;
    }
    break;

    case XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO: {
      x11_rectangle_t *rect = data;
      int x1, y1, x2, y2;

      _x_vo_scale_translate_gui2video(&this->vo_scale, rect->x, rect->y, &x1, &y1);
      _x_vo_scale_translate_gui2video(&this->vo_scale, rect->x + rect->w, rect->y + rect->h, &x2, &y2);

      rect->x = x1;
      rect->y = y1;
      rect->w = x2 - x1;
      rect->h = y2 - y1;
    }
    break;
  }

  return 0;
}

static int pgx32_redraw_needed(vo_driver_t *this_gen)
{
  pgx32_driver_t *this = (pgx32_driver_t *)(void *)this_gen;

  if (_x_vo_scale_redraw_needed(&this->vo_scale)) {  
    this->vo_scale.force_redraw = 1;
    return 1;
  }

  return 0;
}

static void pgx32_dispose(vo_driver_t *this_gen)
{
  pgx32_driver_t *this = (pgx32_driver_t *)(void *)this_gen;

  XLockDisplay (this->display);
  XFreeGC(this->display, this->gc);
  XUnlockDisplay (this->display);

  munmap(this->vbase, GFXP_VRAM_MMAPLEN);
  munmap(this->vbase+GFXP_REGSBASE, GFXP_REGS_MMAPLEN);
  close(this->fbfd);

  pthread_mutex_lock(&this->class->mutex);
  this->class->instance_count--;
  pthread_mutex_unlock(&this->class->mutex);
  
  free(this);
}

/*
 * XINE VIDEO DRIVER CLASS FUNCTIONS
 */

static void pgx32_dispose_class(video_driver_class_t *class_gen)
{
  pgx32_driver_class_t *class = (pgx32_driver_class_t *)(void *)class_gen;

  pthread_mutex_destroy(&class->mutex);
  free(class);
}

static vo_info_t vo_info_pgx32 = {
  10,
  XINE_VISUAL_TYPE_X11
};

static vo_driver_t *pgx32_init_driver(video_driver_class_t *class_gen, const void *visual_gen)
{
  pgx32_driver_class_t *class = (pgx32_driver_class_t *)(void *)class_gen;
  char *devname;
  int fbfd, i;
  struct vis_identifier ident;
  struct fbgattr attr;
  uint8_t *vbase;
  pgx32_driver_t *this;
  XWindowAttributes win_attrs;

  pthread_mutex_lock(&class->mutex);
  if (class->instance_count > 0) {
    pthread_mutex_unlock(&class->mutex);
    return NULL;
  }
  class->instance_count++;
  pthread_mutex_unlock(&class->mutex);

  devname = class->config->register_string(class->config, "video.pgx32_device", "/dev/fb", "name of pgx32 device", NULL, 10, NULL, NULL);
  if ((fbfd = open(devname, O_RDWR)) < 0) {
    xprintf(class->xine, XINE_VERBOSITY_DEBUG, "video_out_pgx32: Error: can't open framebuffer device '%s'\n", devname);
    return NULL;
  }

  if (ioctl(fbfd, VIS_GETIDENTIFIER, &ident) < 0) {
    xprintf(class->xine, XINE_VERBOSITY_DEBUG, "video_out_sunfb: Error: ioctl failed, unable to determine framebuffer type\n");
    close(fbfd);
    return NULL;
  }

  if (strcmp("TSIgfxp", ident.name) != 0) {
    xprintf(class->xine, XINE_VERBOSITY_DEBUG, "video_out_pgx64: Error: '%s' is not a gfxp framebuffer device\n", devname);    
    close(fbfd);
    return NULL;
  }

  if (ioctl(fbfd, FBIOGATTR, &attr) < 0) {
    xprintf(class->xine, XINE_VERBOSITY_DEBUG, "video_out_pgx32: Error: ioctl failed, unable to determine framebuffer characteristics\n");
    close(fbfd);
    return NULL;
  }

  if ((vbase = mmap(0, GFXP_VRAM_MMAPLEN, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0x04000000)) == MAP_FAILED) {
    return 0;
  }
  if (mmap(vbase+GFXP_REGSBASE, GFXP_REGS_MMAPLEN, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fbfd, 0x02000000) == MAP_FAILED) {
    munmap(vbase, GFXP_VRAM_MMAPLEN);
    return 0;
  }

  this = (pgx32_driver_t *)xine_xmalloc(sizeof(pgx32_driver_t));
  if (!this) {
    return NULL;
  }

  this->vo_driver.get_capabilities     = pgx32_get_capabilities;
  this->vo_driver.alloc_frame          = pgx32_alloc_frame;
  this->vo_driver.update_frame_format  = pgx32_update_frame_format;
  this->vo_driver.overlay_begin        = NULL;
  this->vo_driver.overlay_blend        = pgx32_overlay_blend;
  this->vo_driver.overlay_end          = NULL;
  this->vo_driver.display_frame        = pgx32_display_frame;
  this->vo_driver.get_property         = pgx32_get_property;
  this->vo_driver.set_property         = pgx32_set_property;
  this->vo_driver.get_property_min_max = pgx32_get_property_min_max;
  this->vo_driver.gui_data_exchange    = pgx32_gui_data_exchange;
  this->vo_driver.redraw_needed        = pgx32_redraw_needed;
  this->vo_driver.dispose              = pgx32_dispose;

  _x_vo_scale_init(&this->vo_scale, 0, 0, class->config);
  this->vo_scale.user_ratio      = XINE_VO_ASPECT_AUTO;
  this->vo_scale.user_data       = ((x11_visual_t *)visual_gen)->user_data;
  this->vo_scale.frame_output_cb = ((x11_visual_t *)visual_gen)->frame_output_cb;
  this->vo_scale.dest_size_cb    = ((x11_visual_t *)visual_gen)->dest_size_cb;

  this->class = class;

  this->fbfd        = fbfd;
  this->fb_width    = attr.fbtype.fb_width;
  this->fb_height   = attr.fbtype.fb_height;
  this->fb_depth    = attr.fbtype.fb_depth;
  this->vbase       = vbase;

  for (i=0; i<sizeof(pitch_code_table)/sizeof(pitch_code_table[0]); i++) {
    if (pitch_code_table[i][0] == this->fb_width) {
      this->screen_pitch_code = pitch_code_table[i][1];
    }
  }

  this->display  = ((x11_visual_t *)visual_gen)->display;
  this->screen   = ((x11_visual_t *)visual_gen)->screen;
  this->drawable = ((x11_visual_t *)visual_gen)->d;
  this->gc       = XCreateGC(this->display, this->drawable, 0, NULL);

  XGetWindowAttributes(this->display, this->drawable, &win_attrs);
  this->depth  = win_attrs.depth;
  this->visual = win_attrs.visual;

  return (vo_driver_t *)this;
}

static char *pgx32_get_identifier(video_driver_class_t *class_gen)
{
  return "pgx32";
}

static char *pgx32_get_description(video_driver_class_t *class_gen)
{
  return "xine video output plugin for Sun PGX32 framebuffers";
}

static void *pgx32_init_class(xine_t *xine, void *visual_gen)
{
  pgx32_driver_class_t *class;

  class = (pgx32_driver_class_t *)xine_xmalloc(sizeof(pgx32_driver_class_t));
  if (!class) {
    return NULL;
  }

  class->vo_driver_class.open_plugin     = pgx32_init_driver;
  class->vo_driver_class.get_identifier  = pgx32_get_identifier;
  class->vo_driver_class.get_description = pgx32_get_description;
  class->vo_driver_class.dispose         = pgx32_dispose_class;

  class->xine   = xine;
  class->config = xine->config;

  pthread_mutex_init(&class->mutex, NULL);

  return class;
}

plugin_info_t xine_plugin_info[] = {
  {PLUGIN_VIDEO_OUT, 19, "pgx32", XINE_VERSION_CODE, &vo_info_pgx32, pgx32_init_class},
  {PLUGIN_NONE, 0, "", 0, NULL, NULL}
};
