/*
    $Id: files.c,v 1.4 2006/09/26 21:18:18 dgp85 Exp $

    Copyright (C) 2000, 2004 Herbert Valerio Riedel <hvr@gnu.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include <cdio/cdio.h>
#include <cdio/bytesex.h>
#include <cdio/util.h>

/* Public headers */
#include <libvcd/files.h>
#include <libvcd/types.h>
#include <libvcd/logging.h>

/* FIXME! Make this local */
#include <libvcd/files_private.h>

/* Private headers */
#include "vcd_assert.h"
#include "mpeg_stream.h"
#include "obj.h"
#include "pbc.h"
#include "util.h"

static const char _rcsid[] = "$Id: files.c,v 1.4 2006/09/26 21:18:18 dgp85 Exp $";

inline static bool
_pal_p (const struct vcd_mpeg_stream_vid_info *_info)
{
  return (_info->vsize == 288 || _info->vsize == 576);
}

static int
_derive_vid_type (const struct vcd_mpeg_stream_info *_info, bool svcd)
{
  if (_info->shdr[0].seen)
    return _pal_p (&_info->shdr[0]) ? 0x7 : 0x3;

  if (_info->shdr[2].seen)
    {
      if (svcd)
        vcd_warn ("stream with 0xE2 still stream id not allowed for IEC62107 compliant SVCDs");
      return _pal_p (&_info->shdr[2]) ? 0x6 : 0x2;
    }

  if (_info->shdr[1].seen)
    return _pal_p (&_info->shdr[1]) ? 0x5 : 0x1;

  return 0;
}

static int
_derive_ogt_type (const struct vcd_mpeg_stream_info *_info, bool svcd)
{
  
  if (!svcd)
    return 0;

  if ((_info->ogt[3] || _info->ogt[2])
      && _info->ogt[1] && _info->ogt[0])
    return 0x3;

  if (_info->ogt[1] && _info->ogt[0])
    return 0x2;

  if (_info->ogt[0])
    return 0x1;

  vcd_debug ("OGT streams available: %d %d %d %d",
             _info->ogt[0], _info->ogt[1], 
             _info->ogt[2], _info->ogt[3]);

  return 0x0;
}

static int
_derive_aud_type (const struct vcd_mpeg_stream_info *_info, bool svcd)
{
  if (!_info->ahdr[0].seen)
    return 0; /* no MPEG audio */

  if (svcd)
    {
      if (_info->ahdr[2].seen)
        return 3; /* MC */

      if (_info->ahdr[1].seen)
        return 2; /* 2 streams */
      
      return 1; /* just one stream */
    }
  else
    switch (_info->ahdr[0].mode)
      {
      case MPEG_SINGLE_CHANNEL:
        return 1;
        break;

      case MPEG_STEREO:
      case MPEG_JOINT_STEREO:
        return 2;
        break;

      case MPEG_DUAL_CHANNEL:
        return 3;
        break;
      }

  return 0;
}

