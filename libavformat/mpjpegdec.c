/*
 * Multipart JPEG format
 * Copyright (c) 2014 Luca Barbato
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

#include "libavutil/avstring.h"

#include "avformat.h"
#include "internal.h"

static int get_line(AVIOContext *pb, char *line, int line_size)
{
    int i, ch;
    char *q = line;

    for (i = 0; !pb->eof_reached; i++) {
        ch = avio_r8(pb);
        if (ch == '\n') {
            if (q > line && q[-1] == '\r')
                q--;
            *q = '\0';

            return 0;
        } else {
            if ((q - line) < line_size - 1)
                *q++ = ch;
        }
    }

    if (pb->error)
        return pb->error;
    return AVERROR_EOF;
}

static int split_tag_value(char **tag, char **value, char *line)
{
    char *p = line;

    while (*p != '\0' && *p != ':')
        p++;
    if (*p != ':')
        return AVERROR_INVALIDDATA;

    *p   = '\0';
    *tag = line;

    p++;

    while (av_isspace(*p))
        p++;

    *value = p;

    return 0;
}

static int check_content_type(char *line)
{
    char *tag, *value;
    int ret = split_tag_value(&tag, &value, line);

    if (ret < 0)
        return ret;

    if (av_strcasecmp(tag, "Content-type") ||
        av_strcasecmp(value, "image/jpeg"))
        return AVERROR_INVALIDDATA;

    return 0;
}

static int mpjpeg_read_probe(AVProbeData *p)
{
    AVIOContext *pb;
    char line[128] = { 0 };
    int ret;

    pb = avio_alloc_context(p->buf, p->buf_size, 0, NULL, NULL, NULL, NULL);
    if (!pb)
        return AVERROR(ENOMEM);

    while (!pb->eof_reached) {
        ret = get_line(pb, line, sizeof(line));
        if (ret < 0)
            break;

        ret = check_content_type(line);
        if (!ret)
            return AVPROBE_SCORE_MAX;
    }

    return 0;
}

static int mpjpeg_read_header(AVFormatContext *s)
{
    AVStream *st;
    char boundary[70 + 2 + 1];
    int ret;

    ret = get_line(s->pb, boundary, sizeof(boundary));
    if (ret < 0)
        return ret;

    if (strncmp(boundary, "--", 2))
        return AVERROR_INVALIDDATA;

    st = avformat_new_stream(s, NULL);

    st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    st->codec->codec_id   = AV_CODEC_ID_MJPEG;

    avpriv_set_pts_info(st, 60, 1, 25);

    return 0;
}

static int parse_content_length(char *line)
{
    char *tag, *value;
    int ret = split_tag_value(&tag, &value, line);
    long int val;

    if (ret < 0)
        return ret;

    if (av_strcasecmp(tag, "Content-Length"))
        return AVERROR_INVALIDDATA;

    val = strtol(value, NULL, 10);
    if (val == LONG_MIN || val == LONG_MAX)
        return AVERROR(errno);
    if (val > INT_MAX)
        return AVERROR(ERANGE);
    return val;
}

static int mpjpeg_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    char line[128];
    int ret, size;

    ret = get_line(s->pb, line, sizeof(line));
    if (ret < 0)
        return ret;

    ret = check_content_type(line);
    if (ret < 0)
        return ret;

    ret = get_line(s->pb, line, sizeof(line));
    if (ret < 0)
        return ret;

    size = parse_content_length(line);
    if (size < 0)
        return size;

    ret = av_new_packet(pkt, size);
    if (ret < 0)
        return ret;

    ret = avio_read(s->pb, pkt->data, size);
    if (ret < 0)
        goto fail;

    // Consume the boundary marker
    ret = get_line(s->pb, line, sizeof(line));
    if (ret < 0)
        goto fail;

    return ret;

fail:
    av_free_packet(pkt);
    return ret;
}

AVInputFormat ff_mpjpeg_demuxer = {
    .name              = "mpjpeg",
    .long_name         = NULL_IF_CONFIG_SMALL("MIME multipart JPEG"),
    .mime_type         = "multipart/x-mixed-replace",
    .extensions        = "mjpg",
    .read_probe        = mpjpeg_read_probe,
    .read_header       = mpjpeg_read_header,
    .read_packet       = mpjpeg_read_packet,
};
