/*
 * Copyright (C) 2001-2005 the xine project
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
 * $Id: audio_decoder.c,v 1.29 2006/10/13 21:41:30 dgp85 Exp $
 *
 * xine audio decoder plugin using ffmpeg
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

#define LOG_MODULE "ffmpeg_audio_dec"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "xine_internal.h"
#include "buffer.h"
#include "xineutils.h"
#include "bswap.h"
#include "xine_decoder.h"

#define AUDIOBUFSIZE (64 * 1024)

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


static const ff_codec_t ff_audio_lookup[] = {
  {BUF_AUDIO_WMAV1,      CODEC_ID_WMAV1,          "MS Windows Media Audio 1 (ffmpeg)"},
  {BUF_AUDIO_WMAV2,      CODEC_ID_WMAV2,          "MS Windows Media Audio 2 (ffmpeg)"},
  {BUF_AUDIO_14_4,       CODEC_ID_RA_144,         "Real 14.4 (ffmpeg)"},
  {BUF_AUDIO_28_8,       CODEC_ID_RA_288,         "Real 28.8 (ffmpeg)"},
  {BUF_AUDIO_MPEG,       CODEC_ID_MP3,            "MP3 (ffmpeg)"},
  {BUF_AUDIO_MSADPCM,    CODEC_ID_ADPCM_MS,       "MS ADPCM (ffmpeg)"},
  {BUF_AUDIO_QTIMAADPCM, CODEC_ID_ADPCM_IMA_QT,   "QT IMA ADPCM (ffmpeg)"},
  {BUF_AUDIO_MSIMAADPCM, CODEC_ID_ADPCM_IMA_WAV,  "MS IMA ADPCM (ffmpeg)"},
  {BUF_AUDIO_DK3ADPCM,   CODEC_ID_ADPCM_IMA_DK3,  "Duck DK3 ADPCM (ffmpeg)"},
  {BUF_AUDIO_DK4ADPCM,   CODEC_ID_ADPCM_IMA_DK4,  "Duck DK4 ADPCM (ffmpeg)"},
  {BUF_AUDIO_VQA_IMA,    CODEC_ID_ADPCM_IMA_WS,   "Westwood Studios IMA (ffmpeg)"},
  {BUF_AUDIO_SMJPEG_IMA, CODEC_ID_ADPCM_IMA_SMJPEG, "SMJPEG IMA (ffmpeg)"},
  {BUF_AUDIO_XA_ADPCM,   CODEC_ID_ADPCM_XA,       "CD-ROM/XA ADPCM (ffmpeg)"},
  {BUF_AUDIO_4X_ADPCM,   CODEC_ID_ADPCM_4XM,      "4X ADPCM (ffmpeg)"},
  {BUF_AUDIO_EA_ADPCM,   CODEC_ID_ADPCM_EA,       "Electronic Arts ADPCM (ffmpeg)"},
  {BUF_AUDIO_MULAW,      CODEC_ID_PCM_MULAW,      "mu-law logarithmic PCM (ffmpeg)"},
  {BUF_AUDIO_ALAW,       CODEC_ID_PCM_ALAW,       "A-law logarithmic PCM (ffmpeg)"},
  {BUF_AUDIO_ROQ,        CODEC_ID_ROQ_DPCM,       "RoQ DPCM (ffmpeg)"},
  {BUF_AUDIO_INTERPLAY,  CODEC_ID_INTERPLAY_DPCM, "Interplay DPCM (ffmpeg)"},
  {BUF_AUDIO_MAC3,       CODEC_ID_MACE3,          "MACE 3:1 (ffmpeg)"},
  {BUF_AUDIO_MAC6,       CODEC_ID_MACE6,          "MACE 6:1 (ffmpeg)"},
  {BUF_AUDIO_XAN_DPCM,   CODEC_ID_XAN_DPCM,       "Origin Xan DPCM (ffmpeg)"},
  {BUF_AUDIO_VMD,        CODEC_ID_VMDAUDIO,       "Sierra VMD Audio (ffmpeg)"},
  {BUF_AUDIO_FLAC,       CODEC_ID_FLAC,           "FLAC (ffmpeg)"},
  {BUF_AUDIO_SHORTEN,    CODEC_ID_SHORTEN,        "Shorten (ffmpeg)"},
  {BUF_AUDIO_ALAC,       CODEC_ID_ALAC,           "ALAC (ffmpeg)"},
  {BUF_AUDIO_QDESIGN2,   CODEC_ID_QDM2,           "QDesign (ffmpeg)"},
  {BUF_AUDIO_COOK,       CODEC_ID_COOK,           "RealAudio Cooker (ffmpeg)"},
  {BUF_AUDIO_TRUESPEECH, CODEC_ID_TRUESPEECH,     "TrueSpeech (ffmpeg)"},
  {BUF_AUDIO_TTA,        CODEC_ID_TTA,            "True Audio Lossless (ffmpeg)"},
  {BUF_AUDIO_SMACKER,    CODEC_ID_SMACKAUDIO,     "Smacker (ffmpeg)"},
};


 static void ff_audio_ensure_buffer_size(ff_audio_decoder_t *this, int size) {
  if (size > this->bufsize) {
    this->bufsize = size + size / 2;
    xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
            _("ffmpeg_audio_dec: increasing buffer to %d to avoid overflow.\n"), 
            this->bufsize);
    this->buf = realloc( this->buf, this->bufsize );
  }
}

static void ff_audio_decode_data (audio_decoder_t *this_gen, buf_element_t *buf) {

  ff_audio_decoder_t *this = (ff_audio_decoder_t *) this_gen;
  int bytes_consumed;
  int decode_buffer_size;
  int offset;
  int out;
  audio_buffer_t *audio_buffer;
  int bytes_to_send;

  if ( (buf->decoder_flags & BUF_FLAG_HEADER) &&
      !(buf->decoder_flags & BUF_FLAG_SPECIAL) ) {

    /* accumulate init data */
    ff_audio_ensure_buffer_size(this, this->size + buf->size);
    memcpy(this->buf + this->size, buf->content, buf->size);
    this->size += buf->size;
    
    if (buf->decoder_flags & BUF_FLAG_FRAME_END) {
      size_t i;
      unsigned int codec_type;
      xine_waveformatex *audio_header;
  
      codec_type = buf->type & 0xFFFF0000;
      this->codec = NULL;
  
      for(i = 0; i < sizeof(ff_audio_lookup)/sizeof(ff_codec_t); i++)
        if(ff_audio_lookup[i].type == codec_type) {
	  pthread_mutex_lock (&ffmpeg_lock);
          this->codec = avcodec_find_decoder(ff_audio_lookup[i].id);
	  pthread_mutex_unlock (&ffmpeg_lock);
          _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC,
                           ff_audio_lookup[i].name);
          break;
        }
  
      if (!this->codec) {
        xprintf (this->stream->xine, XINE_VERBOSITY_LOG, 
                 _("ffmpeg_audio_dec: couldn't find ffmpeg decoder for buf type 0x%X\n"),
                 codec_type);
        _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_HANDLED, 0);
        return;
      }
  
      this->context = avcodec_alloc_context();
      
      if(buf->decoder_flags & BUF_FLAG_STDHEADER) {
        this->audio_sample_rate = buf->decoder_info[1];
        this->audio_channels    = buf->decoder_info[3];
      
        if(this->size) {
          audio_header = (xine_waveformatex *)this->buf;
        
          this->context->block_align = audio_header->nBlockAlign;
          this->context->bit_rate    = audio_header->nAvgBytesPerSec * 8;
      
          if(audio_header->cbSize > 0) {
            this->context->extradata = xine_xmalloc(audio_header->cbSize);
            this->context->extradata_size = audio_header->cbSize;
            memcpy( this->context->extradata, 
                    (uint8_t *)audio_header + sizeof(xine_waveformatex),
                    audio_header->cbSize ); 
          }
        }
      } else {
        short *ptr;
        
        switch(codec_type) {
          case BUF_AUDIO_14_4:
            this->audio_sample_rate = 8000;
            this->audio_channels    = 1;
          
            this->context->block_align = 240;
            break;
          case BUF_AUDIO_28_8:
            this->audio_sample_rate = BE_16(&this->buf[0x30]);
            this->audio_channels    = this->buf[0x37];
            /* this->audio_bits = buf->content[0x35] */
  
            this->context->block_align = BE_16(&this->buf[0x2A]);
  
            this->context->extradata_size = 5*sizeof(short);
            this->context->extradata      = xine_xmalloc(this->context->extradata_size);
  
            ptr = (short *) this->context->extradata;
  
            ptr[0] = BE_16(&this->buf[0x2C]); /* subpacket size */
            ptr[1] = BE_16(&this->buf[0x28]); /* subpacket height */
            ptr[2] = BE_16(&this->buf[0x16]); /* subpacket flavour */
            ptr[3] = BE_32(&this->buf[0x18]); /* coded frame size */
            ptr[4] = 0;                          /* codec's data length  */
            break;
          default:
            xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
                    "ffmpeg_audio_dec: unknown header with buf type 0x%X\n", codec_type);
            break;
        }
      }
  
      /* Current ffmpeg audio decoders always use 16 bits/sample 
       * buf->decoder_info[2] can't be used as it doesn't refer to the output
       * bits/sample for some codecs (e.g. MS ADPCM) */
      this->audio_bits = 16;  
  
      this->context->bits_per_sample = this->audio_bits;
      this->context->sample_rate = this->audio_sample_rate;
      this->context->channels    = this->audio_channels;
      this->context->codec_id    = this->codec->id;
      this->context->codec_tag   = _x_stream_info_get(this->stream, XINE_STREAM_INFO_AUDIO_FOURCC);
  
      this->size = 0;
  
      this->decode_buffer = xine_xmalloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
  
      return;
    }
  } else if ((buf->decoder_flags & BUF_FLAG_SPECIAL) &&
             (buf->decoder_info[1] == BUF_SPECIAL_STSD_ATOM)) {

    this->context->extradata_size = buf->decoder_info[2];
    this->context->extradata = xine_xmalloc(buf->decoder_info[2] +
      FF_INPUT_BUFFER_PADDING_SIZE);
    memcpy(this->context->extradata, buf->decoder_info_ptr[2],
      buf->decoder_info[2]);

  } else if (!(buf->decoder_flags & BUF_FLAG_SPECIAL)) {

    if( !this->decoder_ok ) {
      if ( ! this->context || ! this->codec ) {
        xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
		_("ffmpeg_audio_dec: trying to open null codec\n"));
	_x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_HANDLED, 0);
	return;
      }

      pthread_mutex_lock (&ffmpeg_lock);
      if (avcodec_open (this->context, this->codec) < 0) {
	pthread_mutex_unlock (&ffmpeg_lock);
        xprintf (this->stream->xine, XINE_VERBOSITY_LOG, 
                 _("ffmpeg_audio_dec: couldn't open decoder\n"));
        _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_HANDLED, 0);
        return;
      }
      pthread_mutex_unlock (&ffmpeg_lock);  
      this->decoder_ok = 1;
    }

    if (!this->output_open) {
      this->output_open = this->stream->audio_out->open(this->stream->audio_out,
        this->stream, this->audio_bits, this->audio_sample_rate,
        _x_ao_channels2mode(this->audio_channels));
    }

    /* if the audio still isn't open, bail */
    if (!this->output_open)
      return;

    if( buf->decoder_flags & BUF_FLAG_PREVIEW )
      return;

    ff_audio_ensure_buffer_size(this, this->size + buf->size);
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
          xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, 
                   "ffmpeg_audio_dec: error decompressing audio frame\n");
          this->size=0;
          return;
        } else if (bytes_consumed == 0 && decode_buffer_size == 0) {
          if (offset)
            memmove(this->buf, &this->buf[offset], this->size);
          return;
        }

        /* dispatch the decoded audio */
        out = 0;
        while (out < decode_buffer_size) {
          audio_buffer = 
            this->stream->audio_out->get_buffer (this->stream->audio_out);
          if (audio_buffer->mem_size == 0) {
            xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, 
                     "ffmpeg_audio_dec: Help! Allocated audio buffer with nothing in it!\n");
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
  if( this->context && this->decoder_ok ) {
    pthread_mutex_lock (&ffmpeg_lock);
    avcodec_close (this->context);
    avcodec_open (this->context, this->codec);
    pthread_mutex_unlock (&ffmpeg_lock);
  }
}