void
set_entries_vcd (VcdObj *obj, void *buf)
{
  CdioListNode *node = NULL;
  int idx = 0;
  int track_idx = 0;
  EntriesVcd_t entries_vcd;

  vcd_assert (sizeof(EntriesVcd_t) == 2048);

  vcd_assert (_cdio_list_length (obj->mpeg_track_list) <= MAX_ENTRIES);
  vcd_assert (_cdio_list_length (obj->mpeg_track_list) > 0);

  memset(&entries_vcd, 0, sizeof(entries_vcd)); /* paranoia / fixme */

  switch (obj->type)
    {
    case VCD_TYPE_VCD:
      strncpy(entries_vcd.ID, ENTRIES_ID_VCD, 8);
      entries_vcd.version = ENTRIES_VERSION_VCD;
      entries_vcd.sys_prof_tag = ENTRIES_SPTAG_VCD;
      break;

    case VCD_TYPE_VCD11:
      strncpy(entries_vcd.ID, ENTRIES_ID_VCD, 8);
      entries_vcd.version = ENTRIES_VERSION_VCD11;
      entries_vcd.sys_prof_tag = ENTRIES_SPTAG_VCD11;
      break;

    case VCD_TYPE_VCD2:
      strncpy(entries_vcd.ID, ENTRIES_ID_VCD, 8);
      entries_vcd.version = ENTRIES_VERSION_VCD2;
      entries_vcd.sys_prof_tag = ENTRIES_SPTAG_VCD2;
      break;

    case VCD_TYPE_SVCD:
      if (!obj->svcd_vcd3_entrysvd)
        strncpy(entries_vcd.ID, ENTRIES_ID_SVCD, 8);
      else
        {
          vcd_warn ("setting ENTRYSVD signature for *DEPRECATED* VCD 3.0 type SVCD");
          strncpy(entries_vcd.ID, ENTRIES_ID_VCD3, 8);
        }
      entries_vcd.version = ENTRIES_VERSION_SVCD;
      entries_vcd.sys_prof_tag = ENTRIES_SPTAG_SVCD;
      break;

    case VCD_TYPE_HQVCD:
      strncpy(entries_vcd.ID, ENTRIES_ID_SVCD, 8);
      entries_vcd.version = ENTRIES_VERSION_HQVCD;
      entries_vcd.sys_prof_tag = ENTRIES_SPTAG_HQVCD;
      break;
      
    default:
      vcd_assert_not_reached ();
      break;
    }

  idx = 0;
  track_idx = 2;
  _CDIO_LIST_FOREACH (node, obj->mpeg_sequence_list)
    {
      mpeg_sequence_t *track = _cdio_list_node_data (node);
      uint32_t lsect = track->relative_start_extent;
      CdioListNode *node2;

      lsect += obj->iso_size;

      entries_vcd.entry[idx].n = cdio_to_bcd8(track_idx);
      cdio_lba_to_msf(cdio_lsn_to_lba(lsect), 
                      &(entries_vcd.entry[idx].msf));

      idx++;
      lsect += obj->track_front_margin;

      _CDIO_LIST_FOREACH (node2, track->entry_list)
        {
          entry_t *_entry = _cdio_list_node_data (node2);
          /* additional entries */

          vcd_assert (idx < MAX_ENTRIES);

          entries_vcd.entry[idx].n = cdio_to_bcd8(track_idx);
          cdio_lba_to_msf(lsect + cdio_lsn_to_lba(_entry->aps.packet_no),
                          &(entries_vcd.entry[idx].msf));

          idx++;
        }

      track_idx++;
    }

  entries_vcd.entry_count = uint16_to_be (idx);

  memcpy(buf, &entries_vcd, sizeof(entries_vcd));
}

static void
_set_bit (uint8_t bitset[], unsigned bitnum)
{
  unsigned _byte = bitnum / 8;
  unsigned _bit  = bitnum % 8;

  bitset[_byte] |= (1 << _bit);
}

uint32_t 
get_psd_size (VcdObj *obj, bool extended)
{
  if (extended)
    vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_PBC_X));

  if (!_vcd_pbc_available (obj))
    return 0;
  
  if (extended)
    return obj->psdx_size;

  return obj->psd_size;
}

void
set_psd_vcd (VcdObj *obj, void *buf, bool extended)
{
  CdioListNode *node;

  if (extended)
    vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_PBC_X));

  vcd_assert (_vcd_pbc_available (obj));

  _CDIO_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _cdio_list_node_data (node);
      char *_buf = buf;
      unsigned offset = (extended ? _pbc->offset_ext : _pbc->offset);
      
      vcd_assert (offset % INFO_OFFSET_MULT == 0);

      _vcd_pbc_node_write (obj, _pbc, _buf + offset, extended);
    }
}

