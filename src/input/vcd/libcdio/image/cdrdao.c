/*
    $Id: cdrdao.c,v 1.1 2005/01/01 02:43:58 rockyb Exp $

    Copyright (C) 2004 Rocky Bernstein <rocky@panix.com>
    toc reading routine adapted from cuetools
    Copyright (C) 2003 Svend Sanjay Sorensen <ssorensen@fastmail.fm>

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

/* This code implements low-level access functions for a CD images
   residing inside a disk file (*.bin) and its associated cue sheet.
   (*.cue).
*/

static const char _rcsid[] = "$Id: cdrdao.c,v 1.1 2005/01/01 02:43:58 rockyb Exp $";

#include "image.h"
#include "cdio_assert.h"
#include "_cdio_stdio.h"

#include <cdio/logging.h>
#include <cdio/sector.h>
#include <cdio/util.h>
#include <cdio/version.h>

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_GLOB_H
#include <glob.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <ctype.h>

#include "portable.h"

/* reader */

#define DEFAULT_CDIO_DEVICE "videocd.bin"
#define DEFAULT_CDIO_CDRDAO "videocd.toc"

typedef struct {
  /* Things common to all drivers like this. 
     This must be first. */
  generic_img_private_t gen; 
  internal_position_t pos; 
  
  char         *psz_cue_name;
  char         *psz_mcn;        /* Media Catalog Number (5.22.3) 
				   exactly 13 bytes */
  track_info_t  tocent[CDIO_CD_MAX_TRACKS+1]; /* entry info for each track 
					         add 1 for leadout. */
  discmode_t    disc_mode;
} _img_private_t;

static uint32_t _stat_size_cdrdao (void *user_data);
static bool parse_tocfile (_img_private_t *cd, const char *toc_name);

#define NEED_MEDIA_EJECT_IMAGE
#include "image_common.h"

/*!
  Initialize image structures.
 */
static bool
_init_cdrdao (_img_private_t *env)
{
  lsn_t lead_lsn;

  if (env->gen.init)
    return false;

  /* Have to set init before calling _stat_size_cdrdao() or we will
     get into infinite recursion calling passing right here.
   */
  env->gen.init      = true;  
  env->gen.i_first_track = 1;
  env->psz_mcn       = NULL;
  env->disc_mode     = CDIO_DISC_MODE_NO_INFO;

  cdtext_init (&(env->gen.cdtext));

  /* Read in TOC sheet. */
  if ( !parse_tocfile(env, env->psz_cue_name) ) return false;
  
  lead_lsn = _stat_size_cdrdao( (_img_private_t *) env);

  if (-1 == lead_lsn) 
    return false;

  /* Fake out leadout track and sector count for last track*/
  cdio_lsn_to_msf (lead_lsn, &env->tocent[env->gen.i_tracks].start_msf);
  env->tocent[env->gen.i_tracks].start_lba = cdio_lsn_to_lba(lead_lsn);
  env->tocent[env->gen.i_tracks-env->gen.i_first_track].sec_count = 
    cdio_lsn_to_lba(lead_lsn - env->tocent[env->gen.i_tracks-1].start_lba);

  return true;
}

/*!
  Reads into buf the next size bytes.
  Returns -1 on error. 
  Would be libc's seek() but we have to adjust for the extra track header 
  information in each sector.
*/
static off_t
_lseek_cdrdao (void *user_data, off_t offset, int whence)
{
  _img_private_t *env = user_data;

  /* real_offset is the real byte offset inside the disk image
     The number below was determined empirically. I'm guessing
     the 1st 24 bytes of a bin file are used for something.
  */
  off_t real_offset=0;

  unsigned int i;

  env->pos.lba = 0;
  for (i=0; i<env->gen.i_tracks; i++) {
    track_info_t  *this_track=&(env->tocent[i]);
    env->pos.index = i;
    if ( (this_track->sec_count*this_track->datasize) >= offset) {
      int blocks            = offset / this_track->datasize;
      int rem               = offset % this_track->datasize;
      int block_offset      = blocks * this_track->blocksize;
      real_offset          += block_offset + rem;
      env->pos.buff_offset = rem;
      env->pos.lba        += blocks;
      break;
    }
    real_offset   += this_track->sec_count*this_track->blocksize;
    offset        -= this_track->sec_count*this_track->datasize;
    env->pos.lba += this_track->sec_count;
  }

  if (i==env->gen.i_tracks) {
    cdio_warn ("seeking outside range of disk image");
    return -1;
  } else {
    real_offset += env->tocent[i].datastart;
    return cdio_stream_seek(env->tocent[i].data_source, real_offset, whence);
  }
}

/*!
  Reads into buf the next size bytes.
  Returns -1 on error. 
  FIXME: 
   At present we assume a read doesn't cross sector or track
   boundaries.
*/
static ssize_t
_read_cdrdao (void *user_data, void *data, size_t size)
{
  _img_private_t *env = user_data;
  char buf[CDIO_CD_FRAMESIZE_RAW] = { 0, };
  char *p = data;
  ssize_t final_size=0;
  ssize_t this_size;
  track_info_t  *this_track=&(env->tocent[env->pos.index]);
  ssize_t skip_size = this_track->datastart + this_track->endsize;

  while (size > 0) {
    int rem = this_track->datasize - env->pos.buff_offset;
    if (size <= rem) {
      this_size = cdio_stream_read(this_track->data_source, buf, size, 1);
      final_size += this_size;
      memcpy (p, buf, this_size);
      break;
    }

    /* Finish off reading this sector. */
    cdio_warn ("Reading across block boundaries not finished");

    size -= rem;
    this_size = cdio_stream_read(this_track->data_source, buf, rem, 1);
    final_size += this_size;
    memcpy (p, buf, this_size);
    p += this_size;
    this_size = cdio_stream_read(this_track->data_source, buf, rem, 1);
    
    /* Skip over stuff at end of this sector and the beginning of the next.
     */
    cdio_stream_read(this_track->data_source, buf, skip_size, 1);

    /* Get ready to read another sector. */
    env->pos.buff_offset=0;
    env->pos.lba++;

    /* Have gone into next track. */
    if (env->pos.lba >= env->tocent[env->pos.index+1].start_lba) {
      env->pos.index++;
      this_track=&(env->tocent[env->pos.index]);
      skip_size = this_track->datastart + this_track->endsize;
    }
  }
  return final_size;
}

