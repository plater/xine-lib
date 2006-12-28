/*
 * Copyright (C) 2006 the xine project
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
 * xine interface to libwavpack by Diego Pettenò <flameeyes@gentoo.org>
 *
 * $Id: demux_wavpack.c,v 1.2 2006/12/26 14:28:48 dgp85 Exp $
 */

#define LOG_MODULE "demux_wavpack"
#define LOG_VERBOSE

#include "xine_internal.h"
#include "xineutils.h"
#include "demux.h"
#include "bswap.h"
#include "group_audio.h"

#include <wavpack/wavpack.h>

typedef struct {
  char idcode[4];         /* This should always be the string "wvpk" */
  uint32_t block_size;    /* Size of the rest of the frame */
  uint16_t wv_version;    /* Version of the wavpack, 0x0403 should be latest */
  uint8_t track;          /* Unused, has to be 0 */
  uint8_t index;          /* Unused, has to be 0 */
  uint32_t file_samples;  /* (uint32_t)-1 if unknown, else the total number
			     of samples for the file */
  uint32_t samples_index; /* Index of the first sample in block, from the
			     start of the file */
  uint32_t samples_count; /* Count of samples in the current frame */
  uint32_t flags;         /* Misc flags */
  uint32_t decoded_crc32; /* CRC32 of the decoded data */
} wvheader_t;

typedef struct {
  demux_plugin_t demux_plugin;
  
  xine_stream_t *stream;
  fifo_buffer_t *audio_fifo;
  input_plugin_t *input;
  int status;

  union {
    wvheader_t wv;
    uint8_t buffer[sizeof(wvheader_t)];
  } header;

  uint32_t current_sample;
  uint32_t samples;
  uint32_t samplerate;
  uint32_t bits_per_sample;
  uint32_t channels;
  unsigned int length;
} demux_wv_t;

typedef struct {
  demux_class_t demux_class;
} demux_wv_class_t;

#ifndef __unused
# ifdef SUPPORT_ATTRIBUTE_UNUSED
#  define __unused __attribute__((unused))
# else
#  define __unused
# endif
#endif

static int32_t xine_input_read_bytes(input_plugin_t *this, void *data, int32_t bcount) {
  return this->read(this, data, bcount);
}

static uint32_t xine_input_get_pos(input_plugin_t *this) {
  return this->get_current_pos(this);
}

static int xine_input_set_pos_abs(input_plugin_t *this, uint32_t pos) {
  return this->seek(this, pos, SEEK_SET);
}

static int xine_input_set_pos_rel(input_plugin_t *this, int32_t delta, int mode) {
  return this->seek(this, delta, mode);
}

static int xine_input_push_back_byte(input_plugin_t *this, int c) {
  if ( this->seek(this, -1, SEEK_CUR) ) {
    return c;
  } else {
    lprintf("xine_input_push_back_byte: unable to seek.\n");
    return EOF;
  }
}

static uint32_t xine_input_get_length(input_plugin_t *this) {
  return this->get_length(this);
}

static int xine_input_can_seek(input_plugin_t *this) {
  return INPUT_IS_SEEKABLE(this);
}

static int32_t xine_input_write_bytes(__unused void *id, __unused void *data, __unused int32_t bcount) {
  lprintf("xine_input_write_bytes: acces is read-only.\n");
  return 0;
}

static const WavpackStreamReader wavpack_input_reader = {
  .read_bytes		= xine_input_read_bytes,
  .get_pos		= xine_input_get_pos,
  .set_pos_abs		= xine_input_set_pos_abs,
  .set_pos_rel		= xine_input_set_pos_rel,
  .push_back_byte	= xine_input_push_back_byte,
  .get_length		= xine_input_get_length,
  .can_seek		= xine_input_can_seek,
  .write_bytes		= xine_input_write_bytes
};