void
set_lot_vcd(VcdObj *obj, void *buf, bool extended)
{
  LotVcd_t *lot_vcd = NULL;
  CdioListNode *node;

  if (extended)
    vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_PBC_X));

  vcd_assert (_vcd_pbc_available (obj));

  lot_vcd = _vcd_malloc (sizeof (LotVcd_t));
  memset(lot_vcd, 0xff, sizeof(LotVcd_t));

  lot_vcd->reserved = 0x0000;

  _CDIO_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _cdio_list_node_data (node);
      unsigned int offset = extended ? _pbc->offset_ext : _pbc->offset;
      
      vcd_assert (offset % INFO_OFFSET_MULT == 0);

      if (_pbc->rejected)
        continue;

      offset /= INFO_OFFSET_MULT;

      lot_vcd->offset[_pbc->lid - 1] = uint16_to_be (offset);
    }

  memcpy(buf, lot_vcd, sizeof(LotVcd_t));
  free(lot_vcd);
}

void
set_info_vcd(VcdObj *obj, void *buf)
{
  InfoVcd_t info_vcd;
  CdioListNode *node = NULL;
  int n = 0;

  vcd_assert (sizeof (InfoVcd_t) == 2048);
  vcd_assert (_cdio_list_length (obj->mpeg_track_list) <= 98);
  
  memset (&info_vcd, 0, sizeof (info_vcd));

  switch (obj->type)
    {
    case VCD_TYPE_VCD:
      strncpy (info_vcd.ID, INFO_ID_VCD, sizeof (info_vcd.ID));
      info_vcd.version = INFO_VERSION_VCD;
      info_vcd.sys_prof_tag = INFO_SPTAG_VCD;
      break;

    case VCD_TYPE_VCD11:
      strncpy (info_vcd.ID, INFO_ID_VCD, sizeof (info_vcd.ID));
      info_vcd.version = INFO_VERSION_VCD11;
      info_vcd.sys_prof_tag = INFO_SPTAG_VCD11;
      break;

    case VCD_TYPE_VCD2:
      strncpy (info_vcd.ID, INFO_ID_VCD, sizeof (info_vcd.ID));
      info_vcd.version = INFO_VERSION_VCD2;
      info_vcd.sys_prof_tag = INFO_SPTAG_VCD2;
      break;

    case VCD_TYPE_SVCD:
      strncpy (info_vcd.ID, INFO_ID_SVCD, sizeof (info_vcd.ID));
      info_vcd.version = INFO_VERSION_SVCD;
      info_vcd.sys_prof_tag = INFO_SPTAG_SVCD;
      break;

    case VCD_TYPE_HQVCD:
      strncpy (info_vcd.ID, INFO_ID_HQVCD, sizeof (info_vcd.ID));
      info_vcd.version = INFO_VERSION_HQVCD;
      info_vcd.sys_prof_tag = INFO_SPTAG_HQVCD;
      break;
      
    default:
      vcd_assert_not_reached ();
      break;
    }
  
  iso9660_strncpy_pad (info_vcd.album_desc, 
                       obj->info_album_id,
                       sizeof(info_vcd.album_desc), ISO9660_DCHARS); 
  /* fixme, maybe it's VCD_ACHARS? */

  info_vcd.vol_count = uint16_to_be (obj->info_volume_count);
  info_vcd.vol_id = uint16_to_be (obj->info_volume_number);

  if (_vcd_obj_has_cap_p (obj, _CAP_PAL_BITS))
    {
      /* NTSC/PAL bitset */

      n = 0;
      _CDIO_LIST_FOREACH (node, obj->mpeg_track_list)
        {
          mpeg_track_t *track = _cdio_list_node_data (node);
          
          const struct vcd_mpeg_stream_vid_info *_info = &track->info->shdr[0];

          if (vcd_mpeg_get_norm (_info) == MPEG_NORM_PAL
              || vcd_mpeg_get_norm (_info) == MPEG_NORM_PAL_S)
            _set_bit(info_vcd.pal_flags, n);
          else if (_pal_p (_info))
            {
              vcd_warn ("INFO.{VCD,SVD}: assuming PAL-type resolution for track #%d"
                        " -- are we creating a X(S)VCD?", n);
              _set_bit(info_vcd.pal_flags, n);
            }
        
          n++;
        }
    }

  if (_vcd_obj_has_cap_p (obj, _CAP_PBC))
    {
      info_vcd.flags.restriction = obj->info_restriction;
      info_vcd.flags.use_lid2 = obj->info_use_lid2;
      info_vcd.flags.use_track3 = obj->info_use_seq2;

      if (_vcd_obj_has_cap_p (obj, _CAP_PBC_X)
          &&_vcd_pbc_available (obj))
        info_vcd.flags.pbc_x = true;
      
      info_vcd.psd_size = uint32_to_be (get_psd_size (obj, false));
      info_vcd.offset_mult = _vcd_pbc_available (obj) ? INFO_OFFSET_MULT : 0;
      info_vcd.lot_entries = uint16_to_be (_vcd_pbc_max_lid (obj));
      
      if (_cdio_list_length (obj->mpeg_segment_list))
        {
          unsigned segments = 0;
        
          if (!_vcd_pbc_available (obj))
            vcd_warn ("segment items available, but no PBC items set!"
                      " SPIs will be unreachable");

          _CDIO_LIST_FOREACH (node, obj->mpeg_segment_list)
            {
              mpeg_segment_t *segment = _cdio_list_node_data (node);
              unsigned idx;
              InfoSpiContents contents = { 0, };

              contents.video_type = 
                _derive_vid_type (segment->info,
                                  _vcd_obj_has_cap_p (obj, _CAP_4C_SVCD));

              contents.audio_type = 
                _derive_aud_type (segment->info,
                                  _vcd_obj_has_cap_p (obj, _CAP_4C_SVCD));

              contents.ogt =
                _derive_ogt_type (segment->info,
                                  _vcd_obj_has_cap_p (obj, _CAP_4C_SVCD));

              if (!contents.video_type && !contents.audio_type)
                vcd_warn ("segment item '%s' seems contains neither video nor audio",
                          segment->id);

              for (idx = 0; idx < segment->segment_count; idx++)
                {
                  vcd_assert (segments + idx < MAX_SEGMENTS);

                  info_vcd.spi_contents[segments + idx] = contents;
                
                  if (!contents.item_cont)
                    contents.item_cont = true;
                }

              segments += idx;
            }

          info_vcd.item_count = uint16_to_be (segments); 

          cdio_lba_to_msf (cdio_lsn_to_lba(obj->mpeg_segment_start_extent), 
                           &info_vcd.first_seg_addr);
        }
    }

  memcpy(buf, &info_vcd, sizeof(info_vcd));
}

