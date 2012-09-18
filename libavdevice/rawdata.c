/*
 * Raw data input
 * Copyright (c) 2011 Luca Barbato <lu_zero@gentoo.org>
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

/**
 * @file
 * Generate a data stream by sampling a character device.
 * @author Luca Barbato <lu_zero@gentoo.org>
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libavformat/avformat.h"
#include "libavformat/internal.h"
#include "libavutil/opt.h"

typedef struct {
    AVClass *class;
    int threading;
    int timeout;
    int size;
    int64_t start_time;
    int64_t pts;
    int fd;
} RawData;


static int rawdata_read_header(AVFormatContext *s)
{
    RawData *rd = s->priv_data;
    AVStream *st = avformat_new_stream(s, NULL);

    st->codec->codec_type  = AVMEDIA_TYPE_DATA;
    st->codec->codec_id    = AV_CODEC_ID_TEXT;
    avpriv_set_pts_info(st, 64, 1, 1000000);

    rd->fd = open(s->filename, O_RDWR | O_NONBLOCK, 0);

    if (rd->fd < 0) {
        int err = errno;

        av_log(s, AV_LOG_ERROR, "Cannot open video device %s : %s\n",
               s->filename, strerror(err));

        return AVERROR(err);
    }

    return 0;
}

static int rawdata_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    RawData *rd = s->priv_data;
    int res;

    if (av_new_packet(pkt, rd->size) < 0) {
        return AVERROR(ENOMEM);
    }

    res = read(rd->fd, pkt->data, rd->size);

    if (rd->pts == AV_NOPTS_VALUE) {
        rd->pts = av_gettime() - rd->start_time;
        pkt->pts = rd->start_time;
    } else {
        rd->pts = pkt->pts = av_gettime() - rd->pts;
    }

    if (res < rd->size) {
        av_free_packet(pkt);
        return AVERROR(EAGAIN);
    }

    return 0;
}

static av_cold int rawdata_close(AVFormatContext *s)
{
    RawData *rd = s->priv_data;
    close(rd->fd);
    return 0;
}

#define OFFSET(a) offsetof(RawData, a)
#define D AV_OPT_FLAG_DECODING_PARAM

static const AVOption options[] = {
    { "timeout",        "poll time in milliseconds", OFFSET(timeout),   AV_OPT_TYPE_INT, {.i64 = 100},     -1, INT_MAX, D },
    { "threaded",       "use a polling thread",      OFFSET(threading), AV_OPT_TYPE_INT, {.i64 = 0},        0, 1,       D },
    { "size",           "amount of data to be stored in a packet",  OFFSET(size), AV_OPT_TYPE_INT, {.i64 = 128},        1, INT_MAX,       D },
    { "start_time",     "initial time in milliseconds",  OFFSET(size), AV_OPT_TYPE_INT64, {.i64 = 0},        0, INT64_MAX,       D },
};

static const AVClass rawdata_demuxer_class = {
    .class_name     = "Textdata reader",
    .item_name      = av_default_item_name,
    .option         = options,
    .version        = LIBAVUTIL_VERSION_INT,
};

AVInputFormat ff_rawdata_demuxer = {
    .name           = "rawdata",
    .long_name      = NULL_IF_CONFIG_SMALL("Raw rawdata input"),
    .priv_data_size = sizeof(RawData),
    .read_header    = rawdata_read_header,
    .read_packet    = rawdata_read_packet,
    .read_close     = rawdata_close,
    .flags          = AVFMT_NOFILE,
    .priv_class     = &rawdata_demuxer_class,
};