/*!
   Return the size of the CD in logical block address (LBA) units.
 */
static uint32_t 
_stat_size_cdrdao (void *user_data)
{
  _img_private_t *env = user_data;
  long size;

  size = cdio_stream_stat (env->tocent[0].data_source);

  if (size % CDIO_CD_FRAMESIZE_RAW)
    {
      cdio_warn ("image %s size (%ld) not multiple of blocksize (%d)", 
		 env->tocent[0].filename, size, CDIO_CD_FRAMESIZE_RAW);
      if (size % M2RAW_SECTOR_SIZE == 0)
	cdio_warn ("this may be a 2336-type disc image");
      else if (size % CDIO_CD_FRAMESIZE_RAW == 0)
	cdio_warn ("this may be a 2352-type disc image");
      /* exit (EXIT_FAILURE); */
    }

  size /= CDIO_CD_FRAMESIZE_RAW;

  return size;
}

#define MAXLINE 512
#define UNIMPLIMENTED_MSG \
  cdio_log(log_level, "%s line %d: unimplimented keyword: %s",  \
	   psz_cue_name, i_line, psz_keyword)


static bool
parse_tocfile (_img_private_t *cd, const char *psz_cue_name)
{
  /* The below declarations may be common in other image-parse routines. */
  FILE        *fp;
  char         psz_line[MAXLINE];   /* text of current line read in file fp. */
  unsigned int i_line=0;            /* line number in file of psz_line. */
  int          i = -1;              /* Position in tocent. Same as 
				       cd->gen.i_tracks - 1 */
  char *psz_keyword, *psz_field;
  cdio_log_level_t log_level = (NULL == cd) ? CDIO_LOG_INFO : CDIO_LOG_WARN;
  cdtext_field_t cdtext_key;

  /* The below declaration(s) may be unique to this image-parse routine. */
  unsigned int i_cdtext_nest = 0;

  if (NULL == psz_cue_name) 
    return false;
  
  fp = fopen (psz_cue_name, "r");
  if (fp == NULL) {
    cdio_log(log_level, "error opening %s for reading: %s", 
	     psz_cue_name, strerror(errno));
    return false;
  }

  if (cd) {
    cd->gen.b_cdtext_init  = true;
    cd->gen.b_cdtext_error = false;
  }

  while ((fgets(psz_line, MAXLINE, fp)) != NULL) {

    i_line++;

    /* strip comment from line */
    /* todo: // in quoted strings? */
    /* //comment */
    if (NULL != (psz_field = strstr (psz_line, "//")))
      *psz_field = '\0';
    
    if (NULL != (psz_keyword = strtok (psz_line, " \t\n\r"))) {
      /* CATALOG "ddddddddddddd" */
      if (0 == strcmp ("CATALOG", psz_keyword)) {
	if (-1 == i) {
	  if (NULL != (psz_field = strtok (NULL, "\"\t\n\r"))) {
	    if (13 != strlen(psz_field)) {
	      cdio_log(log_level, 
		       "%s line %d after word CATALOG:", 
		       psz_cue_name, i_line);
	      cdio_log(log_level, 
		       "Token %s has length %ld. Should be 13 digits.", 
		       psz_field, (long int) strlen(psz_field));
	      
	      goto err_exit;
	    } else {
	      /* Check that we have all digits*/
	      unsigned int i;
	      for (i=0; i<13; i++) {
		if (!isdigit(psz_field[i])) {
		    cdio_log(log_level, 
			     "%s line %d after word CATALOG:", 
			     psz_cue_name, i_line);
		    cdio_log(log_level, 
			     "Character \"%c\" at postition %i of token \"%s\""
			     " is not all digits.", 
			     psz_field[i], i+1, psz_field);
		    goto err_exit;
		}
	      }
	      if (NULL != cd) cd->psz_mcn = strdup (psz_field); 
	    }
	  } else {
	    cdio_log(log_level, 
		     "%s line %d after word CATALOG:", 
		     psz_cue_name, i_line);
	    cdio_log(log_level, "Expecting 13 digits; nothing seen.");
	    goto err_exit;
	  }
	} else {
	  goto err_exit;
	}
	
	/* CD_DA | CD_ROM | CD_ROM_XA */
      } else if (0 == strcmp ("CD_DA", psz_keyword)) {
	if (-1 == i) {
	  if (NULL != cd)
	    cd->disc_mode = CDIO_DISC_MODE_CD_DA;
	} else {
	  goto not_in_global_section;
	}
      } else if (0 == strcmp ("CD_ROM", psz_keyword)) {
	if (-1 == i) {
	  if (NULL != cd)
	    cd->disc_mode = CDIO_DISC_MODE_CD_DATA;
	} else {
	  goto not_in_global_section;
	}
	
      } else if (0 == strcmp ("CD_ROM_XA", psz_keyword)) {
	if (-1 == i) {
	  if (NULL != cd)
	    cd->disc_mode = CDIO_DISC_MODE_CD_XA;
	} else {
	  goto not_in_global_section;
	}
	
	/* TRACK <track-mode> [<sub-channel-mode>] */
      } else if (0 == strcmp ("TRACK", psz_keyword)) {
	i++;
	if (NULL != cd) cdtext_init (&(cd->gen.cdtext_track[i]));
	if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	  if (0 == strcmp ("AUDIO", psz_field)) {
	    if (NULL != cd) {
	      cd->tocent[i].track_format = TRACK_FORMAT_AUDIO;
	      cd->tocent[i].blocksize    = CDIO_CD_FRAMESIZE_RAW;
	      cd->tocent[i].datasize     = CDIO_CD_FRAMESIZE_RAW;
	      cd->tocent[i].datastart    = 0;
	      cd->tocent[i].endsize      = 0;
	      switch(cd->disc_mode) {
	      case CDIO_DISC_MODE_NO_INFO:
		cd->disc_mode = CDIO_DISC_MODE_CD_DA;
		break;
	      case CDIO_DISC_MODE_CD_DA:
	      case CDIO_DISC_MODE_CD_MIXED:
	      case CDIO_DISC_MODE_ERROR:
		/* Disc type stays the same. */
		break;
	      case CDIO_DISC_MODE_CD_DATA:
	      case CDIO_DISC_MODE_CD_XA:
		cd->disc_mode = CDIO_DISC_MODE_CD_MIXED;
		break;
	      default:
		cd->disc_mode = CDIO_DISC_MODE_ERROR;
	      }

	    }
	  } else if (0 == strcmp ("MODE1", psz_field)) {
	    if (NULL != cd) {
	      cd->tocent[i].track_format = TRACK_FORMAT_DATA;
	      cd->tocent[i].blocksize    = CDIO_CD_FRAMESIZE_RAW;
	      cd->tocent[i].datastart    = CDIO_CD_SYNC_SIZE 
		+ CDIO_CD_HEADER_SIZE;
	      cd->tocent[i].datasize     = CDIO_CD_FRAMESIZE; 
	      cd->tocent[i].endsize      = CDIO_CD_EDC_SIZE 
		+ CDIO_CD_M1F1_ZERO_SIZE + CDIO_CD_ECC_SIZE;
	      switch(cd->disc_mode) {
	      case CDIO_DISC_MODE_NO_INFO:
		cd->disc_mode = CDIO_DISC_MODE_CD_DATA;
		break;
	      case CDIO_DISC_MODE_CD_DATA:
	      case CDIO_DISC_MODE_CD_MIXED:
	      case CDIO_DISC_MODE_ERROR:
		/* Disc type stays the same. */
		break;
	      case CDIO_DISC_MODE_CD_DA:
	      case CDIO_DISC_MODE_CD_XA:
		cd->disc_mode = CDIO_DISC_MODE_CD_MIXED;
		break;
	      default:
		cd->disc_mode = CDIO_DISC_MODE_ERROR;
	      }
	    }
	  } else if (0 == strcmp ("MODE1_RAW", psz_field)) {
	    if (NULL != cd) {
	      cd->tocent[i].track_format = TRACK_FORMAT_DATA;
	      cd->tocent[i].blocksize = CDIO_CD_FRAMESIZE_RAW;
	      cd->tocent[i].datastart = CDIO_CD_SYNC_SIZE 
		+ CDIO_CD_HEADER_SIZE;
	      cd->tocent[i].datasize  = CDIO_CD_FRAMESIZE; 
	      cd->tocent[i].endsize   = CDIO_CD_EDC_SIZE 
		+ CDIO_CD_M1F1_ZERO_SIZE + CDIO_CD_ECC_SIZE;
	      switch(cd->disc_mode) {
	      case CDIO_DISC_MODE_NO_INFO:
		cd->disc_mode = CDIO_DISC_MODE_CD_DATA;
		break;
	      case CDIO_DISC_MODE_CD_DATA:
	      case CDIO_DISC_MODE_CD_MIXED:
	      case CDIO_DISC_MODE_ERROR:
		/* Disc type stays the same. */
		break;
	      case CDIO_DISC_MODE_CD_DA:
	      case CDIO_DISC_MODE_CD_XA:
		cd->disc_mode = CDIO_DISC_MODE_CD_MIXED;
		break;
	      default:
		cd->disc_mode = CDIO_DISC_MODE_ERROR;
	      }
	    }
	  } else if (0 == strcmp ("MODE2", psz_field)) {
	    if (NULL != cd) {
	      cd->tocent[i].track_format = TRACK_FORMAT_XA;
	      cd->tocent[i].datastart = CDIO_CD_SYNC_SIZE 
		+ CDIO_CD_HEADER_SIZE;
	      cd->tocent[i].datasize = M2RAW_SECTOR_SIZE;
	      cd->tocent[i].endsize   = 0;
	      switch(cd->disc_mode) {
	      case CDIO_DISC_MODE_NO_INFO:
		cd->disc_mode = CDIO_DISC_MODE_CD_XA;
		break;
	      case CDIO_DISC_MODE_CD_XA:
	      case CDIO_DISC_MODE_CD_MIXED:
	      case CDIO_DISC_MODE_ERROR:
		/* Disc type stays the same. */
		break;
	      case CDIO_DISC_MODE_CD_DA:
	      case CDIO_DISC_MODE_CD_DATA:
		cd->disc_mode = CDIO_DISC_MODE_CD_MIXED;
		break;
	      default:
		cd->disc_mode = CDIO_DISC_MODE_ERROR;
	      }
	    }
	  } else if (0 == strcmp ("MODE2_FORM1", psz_field)) {
	    if (NULL != cd) {
	      cd->tocent[i].track_format = TRACK_FORMAT_XA;
	      cd->tocent[i].datastart = CDIO_CD_SYNC_SIZE 
		+ CDIO_CD_HEADER_SIZE;
	      cd->tocent[i].datasize  = CDIO_CD_FRAMESIZE_RAW;  
	      cd->tocent[i].endsize   = 0;
	      switch(cd->disc_mode) {
	      case CDIO_DISC_MODE_NO_INFO:
		cd->disc_mode = CDIO_DISC_MODE_CD_XA;
		break;
	      case CDIO_DISC_MODE_CD_XA:
	      case CDIO_DISC_MODE_CD_MIXED:
	      case CDIO_DISC_MODE_ERROR:
		/* Disc type stays the same. */
		break;
	      case CDIO_DISC_MODE_CD_DA:
	      case CDIO_DISC_MODE_CD_DATA:
		cd->disc_mode = CDIO_DISC_MODE_CD_MIXED;
		break;
	      default:
		cd->disc_mode = CDIO_DISC_MODE_ERROR;
	      }
	    }
	  } else if (0 == strcmp ("MODE2_FORM2", psz_field)) {
	    if (NULL != cd) {
	      cd->tocent[i].track_format = TRACK_FORMAT_XA;
	      cd->tocent[i].datastart    = CDIO_CD_SYNC_SIZE 
		+ CDIO_CD_HEADER_SIZE + CDIO_CD_SUBHEADER_SIZE;
	      cd->tocent[i].datasize     = CDIO_CD_FRAMESIZE;
	      cd->tocent[i].endsize      = CDIO_CD_SYNC_SIZE 
		+ CDIO_CD_ECC_SIZE;
	      switch(cd->disc_mode) {
	      case CDIO_DISC_MODE_NO_INFO:
		cd->disc_mode = CDIO_DISC_MODE_CD_XA;
		break;
	      case CDIO_DISC_MODE_CD_XA:
	      case CDIO_DISC_MODE_CD_MIXED:
	      case CDIO_DISC_MODE_ERROR:
		/* Disc type stays the same. */
		break;
	      case CDIO_DISC_MODE_CD_DA:
	      case CDIO_DISC_MODE_CD_DATA:
		cd->disc_mode = CDIO_DISC_MODE_CD_MIXED;
		break;
	      default:
		cd->disc_mode = CDIO_DISC_MODE_ERROR;
	      }
	    }
	  } else if (0 == strcmp ("MODE2_FORM_MIX", psz_field)) {
	    if (NULL != cd) {
	      cd->tocent[i].track_format = TRACK_FORMAT_XA;
	      cd->tocent[i].datasize     = M2RAW_SECTOR_SIZE;
	      cd->tocent[i].blocksize    = CDIO_CD_FRAMESIZE_RAW;
	      cd->tocent[i].datastart    = CDIO_CD_SYNC_SIZE + 
		CDIO_CD_HEADER_SIZE + CDIO_CD_SUBHEADER_SIZE;
	      cd->tocent[i].track_green  = true;
	      cd->tocent[i].endsize      = 0;
	      switch(cd->disc_mode) {
	      case CDIO_DISC_MODE_NO_INFO:
		cd->disc_mode = CDIO_DISC_MODE_CD_XA;
		break;
	      case CDIO_DISC_MODE_CD_XA:
	      case CDIO_DISC_MODE_CD_MIXED:
	      case CDIO_DISC_MODE_ERROR:
		/* Disc type stays the same. */
		break;
	      case CDIO_DISC_MODE_CD_DA:
	      case CDIO_DISC_MODE_CD_DATA:
		cd->disc_mode = CDIO_DISC_MODE_CD_MIXED;
		break;
	      default:
		cd->disc_mode = CDIO_DISC_MODE_ERROR;
	      }
	    }
	  } else if (0 == strcmp ("MODE2_RAW", psz_field)) {
	    if (NULL != cd) {
	      cd->tocent[i].track_format = TRACK_FORMAT_XA;
	      cd->tocent[i].blocksize    = CDIO_CD_FRAMESIZE_RAW;
	      cd->tocent[i].datastart    = CDIO_CD_SYNC_SIZE + 
		CDIO_CD_HEADER_SIZE + CDIO_CD_SUBHEADER_SIZE;
	      cd->tocent[i].datasize     = CDIO_CD_FRAMESIZE;
	      cd->tocent[i].track_green  = true;
	      cd->tocent[i].endsize      = 0;
	      switch(cd->disc_mode) {
	      case CDIO_DISC_MODE_NO_INFO:
		cd->disc_mode = CDIO_DISC_MODE_CD_XA;
		break;
	      case CDIO_DISC_MODE_CD_XA:
	      case CDIO_DISC_MODE_CD_MIXED:
	      case CDIO_DISC_MODE_ERROR:
		/* Disc type stays the same. */
		break;
	      case CDIO_DISC_MODE_CD_DA:
	      case CDIO_DISC_MODE_CD_DATA:
		cd->disc_mode = CDIO_DISC_MODE_CD_MIXED;
		break;
	      default:
		cd->disc_mode = CDIO_DISC_MODE_ERROR;
	      }
	    }
	  } else {
	    cdio_log(log_level, "%s line %d after TRACK:",
		     psz_cue_name, i_line);
	    cdio_log(log_level, "'%s' not a valid mode.", psz_field);
	    goto err_exit;
	  }
	}
	if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	  /* todo: set sub-channel-mode */
	  if (0 == strcmp ("RW", psz_field))
	    ;
	  else if (0 == strcmp ("RW_RAW", psz_field))
	    ;
	}
	if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	  goto format_error;
	}
	
	/* track flags */
	/* [NO] COPY | [NO] PRE_EMPHASIS */
      } else if (0 == strcmp ("NO", psz_keyword)) {
	if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	  if (0 == strcmp ("COPY", psz_field)) {
	    if (NULL != cd) 
	      cd->tocent[i].flags &= ~CDIO_TRACK_FLAG_COPY_PERMITTED;
	    
	  } else if (0 == strcmp ("PRE_EMPHASIS", psz_field))
	    if (NULL != cd) {
	      cd->tocent[i].flags &= ~CDIO_TRACK_FLAG_PRE_EMPHASIS;
	      goto err_exit;
	    }
	} else {
	  goto format_error;
	}
	if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	  goto format_error;
	}
      } else if (0 == strcmp ("COPY", psz_keyword)) {
	if (NULL != cd)
	  cd->tocent[i].flags |= CDIO_TRACK_FLAG_COPY_PERMITTED;
      } else if (0 == strcmp ("PRE_EMPHASIS", psz_keyword)) {
	if (NULL != cd)
	  cd->tocent[i].flags |= CDIO_TRACK_FLAG_PRE_EMPHASIS;
	/* TWO_CHANNEL_AUDIO */
      } else if (0 == strcmp ("TWO_CHANNEL_AUDIO", psz_keyword)) {
	if (NULL != cd)
	  cd->tocent[i].flags &= ~CDIO_TRACK_FLAG_FOUR_CHANNEL_AUDIO;
	/* FOUR_CHANNEL_AUDIO */
      } else if (0 == strcmp ("FOUR_CHANNEL_AUDIO", psz_keyword)) {
	if (NULL != cd)
	  cd->tocent[i].flags |= CDIO_TRACK_FLAG_FOUR_CHANNEL_AUDIO;
	
	/* ISRC "CCOOOYYSSSSS" */
      } else if (0 == strcmp ("ISRC", psz_keyword)) {
	if (NULL != (psz_field = strtok (NULL, "\"\t\n\r"))) {
	  if (NULL != cd) 
	    cd->tocent[i].isrc = strdup(psz_field);
	} else {
	  goto format_error;
	}
	
	/* SILENCE <length> */
      } else if (0 == strcmp ("SILENCE", psz_keyword)) {
	UNIMPLIMENTED_MSG;
	
	/* ZERO <length> */
      } else if (0 == strcmp ("ZERO", psz_keyword)) {
	UNIMPLIMENTED_MSG;
	
	/* [FILE|AUDIOFILE] "<filename>" <start> [<length>] */
      } else if (0 == strcmp ("FILE", psz_keyword) 
		 || 0 == strcmp ("AUDIOFILE", psz_keyword)) {
	if (0 <= i) {
	  if (NULL != (psz_field = strtok (NULL, "\"\t\n\r"))) {
	    if (NULL != cd) {
	      cd->tocent[i].filename = strdup (psz_field);
	      /* Todo: do something about reusing existing files. */
	      if (!(cd->tocent[i].data_source = cdio_stdio_new (psz_field))) {
		cdio_log (log_level, 
			  "%s line %d: can't open file `%s' for reading", 
			   psz_cue_name, i_line, psz_field);
		goto err_exit;
	      }
	    }
	  }
	  
	  if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	    lba_t lba = cdio_lsn_to_lba(cdio_mmssff_to_lba (psz_field));
	    if (CDIO_INVALID_LBA == lba) {
	      cdio_log(log_level, "%s line %d: invalid MSF string %s", 
		       psz_cue_name, i_line, psz_field);
	      goto err_exit;
	    }
	    
	    if (NULL != cd) {
	      cd->tocent[i].start_lba = lba;
	      cdio_lba_to_msf(lba, &(cd->tocent[i].start_msf));
	    }
	  }
	  if (NULL != (psz_field = strtok (NULL, " \t\n\r")))
	    if (NULL != cd)
	      cd->tocent[i].length = cdio_mmssff_to_lba (psz_field);
	  if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	    goto format_error;
	  }
	} else {
	  goto not_in_global_section;
	}
	
	/* DATAFILE "<filename>" <start> [<length>] */
      } else if (0 == strcmp ("DATAFILE", psz_keyword)) {
	goto unimplimented_error;
	
	/* FIFO "<fifo path>" [<length>] */
      } else if (0 == strcmp ("FIFO", psz_keyword)) {
	goto unimplimented_error;
	
	/* START MM:SS:FF */
      } else if (0 == strcmp ("START", psz_keyword)) {
	if (0 <= i) {
	  if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	    /* todo: line is too long! */
	    if (NULL != cd) {
	      cd->tocent[i].start_lba += cdio_mmssff_to_lba (psz_field);
	      cdio_lba_to_msf(cd->tocent[i].start_lba, 
			      &(cd->tocent[i].start_msf));
	    }
	  }
	  
	  if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	    goto format_error;
	  }
	} else {
	  goto not_in_global_section;
	}
	
	/* PREGAP MM:SS:FF */
      } else if (0 == strcmp ("PREGAP", psz_keyword)) {
	if (0 <= i) {
	  if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	    if (NULL != cd) 
	      cd->tocent[i].pregap = cdio_mmssff_to_lba (psz_field);
	  } else {
	    goto format_error;
	  }
	  if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	    goto format_error;
	  } 
	} else {
	  goto not_in_global_section;
	}
	  
	  /* INDEX MM:SS:FF */
      } else if (0 == strcmp ("INDEX", psz_keyword)) {
	if (0 <= i) {
	  if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	    if (NULL != cd) {
#if 0
	      if (1 == cd->tocent[i].nindex) {
		cd->tocent[i].indexes[1] = cd->tocent[i].indexes[0];
		cd->tocent[i].nindex++;
	      }
	      cd->tocent[i].indexes[cd->tocent[i].nindex++] = 
		cdio_mmssff_to_lba (psz_field) + cd->tocent[i].indexes[0];
#else 
	      ;
	      
#endif
	    }
	  } else {
	    goto format_error;
	  }
	  if (NULL != (psz_field = strtok (NULL, " \t\n\r"))) {
	    goto format_error;
	  }
	}  else {
	  goto not_in_global_section;
	}
	  
	  /* CD_TEXT { ... } */
	  /* todo: opening { must be on same line as CD_TEXT */
      } else if (0 == strcmp ("CD_TEXT", psz_keyword)) {
	  if (NULL == (psz_field = strtok (NULL, " \t\n\r"))) {
	    goto format_error;
	  }
	  if ( 0 == strcmp( "{", psz_field ) ) {
	    i_cdtext_nest++;
	  } else {
	    cdio_log (log_level, 
		      "%s line %d: expecting '{'", psz_cue_name, i_line);
	    goto err_exit;
	  }
	       
      } else if (0 == strcmp ("LANGUAGE_MAP", psz_keyword)) {
	/* LANGUAGE d { ... } */
      } else if (0 == strcmp ("LANGUAGE", psz_keyword)) {
	  if (NULL == (psz_field = strtok (NULL, " \t\n\r"))) {
	    goto format_error;
	  }
	  /* Language number */
	  if (NULL == (psz_field = strtok (NULL, " \t\n\r"))) {
	    goto format_error;
	  }
	  if ( 0 == strcmp( "{", psz_field ) ) {
	    i_cdtext_nest++;
	  }
      } else if (0 == strcmp ("{", psz_keyword)) {
	i_cdtext_nest++;
      } else if (0 == strcmp ("}", psz_keyword)) {
	if (i_cdtext_nest > 0) i_cdtext_nest--;
      } else if ( CDTEXT_INVALID != 
		  (cdtext_key = cdtext_is_keyword (psz_keyword)) ) {
	if (-1 == i) {
	  if (NULL != cd) {
	    cdtext_set (cdtext_key, 
			strtok (NULL, "\"\t\n\r"), 
			&(cd->gen.cdtext));
	  }
	} else {
	  if (NULL != cd) {
	    cdtext_set (cdtext_key, 
			strtok (NULL, "\"\t\n\r"), 
			&(cd->gen.cdtext_track[i]));
	  }
	}

	/* unrecognized line */
      } else {
	cdio_log(log_level, "%s line %d: warning: unrecognized word: %s", 
		 psz_cue_name, i_line, psz_keyword);
	goto err_exit;
      }
    }
  }
    
  if (NULL != cd) {
    cd->gen.i_tracks = i+1;
    cd->gen.toc_init = true;
  }

  fclose (fp);
  return true;

 unimplimented_error:
  UNIMPLIMENTED_MSG;
  goto err_exit;
  
 format_error:
  cdio_log(log_level, "%s line %d after word %s", 
	   psz_cue_name, i_line, psz_keyword);
  goto err_exit;
  
 not_in_global_section:
  cdio_log(log_level, "%s line %d: word %s only allowed in global section", 
	   psz_cue_name, i_line, psz_keyword);

 err_exit: 
  fclose (fp);
  return false;
}