static void
set_tracks_svd_v30 (VcdObj *obj, void *buf)
{
  char tracks_svd_buf[ISO_BLOCKSIZE] = { 0, };
  TracksSVD_v30 *tracks_svd = (void *) tracks_svd_buf;
  CdioListNode *node;
  double playtime;
  int n;

  strncpy (tracks_svd->file_id, TRACKS_SVD_FILE_ID, 
           sizeof (TRACKS_SVD_FILE_ID)-1);
  tracks_svd->version = TRACKS_SVD_VERSION;
  tracks_svd->tracks = _cdio_list_length (obj->mpeg_track_list);

  n = 0;
  playtime = 0;
  _CDIO_LIST_FOREACH (node, obj->mpeg_track_list)
    {
      mpeg_track_t *track = _cdio_list_node_data (node);
      int i;

      playtime += track->info->playing_time;

      tracks_svd->track[n].audio_info = track->info->ahdr[0].seen ? 0x2 : 0x0; /* fixme */
      tracks_svd->track[n].audio_info |= track->info->ahdr[1].seen ? 0x20 : 0x0; /* fixme */

      tracks_svd->track[n].ogt_info = 0x0;
      for (i = 0; i < 4; i++)
        if (track->info->ogt[i])
          tracks_svd->track[n].ogt_info |= 1 << (i * 2); /* fixme */

      /* setting playtime */
      
      {
        double i, f;

        while (playtime >= 6000.0)
          playtime -= 6000.0;

        f = modf(playtime, &i);
        
        cdio_lba_to_msf (i * 75, &tracks_svd->track[n].cum_playing_time);
        tracks_svd->track[n].cum_playing_time.f = 
          cdio_to_bcd8 (floor (f * 75.0));
      }
      
      n++;
    }  
  
  memcpy (buf, &tracks_svd_buf, sizeof(tracks_svd_buf));
}

