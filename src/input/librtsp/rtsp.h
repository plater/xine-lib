/*
 * Copyright (C) 2002-2003 the xine project
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * a minimalistic implementation of rtsp protocol,
 * *not* RFC 2326 compilant yet.
 */

#ifndef HAVE_RTSP_H
#define HAVE_RTSP_H

/*#include <inttypes.h> */
#include "xine_internal.h"

#ifdef __CYGWIN__
#define uint32_t unsigned int
#define uint16_t unsigned short int
#define uint8_t unsigned char
#endif

/* some codes returned by rtsp_request_* functions */

#define RTSP_STATUS_SET_PARAMETER  10
#define RTSP_STATUS_OK            200

typedef struct rtsp_s rtsp_t;

rtsp_t*  rtsp_connect (xine_stream_t *stream, const char *mrl, const char *user_agent) XINE_MALLOC;

int rtsp_request_options(rtsp_t *s, const char *what);
int rtsp_request_describe(rtsp_t *s, const char *what);
int rtsp_request_setup(rtsp_t *s, const char *what);
int rtsp_request_setparameter(rtsp_t *s, const char *what);
int rtsp_request_play(rtsp_t *s, const char *what);
int rtsp_request_tearoff(rtsp_t *s, const char *what);

int rtsp_send_ok(rtsp_t *s);

int rtsp_read_data(rtsp_t *s, char *buffer, unsigned int size);

char* rtsp_search_answers(rtsp_t *s, const char *tag);
void rtsp_add_to_payload(char **payload, const char *string);

void rtsp_free_answers(rtsp_t *this);

int      rtsp_read (rtsp_t *this, char *data, int len);
void     rtsp_close (rtsp_t *this);

void  rtsp_set_session(rtsp_t *s, const char *id);
char *rtsp_get_session(rtsp_t *s);

char *rtsp_get_mrl(rtsp_t *s);

/*int      rtsp_peek_header (rtsp_t *this, char *data); */

void rtsp_schedule_field(rtsp_t *s, const char *string);
void rtsp_unschedule_field(rtsp_t *s, const char *string);
void rtsp_unschedule_all(rtsp_t *s);

#endif