static int open_wv_file(demux_wv_t *this) {
  WavpackContext *ctx = NULL;
  char error[256]; /* Current version of wavpack (4.31) does not write more than this */

  /* Right now we don't support non-seekable streams */
  if (! INPUT_IS_SEEKABLE(this->input) ) {
    lprintf("open_wv_file: non-seekable inputs aren't supported yet.\n");
    return 0;
  }

  /* Read the file header */
  if (_x_demux_read_header(this->input, this->header.buffer, sizeof(wvheader_t)) != sizeof(wvheader_t))
    return 0;

  /* Validate header, we currently support only Wavpack 4 */
  if ( memcmp(this->header.wv.idcode, "wvpk", 4) != 0 || (le2me_16(this->header.wv.wv_version) >> 8) != 4 )
    return 0;

  /* Rewind */
  this->input->seek(this->input, 0 - sizeof(wvheader_t), SEEK_CUR);

  ctx = WavpackOpenFileInputEx(&wavpack_input_reader, this->input, NULL, error, 0, 0);
  if ( ! ctx ) {
    lprintf("xine_open_wavpack_input: unable to open the stream: %s\n", error);
    return 0;
  }

  this->current_sample = 0;
  this->samples = WavpackGetNumSamples(ctx);
  lprintf("number of samples: %u\n", this->samples);
  this->samplerate = WavpackGetSampleRate(ctx);
  lprintf("samplerate: %u Hz\n", this->samplerate);
  this->bits_per_sample = WavpackGetBitsPerSample(ctx);
  lprintf("bits_per_sample: %u\n", this->bits_per_sample);
  this->channels = WavpackGetNumChannels(ctx);
  lprintf("channels: %u\n", this->channels);
  this->length = this->samples / this->samplerate;

  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 1);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_FOURCC, ME_32(this->header.buffer));

  WavpackCloseFile(ctx);
  this->input->seek(this->input, SEEK_SET, 0);

  return 1;
}

static int demux_wv_send_chunk(demux_plugin_t *this_gen) {
  demux_wv_t *this = (demux_wv_t *) this_gen;
  uint32_t bytes_to_read;

  /* Check if we've finished */
  if (this->current_sample >= this->samples) {
    lprintf("all frames read\n");
    this->status = DEMUX_FINISHED;
    return this->status;
  }

  lprintf("current sample: %u\n", this->current_sample);

  /* For some reason, FFmpeg requires to send it the latter 12 bytes of the header.. don't ask! */
  if ( this->input->read(this->input, this->header.buffer, sizeof(wvheader_t)-12) != sizeof(wvheader_t)-12 ) {
      this->status = DEMUX_FINISHED;
      return this->status;
  }

  /* The size of the block is «of course» minus 8, and
     it also includes the size of the header, but we need
     to give FFmpeg the 12 extra bytes (for some reason),
     so the amount of bytes to read is the following.
  */
  bytes_to_read = le2me_32(this->header.wv.block_size) + 8 - (sizeof(wvheader_t)-12);

  lprintf("demux_wavpack: going to read %u bytes.\n", bytes_to_read);

  while(bytes_to_read) {
    off_t bytes_read = 0;
    buf_element_t *buf = NULL;

    /* Get a buffer */
    buf = this->audio_fifo->buffer_pool_alloc(this->audio_fifo);
    buf->type = BUF_AUDIO_WAVPACK;
    buf->pts = 0;
    buf->extra_info->total_time = this->length;
    buf->decoder_flags = 0;

    /* Set normalised position */
    buf->extra_info->input_normpos =
      (int) ((double) this->input->get_current_pos(this->input) * 65535 /
	     this->input->get_length(this->input));

    /* Set time */
    buf->extra_info->input_time = this->current_sample / this->samplerate;

    bytes_read = this->input->read(this->input, buf->content, ( bytes_to_read > buf->max_size ) ? buf->max_size : bytes_to_read);

    buf->size = bytes_read;

    bytes_to_read -= bytes_read;

    if ( bytes_to_read <= 0 )
      buf->decoder_flags |= BUF_FLAG_FRAME_END;
    
    this->audio_fifo->put(this->audio_fifo, buf);
  }

  this->current_sample += this->header.wv.samples_count;

  return this->status;
}

static void demux_wv_send_headers(demux_plugin_t *this_gen) {
  demux_wv_t *this = (demux_wv_t *) this_gen;
  buf_element_t *buf;

  this->audio_fifo  = this->stream->audio_fifo;

  this->status = DEMUX_OK;

  /* Send start buffers */
  _x_demux_control_start(this->stream);

  /* Send header to decoder */
  if (this->audio_fifo) {
    buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
    
    buf->type            = BUF_AUDIO_WAVPACK;
    buf->decoder_flags   = BUF_FLAG_HEADER|BUF_FLAG_STDHEADER|BUF_FLAG_FRAME_END;
    buf->decoder_info[0] = this->input->get_length(this->input);
    buf->decoder_info[1] = this->samplerate;
    buf->decoder_info[2] = this->bits_per_sample;
    buf->decoder_info[3] = this->channels;

    /* Copy the header */
    buf->size = sizeof(xine_waveformatex) + sizeof(wvheader_t);
    memcpy(buf->content+sizeof(xine_waveformatex), this->header.buffer, buf->size);

    this->audio_fifo->put (this->audio_fifo, buf);
  }
}

