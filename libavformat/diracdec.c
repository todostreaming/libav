/*
 * RAW Dirac demuxer
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

#include "libavutil/intreadwrite.h"

#include "avformat.h"
#include "internal.h"

#define VC2_SEQUENCE_HEADER  0x00
#define VC2_END_OF_SEQUENCE  0x10
#define VC2_HQ_PICTURE       0xE8
#define VC2_HEADER_SIZE (4 + 1 + 4 + 4)

static int dirac_probe(AVProbeData *p)
{
    if (AV_RL32(p->buf) == MKTAG('B', 'B', 'C', 'D'))
        return AVPROBE_SCORE_MAX;
    else
        return 0;
}

static int dirac_read_header(AVFormatContext *s)
{
    AVStream *st = avformat_new_stream(s, NULL);
    if (!st)
        return AVERROR(ENOMEM);

    st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    st->codecpar->codec_id   = AV_CODEC_ID_DIRAC;

    // TODO parse the reported framerate
    avpriv_set_pts_info(st, 64, 50, 1);

    return 0;
}

static int parse_header(AVFormatContext *s)
{
    uint32_t start_code = avio_rl32(s->pb);
    uint8_t  parse_code = avio_r8(s->pb);
    uint32_t next_off   = avio_rb32(s->pb);
    uint32_t prev_off   = avio_rb32(s->pb);

    if (start_code != MKTAG('B', 'B', 'C', 'D')) {
        av_log(NULL, AV_LOG_ERROR, "Bogus start_code %d\n",
               start_code);
        return AVERROR_INVALIDDATA;
    }

    av_log(s, AV_LOG_VERBOSE,
           "packet %s next %u prev %u \n",
           parse_code == VC2_SEQUENCE_HEADER ? "Sequence Header" :
           parse_code == VC2_HQ_PICTURE ? "HQ Picture" :
           parse_code == VC2_END_OF_SEQUENCE? "End of Sequence" :
           "Unknown",
           next_off,
           prev_off);

    avio_skip(s->pb, next_off - VC2_HEADER_SIZE);

    return parse_code;
}


static int dirac_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    int64_t pos = avio_tell(s->pb);
    int64_t end;
    int ret;

    // TODO resync
    while ((ret = parse_header(s)) != VC2_HQ_PICTURE) {
        if (s->pb->eof_reached)
            return AVERROR_EOF;
        if (ret < 0)
            return ret;
    }

    end = avio_tell(s->pb);

    avio_seek(s->pb, pos, SEEK_SET);

    return av_get_packet(s->pb, pkt, end - pos);
}

AVInputFormat ff_dirac_demuxer = {
    .name           = "dirac",
    .long_name      = NULL_IF_CONFIG_SMALL("DRC"),
    .read_probe     = dirac_probe,
    .read_header    = dirac_read_header,
    .read_packet    = dirac_read_packet,
    .extensions     = "drc,vc2",
    .raw_codec_id   = AV_CODEC_ID_DIRAC,
};