/*!
   Reads a single audio sector from CD device into data starting
   from lsn. Returns 0 if no error. 
 */
static int
_read_audio_sectors_cdrdao (void *user_data, void *data, lsn_t lsn, 
			  unsigned int nblocks)
{
  _img_private_t *env = user_data;
  int ret;

  /* Why the adjustment of 272, I don't know. It seems to work though */
  if (lsn != 0) {
    ret = cdio_stream_seek (env->tocent[0].data_source, 
			    (lsn * CDIO_CD_FRAMESIZE_RAW) - 272, SEEK_SET);
    if (ret!=0) return ret;

    ret = cdio_stream_read (env->tocent[0].data_source, data, 
			    CDIO_CD_FRAMESIZE_RAW, nblocks);
  } else {
    /* We need to pad out the first 272 bytes with 0's */
    BZERO(data, 272);
    
    ret = cdio_stream_seek (env->tocent[0].data_source, 0, SEEK_SET);

    if (ret!=0) return ret;

    ret = cdio_stream_read (env->tocent[0].data_source, (uint8_t *) data+272, 
			    CDIO_CD_FRAMESIZE_RAW - 272, nblocks);
  }

  /* ret is number of bytes if okay, but we need to return 0 okay. */
  return ret == 0;
}

/*!
   Reads a single mode2 sector from cd device into data starting
   from lsn. Returns 0 if no error. 
 */
