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
 * This file contains plugin entries for several demuxers used in games
 *
 * $Id: group_audio.c,v 1.13 2004/05/16 18:01:44 tmattern Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xine_internal.h"
#include "demux.h"

#include "group_audio.h"

/*
 * exported plugin catalog entries
 */

demuxer_info_t demux_info_aac = {
  0                       /* priority */
};

demuxer_info_t demux_info_ac3 = {
  0                       /* priority */
};

demuxer_info_t demux_info_aud = {
  10                      /* priority */
};

demuxer_info_t demux_info_aiff = {
  10                       /* priority */
};

demuxer_info_t demux_info_cdda = {
  10                       /* priority */
};

demuxer_info_t demux_info_mpgaudio = {
  0                       /* priority */
};

demuxer_info_t demux_info_nsf = {
  10                       /* priority */
};

demuxer_info_t demux_info_realaudio = {
  10                       /* priority */
};

demuxer_info_t demux_info_snd = {
  10                       /* priority */
};

demuxer_info_t demux_info_voc = {
  10                       /* priority */
};

demuxer_info_t demux_info_vox = {
  10                       /* priority */
};

demuxer_info_t demux_info_wav = {
  10                       /* priority */
};

#ifdef HAVE_MODPLUG
demuxer_info_t demux_info_mod = {
  10                       /* priority */
};
#endif

plugin_info_t xine_plugin_info[] = {
  /* type, API, "name", version, special_info, init_function */  
  { PLUGIN_DEMUX, 24, "aac",       XINE_VERSION_CODE, &demux_info_aac,       demux_aac_init_plugin },
  { PLUGIN_DEMUX, 24, "ac3",       XINE_VERSION_CODE, &demux_info_ac3,       demux_ac3_init_plugin },
  { PLUGIN_DEMUX, 24, "aud",       XINE_VERSION_CODE, &demux_info_aud,       demux_aud_init_plugin },
  { PLUGIN_DEMUX, 24, "aiff",      XINE_VERSION_CODE, &demux_info_aiff,      demux_aiff_init_plugin },
  { PLUGIN_DEMUX, 24, "cdda",      XINE_VERSION_CODE, &demux_info_cdda,      demux_cdda_init_plugin },
  { PLUGIN_DEMUX, 24, "mp3",       XINE_VERSION_CODE, &demux_info_mpgaudio,  demux_mpgaudio_init_class },
  { PLUGIN_DEMUX, 24, "nsf",       XINE_VERSION_CODE, &demux_info_nsf,       demux_nsf_init_plugin },
  { PLUGIN_DEMUX, 24, "realaudio", XINE_VERSION_CODE, &demux_info_realaudio, demux_realaudio_init_plugin },
  { PLUGIN_DEMUX, 24, "snd",       XINE_VERSION_CODE, &demux_info_snd,       demux_snd_init_plugin },
  { PLUGIN_DEMUX, 24, "voc",       XINE_VERSION_CODE, &demux_info_voc,       demux_voc_init_plugin },
  { PLUGIN_DEMUX, 24, "vox",       XINE_VERSION_CODE, &demux_info_vox,       demux_vox_init_plugin },
  { PLUGIN_DEMUX, 24, "wav",       XINE_VERSION_CODE, &demux_info_wav,       demux_wav_init_plugin },
#ifdef HAVE_MODPLUG
  { PLUGIN_DEMUX, 24, "mod",       XINE_VERSION_CODE, &demux_info_mod,       demux_mod_init_plugin },
#endif
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
