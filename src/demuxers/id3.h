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
 * ID3 tag parser
 *
 * Supported versions: v1, v1.1, v2.2, v2.3, v2.4
 *
 * $Id: id3.h,v 1.5 2007/03/03 00:58:52 dgp85 Exp $
 */

#ifndef ID3_H
#define ID3_H

#include "xine_internal.h"
#include "xineutils.h"
#include "bswap.h"

/* id3v2 */
#define FOURCC_TAG BE_FOURCC
#define ID3V22_TAG        FOURCC_TAG('I', 'D', '3', 2) /* id3 v2.2 header tag */
#define ID3V23_TAG        FOURCC_TAG('I', 'D', '3', 3) /* id3 v2.3 header tag */
#define ID3V24_TAG        FOURCC_TAG('I', 'D', '3', 4) /* id3 v2.4 header tag */
#define ID3V24_FOOTER_TAG FOURCC_TAG('3', 'D', 'I', 0) /* id3 v2.4 footer tag */


/*
 *  ID3 v2.2
 */
/* tag header */
#define ID3V22_UNSYNCH_FLAG               0x80
#define ID3V22_COMPRESS_FLAG              0x40
#define ID3V22_ZERO_FLAG                  0x3F

/* frame header */
#define ID3V22_FRAME_HEADER_SIZE             6

/*
 *  ID3 v2.3
 */
/* tag header */
#define ID3V23_UNSYNCH_FLAG               0x80
#define ID3V23_EXT_HEADER_FLAG            0x40
#define ID3V23_EXPERIMENTAL_FLAG          0x20
#define ID3V23_ZERO_FLAG                  0x1F

/* frame header */
#define ID3V23_FRAME_HEADER_SIZE            10
#define ID3V23_FRAME_TAG_PRESERV_FLAG   0x8000
#define ID3V23_FRAME_FILE_PRESERV_FLAG  0x4000
#define ID3V23_FRAME_READ_ONLY_FLAG     0x2000
#define ID3V23_FRAME_COMPRESS_FLAG      0x0080
#define ID3V23_FRAME_ENCRYPT_FLAG       0x0040
#define ID3V23_FRAME_GROUP_ID_FLAG      0x0020
#define ID3V23_FRAME_ZERO_FLAG          0x1F1F

/*
 *  ID3 v2.4
 */
/* tag header */
#define ID3V24_UNSYNCH_FLAG               0x80
#define ID3V24_EXT_HEADER_FLAG            0x40
#define ID3V24_EXPERIMENTAL_FLAG          0x20
#define ID3V24_FOOTER_FLAG                0x10
#define ID3V24_ZERO_FLAG                  0x0F

/* extended header */
#define ID3V24_EXT_UPDATE_FLAG            0x40
#define ID3V24_EXT_CRC_FLAG               0x20
#define ID3V24_EXT_RESTRICTIONS_FLAG      0x10
#define ID3V24_EXT_ZERO_FLAG              0x8F

/* frame header */
#define ID3V24_FRAME_HEADER_SIZE            10
#define ID3V24_FRAME_TAG_PRESERV_FLAG   0x4000
#define ID3V24_FRAME_FILE_PRESERV_FLAG  0x2000
#define ID3V24_FRAME_READ_ONLY_FLAG     0x1000
#define ID3V24_FRAME_GROUP_ID_FLAG      0x0040
#define ID3V24_FRAME_COMPRESS_FLAG      0x0008
#define ID3V24_FRAME_ENCRYPT_FLAG       0x0004
#define ID3V24_FRAME_UNSYNCH_FLAG       0x0002
#define ID3V24_FRAME_DATA_LEN_FLAG      0x0001
#define ID3V24_FRAME_ZERO_FLAG          0x8FB0

/* footer */
#define ID3V24_FOOTER_SIZE                  10


typedef struct {
  uint32_t  id;
  uint8_t   revision;
  uint8_t   flags;
  uint32_t  size;
} id3v2_header_t;

typedef struct {
  uint32_t  id;
  uint32_t  size;
} id3v22_frame_header_t;

typedef struct {
  uint32_t  id;
  uint32_t  size;
  uint16_t  flags;
} id3v23_frame_header_t;

typedef struct {
  uint32_t  size;
  uint16_t  flags;
  uint32_t  padding_size;
  uint32_t  crc;
} id3v23_frame_ext_header_t;

typedef id3v2_header_t id3v24_footer_t;

typedef struct {
  uint32_t  id;
  uint32_t  size;
  uint16_t  flags;
} id3v24_frame_header_t;

typedef struct {
  uint32_t  size;
  uint8_t   flags;
  uint32_t  crc;
  uint8_t   restrictions;
} id3v24_frame_ext_header_t;

typedef struct {
  char    tag[3];
  char    title[30];
  char    artist[30];
  char    album[30];
  char    year[4];
  char    comment[30];
  uint8_t genre;
} id3v1_tag_t;

int id3v1_parse_tag (input_plugin_t *input, xine_stream_t *stream);

int id3v22_parse_tag(input_plugin_t *input,
                     xine_stream_t *stream,
                     int8_t *mp3_frame_header);

int id3v23_parse_tag(input_plugin_t *input,
                     xine_stream_t *stream,
                     int8_t *mp3_frame_header);

int id3v24_parse_tag(input_plugin_t *input,
                     xine_stream_t *stream,
                     int8_t *mp3_frame_header);

/* Generic function that switch between the three above */
int id3v2_parse_tag(input_plugin_t *input,
		    xine_stream_t *stream,
		    int8_t *mp3_frame_header);

#endif /* ID3_H */
