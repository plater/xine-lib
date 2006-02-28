/*
 * Copyright (C) 2000-2006 the xine project and Claudio Ciccani
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
 *
 * FusionSound based audio output plugin by Claudio Ciccani <klan@directfb.org>
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_MODULE "audio_fusionsound_out"
#define LOG_VERBOSE

#include "xine.h"
#include "xine_internal.h"
#include "audio_out.h"
#include "xineutils.h"

#include <directfb.h>

#include <fusionsound.h>
#include <fusionsound_version.h>


#define AO_OUT_FS_IFACE_VERSION 8

#define GAP_TOLERANCE  5000

typedef struct fusionsound_driver_s {
  ao_driver_t           ao_driver;

  xine_t               *xine;

  IFusionSound         *sound;
  IFusionSoundStream   *stream;
  IFusionSoundPlayback *playback;

  FSSampleFormat        format;
  int                   channels;
  int                   rate;
  int                   bytes_per_frame;

  float                 vol;
  int                   vol_mute;
  
  float                 amp;
  int                   amp_mute;
  
  int                   paused;
} fusionsound_driver_t;

typedef struct {
  audio_driver_class_t   ao_class;
  xine_t                *xine;
} fusionsound_class_t;



static int ao_fusionsound_open(ao_driver_t *ao_driver,
                               uint32_t bits, uint32_t rate, int mode) {
  fusionsound_driver_t *this = (fusionsound_driver_t *) ao_driver;
  FSStreamDescription   dsc;
  DFBResult             ret;

  xprintf (this->xine, XINE_VERBOSITY_DEBUG,
           "audio_fusionsound_out: ao_open bits=%d rate=%d, mode=%d\n", 
           bits, rate, mode);
  
  dsc.flags = FSSDF_BUFFERSIZE   | FSBDF_CHANNELS | 
              FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE;

  switch (mode) {
    case AO_CAP_MODE_MONO: 
      dsc.channels = 1;
      break;
    case AO_CAP_MODE_STEREO:
      dsc.channels = 2;
    break;
    default:
      xprintf (this->xine, XINE_VERBOSITY_LOG,
               "audio_fusionsound_out: mode %#x not supported\n", mode);
      return 0;
  }

  switch (bits) {
    case 8:
      dsc.sampleformat = FSSF_U8;
      break;
    case 16:
      dsc.sampleformat = FSSF_S16;
      break;
    case 24:
      dsc.sampleformat = FSSF_S24;
      break;
    case 32:
      dsc.sampleformat = FSSF_S32;
      break;
    default:
      xprintf (this->xine, XINE_VERBOSITY_LOG,
               "audio_fusionsound_out: bits %d not supported\n", bits);
      return 0;
  }

  dsc.samplerate = rate;
  dsc.buffersize = rate / 5;

  if (dsc.sampleformat != this->format   ||
      dsc.channels     != this->channels ||
      dsc.samplerate   != this->rate     ||
      this->stream     == NULL)
  {
    if (this->playback) {
      this->playback->Release (this->playback);
      this->playback = NULL;
    }
    
    if (this->stream) {
      this->stream->Release (this->stream);
      this->stream = NULL;
    }
    
    ret = this->sound->CreateStream (this->sound, &dsc, &this->stream);  
    if (ret != DFB_OK) {
      xprintf (this->xine, XINE_VERBOSITY_LOG,
               "audio_fusionsound_out: IFusionSound::CreateStream() failed [%s]\n",
               FusionSoundErrorString (ret));
      return 0;
    }

    this->stream->GetDescription (this->stream, &dsc);

    this->format = dsc.sampleformat;
    this->channels = dsc.channels;
    this->rate = dsc.samplerate;
    this->bytes_per_frame = this->channels * FS_BYTES_PER_SAMPLE(this->format);
    
    ret = this->stream->GetPlayback (this->stream, &this->playback);
    if (ret == DFB_OK) {
      this->playback->SetVolume (this->playback, 
                                 (this->vol_mute ? 0 : this->vol) *
                                 (this->amp_mute ? 0 : this->amp));
      if (this->paused)
        this->playback->Stop (this->playback);
    }
    else {
      xprintf (this->xine, XINE_VERBOSITY_LOG,
               "audio_fusionsound_out: "
               "IFusionSoundStream::GetPlayback() failed [%s]\n",
               FusionSoundErrorString (ret));
    }
  }

  return this->rate;
}

static int ao_fusionsound_num_channels(ao_driver_t *ao_driver) {
  fusionsound_driver_t *this = (fusionsound_driver_t *) ao_driver;
  
  return this->channels;
}

static int ao_fusionsound_bytes_per_frame(ao_driver_t *ao_driver) {
  fusionsound_driver_t *this = (fusionsound_driver_t *) ao_driver;
  
  return this->bytes_per_frame;
}

static int ao_fusionsound_delay(ao_driver_t *ao_driver) {
  fusionsound_driver_t *this  = (fusionsound_driver_t *) ao_driver;
  int                   delay = 0;
  
  this->stream->GetPresentationDelay (this->stream, &delay);
  
  return (delay * this->rate / 1000);
}

static int ao_fusionsound_get_gap_tolerance(ao_driver_t *ao_driver) {
  return GAP_TOLERANCE;
}

static int ao_fusionsound_write(ao_driver_t *ao_driver,
                                int16_t *data, uint32_t num_frames) {
  fusionsound_driver_t *this = (fusionsound_driver_t *) ao_driver;
  DFBResult             ret;

  ret = this->stream->Write (this->stream, (void *)data, num_frames);
  if (ret != DFB_OK) {
    xprintf (this->xine, XINE_VERBOSITY_LOG,
             "audio_fusionsound_out: IFusionSoundStream::Write() failed [%s]\n",
             FusionSoundErrorString (ret));
    return 0;
  }
  
  return num_frames;
}

static void ao_fusionsound_close(ao_driver_t *ao_driver){
  fusionsound_driver_t *this = (fusionsound_driver_t *) ao_driver;

  if (this->playback) {
    this->playback->Release (this->playback);
    this->playback = NULL;
  }

  if (this->stream) {
    this->stream->Release (this->stream);
    this->stream = NULL;
  }
}

/*
 * FusionSound supports amplifier level adjustment;
 * probably AO_CAP_AMP should be added to take advantage of this feature.
 */