static int
_read_mode1_sector_cdrdao (void *user_data, void *data, lsn_t lsn, 
			 bool b_form2)
{
  _img_private_t *env = user_data;
  int ret;
  char buf[CDIO_CD_FRAMESIZE_RAW] = { 0, };

  ret = cdio_stream_seek (env->tocent[0].data_source, 
			  lsn * CDIO_CD_FRAMESIZE_RAW, SEEK_SET);
  if (ret!=0) return ret;

  /* FIXME: Not completely sure the below is correct. */
  ret = cdio_stream_read (env->tocent[0].data_source, buf, 
			  CDIO_CD_FRAMESIZE_RAW, 1);
  if (ret==0) return ret;

  memcpy (data, buf + CDIO_CD_SYNC_SIZE + CDIO_CD_HEADER_SIZE, 
	  b_form2 ? M2RAW_SECTOR_SIZE: CDIO_CD_FRAMESIZE);

  return 0;
}

/*!
   Reads nblocks of mode1 sectors from cd device into data starting
   from lsn.
   Returns 0 if no error. 
 */
static int
_read_mode1_sectors_cdrdao (void *user_data, void *data, lsn_t lsn, 
			    bool b_form2, unsigned int nblocks)
{
  _img_private_t *env = user_data;
  int i;
  int retval;
  unsigned int blocksize = b_form2 ? M2RAW_SECTOR_SIZE : CDIO_CD_FRAMESIZE;

  for (i = 0; i < nblocks; i++) {
    if ( (retval = _read_mode1_sector_cdrdao (env, 
					    ((char *)data) + (blocksize * i),
					    lsn + i, b_form2)) )
      return retval;
  }
  return 0;
}

