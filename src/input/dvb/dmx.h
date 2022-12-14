/*
 * dmx.h
 *
 * Copyright (C) 2000 Marcus Metzler <marcus@convergence.de>
 *                  & Ralph  Metzler <ralph@convergence.de>
                      for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef _DMX_H_
#define _DMX_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#if HAVE_INTTYPES_H
#include <inttypes.h>
#else
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#endif
#endif

#define DMX_FILTER_SIZE 16

typedef enum
{
	DMX_OUT_DECODER, /* Streaming directly to decoder. */
	DMX_OUT_TAP,     /* Output going to a memory buffer */
	                 /* (to be retrieved via the read command).*/
	DMX_OUT_TS_TAP   /* Output multiplexed into a new TS  */
	                 /* (to be retrieved by reading from the */
	                 /* logical DVR device).                 */
} dmx_output_t;


typedef enum
{
	DMX_IN_FRONTEND, /* Input from a front-end device.  */
	DMX_IN_DVR       /* Input from the logical DVR device.  */
} dmx_input_t;


typedef enum
{
        DMX_PES_AUDIO0,
	DMX_PES_VIDEO0,
	DMX_PES_TELETEXT0,
	DMX_PES_SUBTITLE0,
	DMX_PES_PCR0,

        DMX_PES_AUDIO1,
	DMX_PES_VIDEO1,
	DMX_PES_TELETEXT1,
	DMX_PES_SUBTITLE1,
	DMX_PES_PCR1,

        DMX_PES_AUDIO2,
	DMX_PES_VIDEO2,
	DMX_PES_TELETEXT2,
	DMX_PES_SUBTITLE2,
	DMX_PES_PCR2,

        DMX_PES_AUDIO3,
	DMX_PES_VIDEO3,
	DMX_PES_TELETEXT3,
	DMX_PES_SUBTITLE3,
	DMX_PES_PCR3,

	DMX_PES_OTHER
} dmx_pes_type_t;

#define DMX_PES_AUDIO    DMX_PES_AUDIO0
#define DMX_PES_VIDEO    DMX_PES_VIDEO0
#define DMX_PES_TELETEXT DMX_PES_TELETEXT0
#define DMX_PES_SUBTITLE DMX_PES_SUBTITLE0
#define DMX_PES_PCR      DMX_PES_PCR0


typedef enum
{
        DMX_SCRAMBLING_EV,
        DMX_FRONTEND_EV
} dmx_event_t;


typedef enum
{
	DMX_SCRAMBLING_OFF,
	DMX_SCRAMBLING_ON
} dmx_scrambling_status_t;


typedef struct dmx_filter
{
	uint8_t         filter[DMX_FILTER_SIZE];
	uint8_t         mask[DMX_FILTER_SIZE];
	uint8_t         mode[DMX_FILTER_SIZE];
} dmx_filter_t;


struct dmx_sct_filter_params
{
	uint16_t            pid;
	dmx_filter_t        filter;
	uint32_t            timeout;
	uint32_t            flags;
#define DMX_CHECK_CRC       1
#define DMX_ONESHOT         2
#define DMX_IMMEDIATE_START 4
#define DMX_KERNEL_CLIENT   0x8000
};


struct dmx_pes_filter_params
{
	uint16_t            pid;
	dmx_input_t         input;
	dmx_output_t        output;
	dmx_pes_type_t      pes_type;
	uint32_t            flags;
};


struct dmx_event
{
	dmx_event_t         event;
	time_t              timeStamp;
	union
	{
		dmx_scrambling_status_t scrambling;
	} u;
};

typedef struct dmx_caps {
	uint32_t caps;
	int num_decoders;
} dmx_caps_t;

typedef enum {
	DMX_SOURCE_FRONT0 = 0,
	DMX_SOURCE_FRONT1,
	DMX_SOURCE_FRONT2,
	DMX_SOURCE_FRONT3,
	DMX_SOURCE_DVR0   = 16,
	DMX_SOURCE_DVR1,
	DMX_SOURCE_DVR2,
	DMX_SOURCE_DVR3
} dmx_source_t;


#define DMX_START                _IO('o',41)
#define DMX_STOP                 _IO('o',42)
#define DMX_SET_FILTER           _IOW('o',43,struct dmx_sct_filter_params)
#define DMX_SET_PES_FILTER       _IOW('o',44,struct dmx_pes_filter_params)
#define DMX_SET_BUFFER_SIZE      _IO('o',45)
#define DMX_GET_EVENT            _IOR('o',46,struct dmx_event)
#define DMX_GET_PES_PIDS         _IOR('o',47,uint16_t[5])
#define DMX_GET_CAPS             _IOR('o',48,dmx_caps_t)
#define DMX_SET_SOURCE           _IOW('o',49,dmx_source_t)

#endif /*_DMX_H_*/