static uint32_t ao_fusionsound_get_capabilities(ao_driver_t *ao_driver) {
  return (AO_CAP_MODE_MONO | AO_CAP_MODE_STEREO | AO_CAP_MIXER_VOL |
          AO_CAP_8BITS     | AO_CAP_16BITS      | AO_CAP_24BITS);
}

static void ao_fusionsound_exit(ao_driver_t *ao_driver) {
  fusionsound_driver_t *this = (fusionsound_driver_t *) ao_driver;

  if (this->sound)
    this->sound->Release (this->sound);
    
  free (this);
}

static int ao_fusionsound_get_property(ao_driver_t *ao_driver, int property) {
  fusionsound_driver_t *this = (fusionsound_driver_t *) ao_driver;

  switch (property) {
    case AO_PROP_MIXER_VOL:
      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
               "audio_fusionsound_out: volume is %.2f\n", this->vol);
      return (int) (this->vol * 100.0);

    case AO_PROP_MUTE_VOL:
      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
               "audio_fusionsound_out: volume mute is %d\n", this->vol_mute);
      return this->vol_mute;
      
    case AO_PROP_AMP:
      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
               "audio_fusionsound_out: amplifier is %.2f\n", this->amp);
      return (int) (this->amp * 100.0);
      
    case AO_PROP_AMP_MUTE:
      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
               "audio_fusionsound_out: amplifier mute is %d\n", this->amp_mute);
      return this->amp_mute;
      
    default:
      break;
  }

  return 0;
}

static int ao_fusionsound_set_property(ao_driver_t *ao_driver, 
                                       int property, int value ) {
  fusionsound_driver_t *this = (fusionsound_driver_t *) ao_driver;
  
  if (!this->playback)
    return 0;

  switch (property) {
    case AO_PROP_MIXER_VOL:
      this->vol = (float)value / 100.0;
      this->playback->SetVolume (this->playback, 
                                 (this->vol_mute ? 0 : this->vol) *
                                 (this->amp_mute ? 0 : this->amp));
      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
              "audio_fusionsound_out: volume set to %.2f\n", this->vol);
      break;
      
    case AO_PROP_MUTE_VOL:
      value = value ? 1 : 0;
      if (this->vol_mute != value) {
        this->vol_mute = value;
        this->playback->SetVolume (this->playback, 
                                   (this->vol_mute ? 0 : this->vol) *
                                   (this->amp_mute ? 0 : this->amp));
        xprintf (this->xine, XINE_VERBOSITY_DEBUG,
                 "audio_fusionsound_out: volume mute set to %d\n", 
                 this->vol_mute);
      }
      break;

    case AO_PROP_AMP:
      this->amp = (float)value / 100.0;
      this->playback->SetVolume (this->playback, 
                                 (this->vol_mute ? 0 : this->vol) *
                                 (this->amp_mute ? 0 : this->amp));
      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
              "audio_fusionsound_out: amplifier set to %.2f\n", this->amp);
      break;
      
    case AO_PROP_AMP_MUTE:
      value = value ? 1 : 0;
      if (this->amp_mute != value) {
        this->amp_mute = value;
        this->playback->SetVolume (this->playback, 
                                   (this->vol_mute ? 0 : this->vol) *
                                   (this->amp_mute ? 0 : this->amp));
        xprintf (this->xine, XINE_VERBOSITY_DEBUG,
                 "audio_fusionsound_out: amplifier mute set to %d\n", 
                 this->amp_mute);
      }
      break;

    default:
      return 0;
  }

  return value;
}