/*!
   Reads a single mode1 sector from cd device into data starting
   from lsn. Returns 0 if no error. 
 */
static int
_read_mode2_sector_cdrdao (void *user_data, void *data, lsn_t lsn, 
			 bool b_form2)
{
  _img_private_t *env = user_data;
  int ret;
  char buf[CDIO_CD_FRAMESIZE_RAW] = { 0, };

  /* NOTE: The logic below seems a bit wrong and convoluted
     to me, but passes the regression tests. (Perhaps it is why we get
     valgrind errors in vcdxrip). Leave it the way it was for now.
     Review this sector 2336 stuff later.
  */

  ret = cdio_stream_seek (env->tocent[0].data_source, 
			  lsn * CDIO_CD_FRAMESIZE_RAW, SEEK_SET);
  if (ret!=0) return ret;

  ret = cdio_stream_read (env->tocent[0].data_source, buf, 
			  CDIO_CD_FRAMESIZE_RAW, 1);
  if (ret==0) return ret;


  /* See NOTE above. */
  if (b_form2)
    memcpy (data, buf + CDIO_CD_SYNC_SIZE + CDIO_CD_HEADER_SIZE, 
	    M2RAW_SECTOR_SIZE);
  else
    memcpy (data, buf + CDIO_CD_XA_SYNC_HEADER, CDIO_CD_FRAMESIZE);

  return 0;
}