static int demux_wv_seek (demux_plugin_t *this_gen,
                           off_t start_pos, int start_time, int playing) {
  demux_wv_t *this = (demux_wv_t *) this_gen;

  /* If thread is not running, initialize demuxer */
  if( !playing ) {

    /* send new pts */
    _x_demux_control_newpts(this->stream, 0, 0);

    this->status = DEMUX_OK;
  }

  return this->status;
}

static void demux_wv_dispose (demux_plugin_t *this_gen) {
  demux_wv_t *this = (demux_wv_t *) this_gen;

  free(this);
}

static int demux_wv_get_status (demux_plugin_t *this_gen) {
  demux_wv_t *this = (demux_wv_t *) this_gen;

  return this->status;
}

static int demux_wv_get_stream_length (demux_plugin_t *this_gen) {
//  demux_wv_t *this = (demux_wv_t *) this_gen;

  return 0;
}

static uint32_t demux_wv_get_capabilities(demux_plugin_t *this_gen) {
  return DEMUX_CAP_NOCAP;
}

static int demux_wv_get_optional_data(demux_plugin_t *this_gen,
                                       void *data, int data_type) {
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static demux_plugin_t *open_plugin (demux_class_t *class_gen, xine_stream_t *stream,
				    input_plugin_t *input) {
  demux_wv_t *this;
  this = xine_xmalloc (sizeof (demux_wv_t));
  this->stream = stream;
  this->input = input;

  this->demux_plugin.send_headers      = demux_wv_send_headers;
  this->demux_plugin.send_chunk        = demux_wv_send_chunk;
  this->demux_plugin.seek              = demux_wv_seek;
  this->demux_plugin.dispose           = demux_wv_dispose;
  this->demux_plugin.get_status        = demux_wv_get_status;
  this->demux_plugin.get_stream_length = demux_wv_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_wv_get_capabilities;
  this->demux_plugin.get_optional_data = demux_wv_get_optional_data;
  this->demux_plugin.demux_class       = class_gen;

  this->status = DEMUX_FINISHED;
  switch (stream->content_detection_method) {

  case METHOD_BY_EXTENSION: {
    char *extensions, *mrl;

    mrl = input->get_mrl (input);
    extensions = class_gen->get_extensions (class_gen);

    if (!_x_demux_check_extension (mrl, extensions)) {
      free (this);
      return NULL;
    }
  }
  /* Falling through is intended */

  case METHOD_BY_CONTENT:
  case METHOD_EXPLICIT:
    
    if (!open_wv_file(this)) {
      free (this);
      return NULL;
    }
  
  break;

  default:
    free (this);
    return NULL;
  }

  return &this->demux_plugin;
}

static char *get_description (demux_class_t *this_gen) {
  return "Wavpack demux plugin";
}

static char *get_identifier (demux_class_t *this_gen) {
  return "Wavpack";
}

static char *get_extensions (demux_class_t *this_gen) {
  return "wv";
}

static char *get_mimetypes (demux_class_t *this_gen) {
  return NULL;
}

static void class_dispose (demux_class_t *this_gen) {
  demux_wv_class_t *this = (demux_wv_class_t *) this_gen;

  free (this);
}

static void *demux_wv_init_plugin (xine_t *xine, void *data) {
  demux_wv_class_t *this;
  
  this = xine_xmalloc (sizeof (demux_wv_class_t));

  this->demux_class.open_plugin     = open_plugin;
  this->demux_class.get_description = get_description;
  this->demux_class.get_identifier  = get_identifier;
  this->demux_class.get_mimetypes   = get_mimetypes;
  this->demux_class.get_extensions  = get_extensions;
  this->demux_class.dispose         = class_dispose;

  return this;
}

static const demuxer_info_t demux_info_wv = {
  0			/* priority */
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_DEMUX, 26, "wavpack", XINE_VERSION_CODE, &demux_info_wv, demux_wv_init_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};