/*
 * frame information dumper
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

#include <inttypes.h>

#include "avformat.h"
#include "internal.h"

static int frameinfo_write_packet(struct AVFormatContext *s, AVPacket *pkt)
{
    char buf[256];
    AVFrame *frame = (AVFrame *)pkt->data;

    snprintf(buf, sizeof(buf), "%d, %10"PRId64", %10"PRId64", %8"PRId64", %8d, Interlaced %d Top Field First %d\n",
             pkt->stream_index, pkt->dts, pkt->pts, pkt->duration, frame->interlaced_frame, frame->top_field_first);
    avio_write(s->pb, buf, strlen(buf));
    return 0;
}

AVOutputFormat ff_frameinfo_muxer = {
    .name              = "frameinfo",
    .long_name         = NULL_IF_CONFIG_SMALL("frame information"),
    .extensions        = "",
    .audio_codec       = AV_CODEC_ID_NONE,
    .video_codec       = AV_CODEC_ID_WRAPPED_AVFRAME,
    .write_packet      = frameinfo_write_packet,
    .flags             = AVFMT_VARIABLE_FPS | AVFMT_TS_NONSTRICT |
                         AVFMT_TS_NEGATIVE,
};