/*!
   Reads nblocks of mode2 sectors from cd device into data starting
   from lsn.
   Returns 0 if no error. 
 */
static int
_read_mode2_sectors_cdrdao (void *user_data, void *data, lsn_t lsn, 
			    bool b_form2, unsigned int nblocks)
{
  _img_private_t *env = user_data;
  int i;
  int retval;

  for (i = 0; i < nblocks; i++) {
    if ( (retval = _read_mode2_sector_cdrdao (env, 
					    ((char *)data) + (CDIO_CD_FRAMESIZE * i),
					    lsn + i, b_form2)) )
      return retval;
  }
  return 0;
}

/*!
  Return an array of strings giving possible TOC disk images.
 */
char **
cdio_get_devices_cdrdao (void)
{
  char **drives = NULL;
  unsigned int num_files=0;
#ifdef HAVE_GLOB_H
  unsigned int i;
  glob_t globbuf;
  globbuf.gl_offs = 0;
  glob("*.toc", GLOB_DOOFFS, NULL, &globbuf);
  for (i=0; i<globbuf.gl_pathc; i++) {
    cdio_add_device_list(&drives, globbuf.gl_pathv[i], &num_files);
  }
  globfree(&globbuf);
#else
  cdio_add_device_list(&drives, DEFAULT_CDIO_DEVICE, &num_files);
#endif /*HAVE_GLOB_H*/
  cdio_add_device_list(&drives, NULL, &num_files);
  return drives;
}

