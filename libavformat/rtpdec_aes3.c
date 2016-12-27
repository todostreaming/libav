/*
 * Copyright (c) 2016 Luca Barbato
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

#include "libavutil/attributes.h"
#include "libavutil/channel_layout.h"

#include "avformat.h"
#include "rtpdec_formats.h"


static av_cold int aes3_rtp_init(AVFormatContext *s, int st_index, PayloadContext *data)
{
    AVStream *stream = s->streams[st_index];
    AVCodecParameters *par = stream->codecpar;

    par->channels              = 2;
    par->bits_per_coded_sample = 24;
    par->sample_rate           = 48000;

    par->bit_rate              = par->bits_per_coded_sample * par->sample_rate;
    par->channel_layout        = av_get_default_channel_layout(par->channels);

    return 0;
}

static int aes3_rtp_parse_packet(AVFormatContext *ctx, PayloadContext *data,
                                 AVStream *st, AVPacket *pkt, uint32_t *timestamp,
                                 const uint8_t *buf, int len, uint16_t seq, int flags)
{

    return 0;
}

RTPDynamicProtocolHandler ff_aes3_rtp_dynamic_handler = {
    .enc_name     = "RawAudio",
    .codec_type   = AVMEDIA_TYPE_AUDIO,
    .codec_id     = AV_CODEC_ID_PCM_S24BE,
    .init         = aes3_rtp_init,
    .parse_packet = aes3_rtp_parse_packet,
};


