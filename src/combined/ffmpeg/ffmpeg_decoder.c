/*
 * Copyright (C) 2001-2004 the xine project
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
 * xine decoder plugin using ffmpeg
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "../../libffmpeg/ffmpeg_config.h"
#endif

#include "xine_internal.h"

#include "ffmpeg_decoder.h"
#include "ffmpeg_compat.h"

/*
 * common initialisation
 */

pthread_once_t once_control = PTHREAD_ONCE_INIT;
pthread_mutex_t ffmpeg_lock;

#ifndef HAVE_FFMPEG

#define REGISTER_ENCODER(X,x) \
          if(ENABLE_##X##_ENCODER)  register_avcodec(&x##_encoder)
#define REGISTER_DECODER(X,x) \
          if(ENABLE_##X##_DECODER)  register_avcodec(&x##_decoder)
#define REGISTER_ENCDEC(X,x)  REGISTER_ENCODER(X,x); REGISTER_DECODER(X,x)

#define REGISTER_PARSER(X,x) \
          if(ENABLE_##X##_PARSER)  av_register_codec_parser(&x##_parser)

/* If you do not call this function, then you can select exactly which
   formats you want to support */

/**
 * simple call to register all the codecs.
 */
void avcodec_register_all(void)
{
    static int inited = 0;

    if (inited != 0)
        return;
    inited = 1;

    /* video codecs */
    REGISTER_DECODER(AASC, aasc);
    REGISTER_ENCDEC (ASV1, asv1);
    REGISTER_ENCDEC (ASV2, asv2);
    REGISTER_DECODER(AVS, avs);
    REGISTER_DECODER(BMP, bmp);
    REGISTER_DECODER(CAVS, cavs);
    REGISTER_DECODER(CINEPAK, cinepak);
    REGISTER_DECODER(CLJR, cljr);
    REGISTER_DECODER(CSCD, cscd);
    REGISTER_DECODER(CYUV, cyuv);
    REGISTER_DECODER(DSICINVIDEO, dsicinvideo);
    REGISTER_ENCDEC (DVVIDEO, dvvideo);
    REGISTER_DECODER(EIGHTBPS, eightbps);
    REGISTER_ENCDEC (FFV1, ffv1);
    REGISTER_ENCDEC (FFVHUFF, ffvhuff);
    REGISTER_DECODER(FLASHSV, flashsv);
    REGISTER_DECODER(FLIC, flic);
    REGISTER_ENCDEC (FLV, flv);
    REGISTER_DECODER(FOURXM, fourxm);
    REGISTER_DECODER(FRAPS, fraps);
    REGISTER_ENCDEC (GIF, gif);
    REGISTER_ENCDEC (H261, h261);
    REGISTER_ENCDEC (H263, h263);
    REGISTER_DECODER(H263I, h263i);
    REGISTER_ENCODER(H263P, h263p);
    REGISTER_DECODER(H264, h264);
    REGISTER_ENCDEC (HUFFYUV, huffyuv);
    REGISTER_DECODER(IDCIN, idcin);
    REGISTER_DECODER(INDEO2, indeo2);
    REGISTER_DECODER(INDEO3, indeo3);
    REGISTER_DECODER(INTERPLAY_VIDEO, interplay_video);
    REGISTER_ENCODER(JPEGLS, jpegls);
    REGISTER_DECODER(KMVC, kmvc);
    REGISTER_ENCODER(LJPEG, ljpeg);
    REGISTER_DECODER(LOCO, loco);
    REGISTER_DECODER(MDEC, mdec);
    REGISTER_ENCDEC (MJPEG, mjpeg);
    REGISTER_DECODER(MJPEGB, mjpegb);
    REGISTER_DECODER(MMVIDEO, mmvideo);
#ifdef HAVE_XVMC
    REGISTER_DECODER(MPEG_XVMC, mpeg_xvmc);
#endif
    REGISTER_ENCDEC (MPEG1VIDEO, mpeg1video);
    REGISTER_ENCDEC (MPEG2VIDEO, mpeg2video);
    REGISTER_ENCDEC (MPEG4, mpeg4);
    REGISTER_DECODER(MPEGVIDEO, mpegvideo);
    REGISTER_ENCDEC (MSMPEG4V1, msmpeg4v1);
    REGISTER_ENCDEC (MSMPEG4V2, msmpeg4v2);
    REGISTER_ENCDEC (MSMPEG4V3, msmpeg4v3);
    REGISTER_DECODER(MSRLE, msrle);
    REGISTER_DECODER(MSVIDEO1, msvideo1);
    REGISTER_DECODER(MSZH, mszh);
    REGISTER_DECODER(NUV, nuv);
    REGISTER_ENCODER(PAM, pam);
    REGISTER_ENCODER(PBM, pbm);
    REGISTER_ENCODER(PGM, pgm);
    REGISTER_ENCODER(PGMYUV, pgmyuv);
#ifdef CONFIG_ZLIB
    REGISTER_ENCDEC (PNG, png);
#endif
    REGISTER_ENCODER(PPM, ppm);
    REGISTER_DECODER(QDRAW, qdraw);
    REGISTER_DECODER(QPEG, qpeg);
    REGISTER_DECODER(QTRLE, qtrle);
    REGISTER_ENCDEC (RAWVIDEO, rawvideo);
    REGISTER_DECODER(ROQ, roq);
    REGISTER_DECODER(RPZA, rpza);
    REGISTER_ENCDEC (RV10, rv10);
    REGISTER_ENCDEC (RV20, rv20);
    REGISTER_DECODER(SMACKER, smacker);
    REGISTER_DECODER(SMC, smc);
    REGISTER_ENCDEC (SNOW, snow);
    REGISTER_DECODER(SP5X, sp5x);
    REGISTER_ENCDEC (SVQ1, svq1);
    REGISTER_DECODER(SVQ3, svq3);
    REGISTER_DECODER(TARGA, targa);
    REGISTER_DECODER(THEORA, theora);
    REGISTER_DECODER(TIERTEXSEQVIDEO, tiertexseqvideo);
    REGISTER_DECODER(TIFF, tiff);
    REGISTER_DECODER(TRUEMOTION1, truemotion1);
    REGISTER_DECODER(TRUEMOTION2, truemotion2);
    REGISTER_DECODER(TSCC, tscc);
    REGISTER_DECODER(ULTI, ulti);
    REGISTER_DECODER(VC1, vc1);
    REGISTER_DECODER(VCR1, vcr1);
    REGISTER_DECODER(VMDVIDEO, vmdvideo);
    REGISTER_DECODER(VMNC, vmnc);
    REGISTER_DECODER(VP3, vp3);
    REGISTER_DECODER(VP5, vp5);
    REGISTER_DECODER(VP6, vp6);
    REGISTER_DECODER(VP6F, vp6f);
    REGISTER_DECODER(VQA, vqa);
    REGISTER_ENCDEC (WMV1, wmv1);
    REGISTER_ENCDEC (WMV2, wmv2);
    REGISTER_DECODER(WMV3, wmv3);
    REGISTER_DECODER(WNV1, wnv1);
#ifdef CONFIG_X264
    REGISTER_ENCODER(X264, x264);
#endif
    REGISTER_DECODER(XAN_WC3, xan_wc3);
    REGISTER_DECODER(XL, xl);
#ifdef CONFIG_XVID
    REGISTER_ENCODER(XVID, xvid);
#endif
    REGISTER_ENCDEC (ZLIB, zlib);
#ifdef CONFIG_ZLIB
    REGISTER_ENCDEC (ZMBV, zmbv);
#endif

    /* audio codecs */
#ifdef CONFIG_LIBFAAD
    REGISTER_DECODER(AAC, aac);
    REGISTER_DECODER(MPEG4AAC, mpeg4aac);
#endif
#ifdef CONFIG_LIBA52
    REGISTER_DECODER(AC3, ac3);
#endif
    REGISTER_ENCODER(AC3, ac3);
#ifdef CODEC_ID_EAC3
    REGISTER_DECODER(EAC3, eac3);
#endif
    REGISTER_DECODER(ALAC, alac);
#if defined(CONFIG_AMR_NB) || defined(CONFIG_AMR_NB_FIXED)
    REGISTER_ENCDEC (AMR_NB, amr_nb);
#endif
#ifdef CONFIG_AMR_WB
    REGISTER_ENCDEC (AMR_WB, amr_wb);
#endif
    REGISTER_DECODER(COOK, cook);
    REGISTER_DECODER(DSICINAUDIO, dsicinaudio);
#ifdef CONFIG_LIBDTS
    REGISTER_DECODER(DTS, dts);
#endif
#ifdef CONFIG_LIBFAAC
    REGISTER_ENCODER(FAAC, faac);
#endif
    REGISTER_ENCDEC (FLAC, flac);
    REGISTER_DECODER(IMC, imc);
#ifdef CONFIG_LIBGSM
    REGISTER_ENCDEC (LIBGSM, libgsm);
#endif
    REGISTER_DECODER(MACE3, mace3);
    REGISTER_DECODER(MACE6, mace6);
    REGISTER_ENCDEC (MP2, mp2);
    REGISTER_DECODER(MP3, mp3);
    REGISTER_DECODER(MP3ADU, mp3adu);
#ifdef CONFIG_LIBMP3LAME
    REGISTER_ENCODER(MP3LAME, mp3lame);
#endif
    REGISTER_DECODER(MP3ON4, mp3on4);
    REGISTER_DECODER(MPC7, mpc7);
#ifdef CONFIG_LIBVORBIS
    if (!ENABLE_VORBIS_ENCODER)  REGISTER_ENCODER(OGGVORBIS, oggvorbis);
    if (!ENABLE_VORBIS_DECODER)  REGISTER_DECODER(OGGVORBIS, oggvorbis);
#endif
    REGISTER_DECODER(QDM2, qdm2);
    REGISTER_DECODER(RA_144, ra_144);
    REGISTER_DECODER(RA_288, ra_288);
    REGISTER_DECODER(SHORTEN, shorten);
    REGISTER_DECODER(SMACKAUD, smackaud);
    REGISTER_ENCDEC (SONIC, sonic);
    REGISTER_ENCODER(SONIC_LS, sonic_ls);
    REGISTER_DECODER(TRUESPEECH, truespeech);
    REGISTER_DECODER(TTA, tta);
    REGISTER_DECODER(VMDAUDIO, vmdaudio);
    REGISTER_ENCDEC (VORBIS, vorbis);
    REGISTER_DECODER(WAVPACK, wavpack);
    REGISTER_DECODER(WMAV1, wmav1);
    REGISTER_DECODER(WMAV2, wmav2);
    REGISTER_DECODER(WS_SND1, ws_snd1);

    /* pcm codecs */
    REGISTER_ENCDEC (PCM_ALAW, pcm_alaw);
    REGISTER_ENCDEC (PCM_MULAW, pcm_mulaw);
    REGISTER_ENCDEC (PCM_S8, pcm_s8);
    REGISTER_ENCDEC (PCM_S16BE, pcm_s16be);
    REGISTER_ENCDEC (PCM_S16LE, pcm_s16le);
    REGISTER_ENCDEC (PCM_S24BE, pcm_s24be);
    REGISTER_ENCDEC (PCM_S24DAUD, pcm_s24daud);
    REGISTER_ENCDEC (PCM_S24LE, pcm_s24le);
    REGISTER_ENCDEC (PCM_S32BE, pcm_s32be);
    REGISTER_ENCDEC (PCM_S32LE, pcm_s32le);
    REGISTER_ENCDEC (PCM_U8, pcm_u8);
    REGISTER_ENCDEC (PCM_U16BE, pcm_u16be);
    REGISTER_ENCDEC (PCM_U16LE, pcm_u16le);
    REGISTER_ENCDEC (PCM_U24BE, pcm_u24be);
    REGISTER_ENCDEC (PCM_U24LE, pcm_u24le);
    REGISTER_ENCDEC (PCM_U32BE, pcm_u32be);
    REGISTER_ENCDEC (PCM_U32LE, pcm_u32le);

    /* dpcm codecs */
    REGISTER_DECODER(INTERPLAY_DPCM, interplay_dpcm);
    REGISTER_DECODER(ROQ_DPCM, roq_dpcm);
    REGISTER_DECODER(SOL_DPCM, sol_dpcm);
    REGISTER_DECODER(XAN_DPCM, xan_dpcm);

    /* adpcm codecs */
    REGISTER_ENCDEC (ADPCM_4XM, adpcm_4xm);
    REGISTER_ENCDEC (ADPCM_ADX, adpcm_adx);
    REGISTER_ENCDEC (ADPCM_CT, adpcm_ct);
    REGISTER_ENCDEC (ADPCM_EA, adpcm_ea);
    REGISTER_ENCDEC (ADPCM_G726, adpcm_g726);
    REGISTER_ENCDEC (ADPCM_IMA_DK3, adpcm_ima_dk3);
    REGISTER_ENCDEC (ADPCM_IMA_DK4, adpcm_ima_dk4);
    REGISTER_ENCDEC (ADPCM_IMA_QT, adpcm_ima_qt);
    REGISTER_ENCDEC (ADPCM_IMA_SMJPEG, adpcm_ima_smjpeg);
    REGISTER_ENCDEC (ADPCM_IMA_WAV, adpcm_ima_wav);
    REGISTER_ENCDEC (ADPCM_IMA_WS, adpcm_ima_ws);
    REGISTER_ENCDEC (ADPCM_MS, adpcm_ms);
    REGISTER_ENCDEC (ADPCM_SBPRO_2, adpcm_sbpro_2);
    REGISTER_ENCDEC (ADPCM_SBPRO_3, adpcm_sbpro_3);
    REGISTER_ENCDEC (ADPCM_SBPRO_4, adpcm_sbpro_4);
    REGISTER_ENCDEC (ADPCM_SWF, adpcm_swf);
    REGISTER_ENCDEC (ADPCM_XA, adpcm_xa);
    REGISTER_ENCDEC (ADPCM_YAMAHA, adpcm_yamaha);

    /* subtitles */
    REGISTER_ENCDEC (DVBSUB, dvbsub);
    REGISTER_ENCDEC (DVDSUB, dvdsub);

    /* parsers */
    REGISTER_PARSER (AAC, aac);
    REGISTER_PARSER (AC3, ac3);
    REGISTER_PARSER (CAVSVIDEO, cavsvideo);
    REGISTER_PARSER (DVBSUB, dvbsub);
    REGISTER_PARSER (DVDSUB, dvdsub);
    REGISTER_PARSER (H261, h261);
    REGISTER_PARSER (H263, h263);
    REGISTER_PARSER (H264, h264);
    REGISTER_PARSER (MJPEG, mjpeg);
    REGISTER_PARSER (MPEG4VIDEO, mpeg4video);
    REGISTER_PARSER (MPEGAUDIO, mpegaudio);
    REGISTER_PARSER (MPEGVIDEO, mpegvideo);
    REGISTER_PARSER (PNM, pnm);

    /*
    av_register_bitstream_filter(&dump_extradata_bsf);
    av_register_bitstream_filter(&remove_extradata_bsf);
    av_register_bitstream_filter(&noise_bsf);
    av_register_bitstream_filter(&mp3_header_compress_bsf);
    av_register_bitstream_filter(&mp3_header_decompress_bsf);
    av_register_bitstream_filter(&mjpega_dump_header_bsf);
    */
}

#endif

void init_once_routine(void) {
  pthread_mutex_init(&ffmpeg_lock, NULL);
  avcodec_init();
  avcodec_register_all();
}

/*
 * exported plugin catalog entry
 */

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_DECODER | PLUGIN_MUST_PRELOAD, 18, "ffmpegvideo", XINE_VERSION_CODE, &dec_info_ffmpeg_video, init_video_plugin },
  { PLUGIN_VIDEO_DECODER, 18, "ffmpeg-wmv8", XINE_VERSION_CODE, &dec_info_ffmpeg_wmv8, init_video_plugin },
  { PLUGIN_VIDEO_DECODER, 18, "ffmpeg-wmv9", XINE_VERSION_CODE, &dec_info_ffmpeg_wmv9, init_video_plugin },
  { PLUGIN_AUDIO_DECODER, 15, "ffmpegaudio", XINE_VERSION_CODE, &dec_info_ffmpeg_audio, init_audio_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