/*!
  Return a string containing the default CD device.
 */
char *
cdio_get_default_device_cdrdao(void)
{
  char **drives = cdio_get_devices_nrg();
  char *drive = (drives[0] == NULL) ? NULL : strdup(drives[0]);
  cdio_free_device_list(drives);
  return drive;
}

static bool
get_hwinfo_cdrdao ( const CdIo *p_cdio, /*out*/ cdio_hwinfo_t *hw_info)
{
  strcpy(hw_info->psz_vendor, "libcdio");
  strcpy(hw_info->psz_model, "cdrdao");
  strcpy(hw_info->psz_revision, CDIO_VERSION);
  return true;
  
}

/*!
  Return the number of tracks in the current medium.
  CDIO_INVALID_TRACK is returned on error.
*/
static track_format_t
_get_track_format_cdrdao(void *user_data, track_t i_track) 
{
  _img_private_t *env = user_data;
  
  if (!env->gen.init) _init_cdrdao(env);

  if (i_track > env->gen.i_tracks || i_track == 0) 
    return TRACK_FORMAT_ERROR;

  return env->tocent[i_track-env->gen.i_first_track].track_format;
}

/*!
  Return true if we have XA data (green, mode2 form1) or
  XA data (green, mode2 form2). That is track begins:
  sync - header - subheader
  12     4      -  8
  
  FIXME: there's gotta be a better design for this and get_track_format?
*/
static bool
_get_track_green_cdrdao(void *user_data, track_t i_track) 
{
  _img_private_t *env = user_data;
  
  if (!env->gen.init) _init_cdrdao(env);

  if (i_track > env->gen.i_tracks || i_track == 0) 
    return false;

  return env->tocent[i_track-env->gen.i_first_track].track_green;
}

