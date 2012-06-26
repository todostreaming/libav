/*
 * Float MPEG Audio decoder
 * Copyright (c) 2010 Michael Niedermayer
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define CONFIG_FLOAT 1
#include "mpegaudiodec.c"

#if CONFIG_MP1FLOAT_DECODER
AVCodec ff_mp1float_decoder = {
    "mp1float",
    NULL_IF_CONFIG_SMALL("MP1 (MPEG audio layer 1)"),
    AVMEDIA_TYPE_AUDIO,
    CODEC_ID_MP1,
    CODEC_CAP_DR1,
    0, 0, 0, 0, 0, 0, 0, 0, sizeof(MPADecodeContext),
    0, 0, 0, 0, 0, decode_init,
    0, 0, decode_frame,
    0, flush,
};
#endif
#if CONFIG_MP2FLOAT_DECODER
AVCodec ff_mp2float_decoder = {
    "mp2float",
    NULL_IF_CONFIG_SMALL("MP2 (MPEG audio layer 2)"),
    AVMEDIA_TYPE_AUDIO,
    CODEC_ID_MP2,
    CODEC_CAP_DR1,
    0, 0, 0, 0, 0, 0, 0, 0, sizeof(MPADecodeContext),
    0, 0, 0, 0, 0, decode_init,
    0, 0, decode_frame,
    0, flush,
};
#endif
#if CONFIG_MP3FLOAT_DECODER
AVCodec ff_mp3float_decoder = {
    "mp3float",
    NULL_IF_CONFIG_SMALL("MP3 (MPEG audio layer 3)"),
    AVMEDIA_TYPE_AUDIO,
    CODEC_ID_MP3,
    CODEC_CAP_DR1,
    0, 0, 0, 0, 0, 0, 0, 0, sizeof(MPADecodeContext),
    0, 0, 0, 0, 0, decode_init,
    0, 0, decode_frame,
    0, flush,
};
#endif
#if CONFIG_MP3ADUFLOAT_DECODER
AVCodec ff_mp3adufloat_decoder = {
    "mp3adufloat",
    NULL_IF_CONFIG_SMALL("ADU (Application Data Unit) MP3 (MPEG audio layer 3)"),
    AVMEDIA_TYPE_AUDIO,
    CODEC_ID_MP3ADU,
    CODEC_CAP_DR1,
    0, 0, 0, 0, 0, 0, 0, 0, sizeof(MPADecodeContext),
    0, 0, 0, 0, 0, decode_init,
    0, 0, decode_frame_adu,
    0, flush,
};
#endif
#if CONFIG_MP3ON4FLOAT_DECODER
AVCodec ff_mp3on4float_decoder = {
    "mp3on4float",
    NULL_IF_CONFIG_SMALL("MP3onMP4"),
    AVMEDIA_TYPE_AUDIO,
    CODEC_ID_MP3ON4,
    CODEC_CAP_DR1,
    0, 0, 0, 0, 0, 0, 0, 0, sizeof(MP3On4DecodeContext),
    0, 0, 0, 0, 0, decode_init_mp3on4,
    0, 0, decode_frame_mp3on4,
    decode_close_mp3on4,
    flush_mp3on4,
};
#endif