void
set_tracks_svd (VcdObj *obj, void *buf)
{
  char tracks_svd[ISO_BLOCKSIZE] = { 0, };
  TracksSVD *tracks_svd1 = (void *) tracks_svd;
  TracksSVD2 *tracks_svd2;
  CdioListNode *node;
  int n;

  vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_4C_SVCD));

  if (obj->svcd_vcd3_tracksvd)
    {
      set_tracks_svd_v30 (obj, buf);
      return;
    }

  vcd_assert (sizeof (SVDTrackContent) == 1);

  strncpy (tracks_svd1->file_id, TRACKS_SVD_FILE_ID, sizeof (TRACKS_SVD_FILE_ID)-1);
  tracks_svd1->version = TRACKS_SVD_VERSION;

  tracks_svd1->tracks = _cdio_list_length (obj->mpeg_track_list);

  tracks_svd2 = (void *) &(tracks_svd1->playing_time[tracks_svd1->tracks]);

  n = 0;

  _CDIO_LIST_FOREACH (node, obj->mpeg_track_list)
    {
      mpeg_track_t *track = _cdio_list_node_data (node);
      const double playtime = track->info->playing_time;

      int _video;
     
      _video = tracks_svd2->contents[n].video =
        _derive_vid_type (track->info, true);

      tracks_svd2->contents[n].audio =
        _derive_aud_type (track->info, true);

      tracks_svd2->contents[n].ogt = 
        _derive_ogt_type (track->info, true);

      if (_video != 0x3 && _video != 0x7)
        vcd_warn("SVCD/TRACKS.SVCD: No MPEG motion video for track #%d?", n);

      /* setting playtime */
      
      {
        double i, f;

        f = modf(playtime, &i);

        if (playtime >= 6000.0)
          {
            vcd_warn ("SVCD/TRACKS.SVD: playing time value (%d seconds) to great,"
                      " clipping to 100 minutes", (int) i);
            i = 5999.0;
            f = 74.0 / 75.0;
          }

        cdio_lba_to_msf (i * 75, &(tracks_svd1->playing_time[n]));
        tracks_svd1->playing_time[n].f = cdio_to_bcd8 (floor (f * 75.0));
      }
      
      n++;
    }  
  
  memcpy (buf, &tracks_svd, sizeof(tracks_svd));
}

static double
_get_cumulative_playing_time (const VcdObj *obj, unsigned up_to_track_no)
{
  double result = 0;
  CdioListNode *node;

  _CDIO_LIST_FOREACH (node, obj->mpeg_track_list)
    {
      mpeg_track_t *track = _cdio_list_node_data (node);

      if (!up_to_track_no)
        break;

      result += track->info->playing_time;
      up_to_track_no--;
    }
  
  if (up_to_track_no)
    vcd_warn ("internal error...");

  return result;
}

static unsigned 
_get_scanpoint_count (const VcdObj *obj)
{
  double total_playing_time;

  total_playing_time = _get_cumulative_playing_time (obj, _cdio_list_length (obj->mpeg_track_list));

  return ceil (total_playing_time * 2.0);
}

uint32_t 
get_search_dat_size (const VcdObj *obj)
{
  return sizeof (SearchDat) 
    + (_get_scanpoint_count (obj) * sizeof (msf_t));
}