static void ff_audio_discontinuity (audio_decoder_t *this_gen) {
}

static void ff_audio_dispose (audio_decoder_t *this_gen) {

  ff_audio_decoder_t *this = (ff_audio_decoder_t *) this_gen;
  
  if( this->context && this->decoder_ok ) {
    pthread_mutex_lock (&ffmpeg_lock);
    avcodec_close (this->context);
    pthread_mutex_unlock (&ffmpeg_lock);
  }

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
  this->bufsize = 0;
  this->decoder_ok = 0;
  
  ff_audio_ensure_buffer_size(this, AUDIOBUFSIZE);

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

void *init_audio_plugin (xine_t *xine, void *data) {

  ff_audio_class_t *this ;

  this = (ff_audio_class_t *) xine_xmalloc (sizeof (ff_audio_class_t));

  this->decoder_class.open_plugin     = ff_audio_open_plugin;
  this->decoder_class.get_identifier  = ff_audio_get_identifier;
  this->decoder_class.get_description = ff_audio_get_description;
  this->decoder_class.dispose         = ff_audio_dispose_class;

  pthread_once( &once_control, init_once_routine );

  return this;
}

static uint32_t supported_audio_types[] = { 
  BUF_AUDIO_WMAV1,
  BUF_AUDIO_WMAV2,
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
  BUF_AUDIO_MAC3,
  BUF_AUDIO_MAC6,
  BUF_AUDIO_XAN_DPCM,
  BUF_AUDIO_VMD,
  BUF_AUDIO_EA_ADPCM,
  BUF_AUDIO_SMJPEG_IMA,
  BUF_AUDIO_FLAC,
  BUF_AUDIO_ALAC,
  BUF_AUDIO_SHORTEN,
  BUF_AUDIO_MPEG,
  BUF_AUDIO_QDESIGN2,
  BUF_AUDIO_COOK,
  BUF_AUDIO_TRUESPEECH,
  BUF_AUDIO_TTA,
  BUF_AUDIO_SMACKER,
  0
};

decoder_info_t dec_info_ffmpeg_audio = {
  supported_audio_types,   /* supported types */
  6                        /* priority        */
};