/*!  
  Return the starting LSN track number
  i_track in obj.  Track numbers start at 1.
  The "leadout" track is specified either by
  using i_track LEADOUT_TRACK or the total tracks+1.
  False is returned if there is no track entry.
*/
static lba_t
_get_lba_track_cdrdao(void *user_data, track_t i_track)
{
  _img_private_t *env = user_data;
  _init_cdrdao (env);

  if (i_track == CDIO_CDROM_LEADOUT_TRACK) 
    i_track = env->gen.i_tracks+1;

  if (i_track <= env->gen.i_tracks+1 && i_track != 0) {
    return env->tocent[i_track-1].start_lba;
  } else 
    return CDIO_INVALID_LBA;
}

/*! 
  Check that a TOC file is valid. We parse the entire file.

*/
bool
cdio_is_tocfile(const char *psz_cue_name) 
{
  int   i;
  
  if (psz_cue_name == NULL) return false;

  i=strlen(psz_cue_name)-strlen("toc");
  
  if (i>0) {
    if ( (psz_cue_name[i]=='t' && psz_cue_name[i+1]=='o' && psz_cue_name[i+2]=='c') 
	 || (psz_cue_name[i]=='T' && psz_cue_name[i+1]=='O' && psz_cue_name[i+2]=='C') ) {
      return parse_tocfile(NULL, psz_cue_name);
    }
  }
  return false;
}

/*!
  Initialization routine. This is the only thing that doesn't
  get called via a function pointer. In fact *we* are the
  ones to set that up.
 */
CdIo *
cdio_open_am_cdrdao (const char *psz_source_name, const char *psz_access_mode)
{
  if (psz_access_mode != NULL && strcmp(psz_access_mode, "image"))
    cdio_warn ("there is only one access mode, 'image' for cdrdao. Arg %s ignored",
	       psz_access_mode);
  return cdio_open_cdrdao(psz_source_name);
}

/*!
  Initialization routine. This is the only thing that doesn't
  get called via a function pointer. In fact *we* are the
  ones to set that up.
 */
CdIo *
cdio_open_cdrdao (const char *psz_cue_name)
{
  CdIo *ret;
  _img_private_t *_data;

  cdio_funcs _funcs;
  
  memset( &_funcs, 0, sizeof(_funcs) );
  
  _funcs.eject_media        = _eject_media_image;
  _funcs.free               = _free_image;
  _funcs.get_arg            = _get_arg_image;
  _funcs.get_cdtext         = get_cdtext_generic;
  _funcs.get_devices        = cdio_get_devices_cdrdao;
  _funcs.get_default_device = cdio_get_default_device_cdrdao;
  _funcs.get_discmode       = _get_discmode_image;
  _funcs.get_drive_cap      = _get_drive_cap_image;
  _funcs.get_first_track_num= _get_first_track_num_image;
  _funcs.get_hwinfo         = get_hwinfo_cdrdao;
  _funcs.get_mcn            = _get_mcn_image;
  _funcs.get_num_tracks     = _get_num_tracks_image;
  _funcs.get_track_format   = _get_track_format_cdrdao;
  _funcs.get_track_green    = _get_track_green_cdrdao;
  _funcs.get_track_lba      = _get_lba_track_cdrdao;
  _funcs.get_track_msf      = _get_track_msf_image;
  _funcs.lseek              = _lseek_cdrdao;
  _funcs.read               = _read_cdrdao;
  _funcs.read_audio_sectors = _read_audio_sectors_cdrdao;
  _funcs.read_mode1_sector  = _read_mode1_sector_cdrdao;
  _funcs.read_mode1_sectors = _read_mode1_sectors_cdrdao;
  _funcs.read_mode2_sector  = _read_mode2_sector_cdrdao;
  _funcs.read_mode2_sectors = _read_mode2_sectors_cdrdao;
  _funcs.set_arg            = _set_arg_image;
  _funcs.stat_size          = _stat_size_cdrdao;

  if (NULL == psz_cue_name) return NULL;
  
  _data                  = _cdio_malloc (sizeof (_img_private_t));
  _data->gen.init        = false;
  _data->psz_cue_name    = NULL;
  _data->gen.data_source = NULL;
  _data->gen.source_name = NULL;

  ret = cdio_new ((void *)_data, &_funcs);

  if (ret == NULL) {
    free(_data);
    return NULL;
  }

  if (!cdio_is_tocfile(psz_cue_name)) {
    cdio_debug ("source name %s is not recognized as a TOC file", 
		psz_cue_name);
    return NULL;
  }
  
  _set_arg_image (_data, "cue", psz_cue_name);
  _set_arg_image (_data, "source", psz_cue_name);

  if (_init_cdrdao(_data)) {
    return ret;
  } else {
    _free_image(_data);
    free(ret);
    return NULL;
  }
}

bool
cdio_have_cdrdao (void)
{
  return true;
}