static CdioList *
_make_track_scantable (const VcdObj *obj)
{
  CdioList *all_aps = _cdio_list_new ();
  CdioList *scantable = _cdio_list_new ();
  unsigned scanpoints = _get_scanpoint_count (obj);
  unsigned track_no;
  CdioListNode *node;

  track_no = 0;
  _CDIO_LIST_FOREACH (node, obj->mpeg_track_list)
    {
      mpeg_track_t *track = _cdio_list_node_data (node);
      CdioListNode *node2;
      
      _CDIO_LIST_FOREACH (node2, track->info->shdr[0].aps_list)
        {
          struct aps_data *_data = _vcd_malloc (sizeof (struct aps_data));
          
          *_data = *(struct aps_data *)_cdio_list_node_data (node2);

          _data->timestamp += _get_cumulative_playing_time (obj, track_no);
          _data->packet_no += obj->iso_size + track->relative_start_extent;
          _data->packet_no += obj->track_front_margin;

          _cdio_list_append (all_aps, _data);
        }
      track_no++;
    }
  
  {
    CdioListNode *aps_node = _cdio_list_begin (all_aps);
    CdioListNode *n;
    struct aps_data *_data;
    double aps_time;
    double playing_time;
    int aps_packet;
    double t;

    playing_time = scanpoints;
    playing_time /= 2;

    vcd_assert (aps_node != NULL);

    _data = _cdio_list_node_data (aps_node);
    aps_time = _data->timestamp;
    aps_packet = _data->packet_no;

    for (t = 0; t < playing_time; t += 0.5)
      {
	for(n = _cdio_list_node_next (aps_node); n; 
            n = _cdio_list_node_next (n))
	  {
	    _data = _cdio_list_node_data (n);

	    if (fabs (_data->timestamp - t) < fabs (aps_time - t))
	      {
		aps_node = n;
		aps_time = _data->timestamp;
		aps_packet = _data->packet_no;
	      }
	    else 
	      break;
	  }

        {
          uint32_t *lsect = _vcd_malloc (sizeof (uint32_t));
          
          *lsect = aps_packet;
          _cdio_list_append (scantable, lsect);
        }
        
      }

  }

  _cdio_list_free (all_aps, true);

  vcd_assert (scanpoints == _cdio_list_length (scantable));

  return scantable;
}

void
set_search_dat (VcdObj *obj, void *buf)
{
  CdioList *scantable;
  CdioListNode *node;
  SearchDat search_dat;
  unsigned n;

  vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_4C_SVCD));
  /* vcd_assert (sizeof (SearchDat) == ?) */

  memset (&search_dat, 0, sizeof (search_dat));

  strncpy (search_dat.file_id, SEARCH_FILE_ID, sizeof (SEARCH_FILE_ID)-1);
  
  search_dat.version = SEARCH_VERSION;
  search_dat.scan_points = uint16_to_be (_get_scanpoint_count (obj));
  search_dat.time_interval = SEARCH_TIME_INTERVAL;

  memcpy (buf, &search_dat, sizeof (search_dat));
  
  scantable = _make_track_scantable (obj);

  n = 0;
  _CDIO_LIST_FOREACH (node, scantable)
    {
      SearchDat *search_dat2 = buf;
      uint32_t sect = *(uint32_t *) _cdio_list_node_data (node);
          
      cdio_lba_to_msf(cdio_lsn_to_lba(sect), &(search_dat2->points[n]));
      n++;
    }

  vcd_assert (n = _get_scanpoint_count (obj));

  _cdio_list_free (scantable, true);
}

static uint32_t 
_get_scandata_count (const struct vcd_mpeg_stream_info *info)
{ 
  return ceil (info->playing_time * 2.0);
}

static uint32_t *
_get_scandata_table (const struct vcd_mpeg_stream_info *info)
{
  CdioListNode *n, *aps_node = _cdio_list_begin (info->shdr[0].aps_list);
  struct aps_data *_data;
  double aps_time, t;
  int aps_packet;
  uint32_t *retval;
  unsigned i;
  
  retval = _vcd_malloc (_get_scandata_count (info) * sizeof (uint32_t));

  _data = _cdio_list_node_data (aps_node);
  aps_time = _data->timestamp;
  aps_packet = _data->packet_no;

  for (t = 0, i = 0; t < info->playing_time; t += 0.5, i++)
    {
      for(n = _cdio_list_node_next (aps_node); n; n = _cdio_list_node_next (n))
        {
          _data = _cdio_list_node_data (n);

          if (fabs (_data->timestamp - t) < fabs (aps_time - t))
            {
              aps_node = n;
              aps_time = _data->timestamp;
              aps_packet = _data->packet_no;
            }
          else 
            break;
        }

      /* vcd_debug ("%f %f %d", t, aps_time, aps_packet); */

      vcd_assert (i < _get_scandata_count (info));

      retval[i] = aps_packet;
    }

  vcd_assert (i = _get_scandata_count (info));

  return retval;
}