static int ao_fusionsound_control(ao_driver_t *ao_driver, int cmd, ...) {
  fusionsound_driver_t *this = (fusionsound_driver_t *) ao_driver;
  DFBResult             ret  = DFB_UNSUPPORTED;

  switch (cmd) {
    case AO_CTRL_PLAY_PAUSE:
      if (this->playback) {
        lprintf ("Pause()\n");
        ret = this->playback->Stop (this->playback);
        this->paused = 1;
      }
      break;
    
    case AO_CTRL_PLAY_RESUME:
      if (this->playback) {
        lprintf ("Resume()\n");
        ret = this->playback->Continue (this->playback);
        this->paused = 0;
      }
      break;

    case AO_CTRL_FLUSH_BUFFERS:
      lprintf ("Flush()\n");
      ret = this->stream->Flush (this->stream);
      break;

    default:
      break;
  }

  return (ret == DFB_OK);
}


static ao_driver_t* open_plugin(audio_driver_class_t *ao_class, 
                                const void           *data ) {
  fusionsound_class_t  *class = (fusionsound_class_t *) ao_class;
  fusionsound_driver_t *this;
  DFBResult             ret;

  this = (fusionsound_driver_t *) xine_xmalloc (sizeof(fusionsound_driver_t));
  if (!this) {
    xprintf (class->xine, XINE_VERBOSITY_LOG,
             "audio_fusionsound_out: driver interface allocation failed!\n");
    return NULL;
  }

  ret = FusionSoundInit (NULL, NULL);
  if (ret != DFB_OK) {
    xprintf (class->xine, XINE_VERBOSITY_LOG,
             "audio_fusionsound_out: FusionSoundInit() failed [%s]\n",
             FusionSoundErrorString (ret));
    free (this);
    return NULL;
  }
  
  DirectFBSetOption ("no-sighandler", NULL);

  ret = FusionSoundCreate (&this->sound);
  if (ret != DFB_OK) {
    xprintf (class->xine, XINE_VERBOSITY_LOG,
             "audio_fusionsound_out: FusionSoundCreate() failed [%s]\n",
             FusionSoundErrorString (ret));
    free (this);
    return NULL;
  }

  this->xine                        = class->xine;
  this->ao_driver.get_capabilities  = ao_fusionsound_get_capabilities;
  this->ao_driver.get_property      = ao_fusionsound_get_property;
  this->ao_driver.set_property      = ao_fusionsound_set_property;
  this->ao_driver.open              = ao_fusionsound_open;
  this->ao_driver.num_channels      = ao_fusionsound_num_channels;
  this->ao_driver.bytes_per_frame   = ao_fusionsound_bytes_per_frame;
  this->ao_driver.delay             = ao_fusionsound_delay;
  this->ao_driver.write             = ao_fusionsound_write;
  this->ao_driver.close             = ao_fusionsound_close;
  this->ao_driver.exit              = ao_fusionsound_exit;
  this->ao_driver.get_gap_tolerance = ao_fusionsound_get_gap_tolerance;
  this->ao_driver.control            = ao_fusionsound_control;
  
  this->vol = this->amp = 1.0;

  return &this->ao_driver;
}

/*
 * class functions
 */

static char* get_identifier(audio_driver_class_t *ao_class) {
  return "FusionSound";
}

static char* get_description(audio_driver_class_t *ao_class) {
  return "xine FusionSound audio output plugin";
}

static void dispose_class(audio_driver_class_t *ao_class) {
  free (ao_class);
}

static void* init_class(xine_t *xine, void *data) {
  fusionsound_class_t *class;
  const char          *error;
  
  /* check FusionSound version */
  error = FusionSoundCheckVersion( FUSIONSOUND_MAJOR_VERSION,
                                   FUSIONSOUND_MINOR_VERSION,
                                   FUSIONSOUND_MICRO_VERSION );
  if (error) {
    xprintf (xine, XINE_VERBOSITY_NONE,
             "audio_fusionsound_out: %s!\n", error);
    return NULL;
  }

  class = (fusionsound_class_t *) xine_xmalloc (sizeof( fusionsound_class_t));
  if (!class) {
    xprintf (xine, XINE_VERBOSITY_LOG,
             "audio_fusionsound_out: class interface allocation failed!\n");
    return NULL;
  }

  class->ao_class.open_plugin     = open_plugin;
  class->ao_class.get_identifier  = get_identifier;
  class->ao_class.get_description = get_description;
  class->ao_class.dispose         = dispose_class;
  class->xine                     = xine;

  return class;
}

static ao_info_t ao_info_fusionsound = {
  4
};

/*
 * exported plugin catalog entry
 */

plugin_info_t xine_plugin_info[] = {
  /* type, API, "name", version, special_info, init_function */  
  { PLUGIN_AUDIO_OUT, AO_OUT_FS_IFACE_VERSION, "FusionSound",
    XINE_VERSION_CODE, &ao_info_fusionsound, init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
