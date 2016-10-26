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
#include "avio_internal.h"
#include "internal.h"

#define VC2_SEQUENCE_HEADER  0x00
#define VC2_END_OF_SEQUENCE  0x10
#define VC2_HQ_PICTURE       0xE8
#define VC2_HEADER_SIZE (4 + 1 + 4 + 4)

typedef struct DiracContext {
    AVIOContext *buf;
} DiracContext;


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
    DiracContext *drc = s->priv_data;
    uint8_t *buf;

    uint32_t start_code = avio_rl32(s->pb);
    uint8_t  parse_code = avio_r8(s->pb);
    uint32_t next_off   = avio_rb32(s->pb);
    uint32_t prev_off   = avio_rb32(s->pb);

    if (s->pb->eof_reached)
        return AVERROR_EOF;

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

    // TODO skip end of sequence
    if (parse_code == VC2_END_OF_SEQUENCE)
        return AVERROR_EOF;

    // TODO make less wasteful
    avio_wl32(drc->buf, start_code);
    avio_w8(drc->buf, parse_code);
    avio_wb32(drc->buf, next_off);
    avio_wb32(drc->buf, prev_off);

    buf = av_mallocz(next_off);

    avio_read(s->pb, buf, next_off - VC2_HEADER_SIZE);

    avio_write(drc->buf, buf, next_off - VC2_HEADER_SIZE);

    av_free(buf);

    return parse_code;
}

static int dirac_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    DiracContext *drc = s->priv_data;
    int ret;

    if (!drc->buf) {
        ret = avio_open_dyn_buf(&drc->buf);
        if (ret < 0)
            return ret;
    }

    // TODO resync
    while ((ret = parse_header(s)) != VC2_HQ_PICTURE) {
        if (ret < 0)
            return ret;
    }

    pkt->size = avio_close_dyn_buf(drc->buf, &pkt->data);
    drc->buf = NULL;

    if ((ret = av_packet_from_data(pkt, pkt->data, pkt->size)) < 0) {
        av_freep(&pkt->data);
        return ret;
    }

    return pkt->size;
}

static int dirac_read_close(AVFormatContext *s)
{
    DiracContext *drc = s->priv_data;

    ffio_free_dyn_buf(&drc->buf);

    return 0;
}

AVInputFormat ff_dirac_demuxer = {
    .name           = "dirac",
    .long_name      = NULL_IF_CONFIG_SMALL("DRC"),
    .priv_data_size = sizeof(DiracContext),
    .read_probe     = dirac_probe,
    .read_header    = dirac_read_header,
    .read_packet    = dirac_read_packet,
    .read_close     = dirac_read_close,
    .extensions     = "drc,vc2",
    .raw_codec_id   = AV_CODEC_ID_DIRAC,
};