uint32_t 
get_scandata_dat_size (const VcdObj *obj)
{
  uint32_t retval = 0;

  /* struct 1 */
  retval += sizeof (ScandataDat1);
  retval += sizeof (msf_t) * _cdio_list_length (obj->mpeg_track_list);

  /* struct 2 */
  /* vcd_assert (sizeof (ScandataDat2) == 0);
     retval += sizeof (ScandataDat2); */
  retval += sizeof (uint16_t) * 0;

  /* struct 3 */
  retval += sizeof (ScandataDat3);
  retval += (sizeof (uint8_t) + sizeof (uint16_t)) * _cdio_list_length (obj->mpeg_track_list);

  /* struct 4 */
  /* vcd_assert (sizeof (ScandataDat4) == 0);
     retval += sizeof (ScandataDat4); */
  {
    CdioListNode *node;
    _CDIO_LIST_FOREACH (node, obj->mpeg_track_list)
      {
        const mpeg_track_t *track = _cdio_list_node_data (node);
        
        retval += sizeof (msf_t) * _get_scandata_count (track->info);
      }
  }

  return retval;
}

void
set_scandata_dat (VcdObj *obj, void *buf)
{
  const unsigned tracks = _cdio_list_length (obj->mpeg_track_list);

  ScandataDat1 *scandata_dat1 = (ScandataDat1 *) buf;
  ScandataDat2 *scandata_dat2 = 
    (ScandataDat2 *) &(scandata_dat1->cum_playtimes[tracks]);
  ScandataDat3 *scandata_dat3 =
    (ScandataDat3 *) &(scandata_dat2->spi_indexes[0]);
  ScandataDat4 *scandata_dat4 = 
    (ScandataDat4 *) &(scandata_dat3->mpeg_track_offsets[tracks]);

  const uint16_t _begin_offset =
    __cd_offsetof (ScandataDat3, mpeg_track_offsets[tracks])
    - __cd_offsetof (ScandataDat3, mpeg_track_offsets);

  CdioListNode *node;
  unsigned n;
  uint16_t _tmp_offset;

  vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_4C_SVCD));

  /* memset (buf, 0, get_scandata_dat_size (obj)); */

  /* struct 1 */
  strncpy (scandata_dat1->file_id, SCANDATA_FILE_ID, sizeof (SCANDATA_FILE_ID)-1);
  
  scandata_dat1->version = SCANDATA_VERSION_SVCD;
  scandata_dat1->reserved = 0x00;
  scandata_dat1->scandata_count = uint16_to_be (_get_scanpoint_count (obj));

  scandata_dat1->track_count = uint16_to_be (tracks);
  scandata_dat1->spi_count = uint16_to_be (0);

  for (n = 0; n < tracks; n++)
    {
      double playtime = _get_cumulative_playing_time (obj, n + 1);
      double i = 0, f = 0;

      f = modf(playtime, &i);

      while (i >= (60 * 100))
        i -= (60 * 100);

      vcd_assert (i >= 0);

      cdio_lba_to_msf (i * 75, &(scandata_dat1->cum_playtimes[n]));
      scandata_dat1->cum_playtimes[n].f = cdio_to_bcd8 (floor (f * 75.0));
    }

  /* struct 2 -- nothing yet */

  /* struct 3/4 */

  vcd_assert ((_begin_offset % sizeof (msf_t) == 0)
              && _begin_offset > 0);

  _tmp_offset = 0;

  scandata_dat3->mpegtrack_start_index = uint16_to_be (_begin_offset);

  n = 0;
  _CDIO_LIST_FOREACH (node, obj->mpeg_track_list)
    {
      const mpeg_track_t *track = _cdio_list_node_data (node);
      uint32_t *_table;
      const unsigned scanpoints = _get_scandata_count (track->info);
      const unsigned _table_ofs =
        (_tmp_offset * sizeof (msf_t)) + _begin_offset;
      unsigned point;

      scandata_dat3->mpeg_track_offsets[n].track_num = n + 2;
      scandata_dat3->mpeg_track_offsets[n].table_offset = uint16_to_be (_table_ofs);

      _table = _get_scandata_table (track->info);

      for (point = 0; point < scanpoints; point++)
        {
          uint32_t lsect = _table[point];

          lsect += obj->iso_size;
          lsect += track->relative_start_extent;
          lsect += obj->track_front_margin;

          /* vcd_debug ("lsect %d %d", point, lsect); */

          cdio_lba_to_msf(cdio_lsn_to_lba(lsect),
                          &(scandata_dat4->scandata_table[_tmp_offset + point]));
        }

      free (_table);

      _tmp_offset += scanpoints;
      n++;
    }

  /* struct 4 */

  
}

vcd_type_t
vcd_files_info_detect_type (const void *info_buf)
{
  const InfoVcd_t *_info = info_buf;
  vcd_type_t _type = VCD_TYPE_INVALID;

  vcd_assert (info_buf != NULL);
  
  if (!strncmp (_info->ID, INFO_ID_VCD, sizeof (_info->ID)))
    switch (_info->version)
      {
      case INFO_VERSION_VCD2:
        if (_info->sys_prof_tag != INFO_SPTAG_VCD2)
          vcd_warn ("INFO.VCD: unexpected system profile tag %d encountered",
                    _info->version);
        _type = VCD_TYPE_VCD2;
        break;

      case INFO_VERSION_VCD:
   /* case INFO_VERSION_VCD11: */
        switch (_info->sys_prof_tag)
          {
          case INFO_SPTAG_VCD:
            _type = VCD_TYPE_VCD;
            break;
          case INFO_SPTAG_VCD11:
            _type = VCD_TYPE_VCD11;
            break;
          default:
            vcd_warn ("INFO.VCD: unexpected system profile tag %d "
                      "encountered, assuming VCD 1.1", _info->sys_prof_tag);
            break;
          }
        break;

      default:
        vcd_warn ("unexpected VCD version %d encountered -- assuming VCD 2.0",
                  _info->version);
        break;
      }
  else if (!strncmp (_info->ID, INFO_ID_SVCD, sizeof (_info->ID)))
    switch (_info->version) 
      {
      case INFO_VERSION_SVCD:
        if (_info->sys_prof_tag != INFO_SPTAG_SVCD)
          vcd_warn ("INFO.SVD: unexpected system profile tag value %d "
                    "-- assuming SVCD", _info->sys_prof_tag);
        _type = VCD_TYPE_SVCD;
        break;
        
      default:
        vcd_warn ("INFO.SVD: unexpected version value %d seen "
                  " -- still assuming SVCD", _info->version);
        _type = VCD_TYPE_SVCD;
        break;
      }
  else if (!strncmp (_info->ID, INFO_ID_HQVCD, sizeof (_info->ID)))
    switch (_info->version) 
      {
      case INFO_VERSION_HQVCD:
  if (_info->sys_prof_tag != INFO_SPTAG_HQVCD)
          vcd_warn ("INFO.SVD: unexpected system profile tag value -- assuming hqvcd");
        _type = VCD_TYPE_HQVCD;
        break;
        
      default:
        vcd_warn ("INFO.SVD: unexpected version value %d seen "
                  "-- still assuming HQVCD", _info->version);
        _type = VCD_TYPE_HQVCD;
        break;
      }
  else
    vcd_warn ("INFO.SVD: signature not found");
  
  return _type;
}

/* eof */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
