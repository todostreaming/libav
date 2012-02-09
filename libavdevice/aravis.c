/*
 * Aravis capture interface
 * Copyright (c) 2012 Luca Barbato
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

#include "config.h"
#include "libavformat/avformat.h"
#include "libavformat/internal.h"
#include "libavutil/imgutils.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/parseutils.h"
#include "libavutil/pixdesc.h"
#include "libavutil/avstring.h"
#include "libavutil/mathematics.h"

#include <arv.h>

static const int desired_video_buffers = 256;

typedef struct {
//    AVClass *class;
    ArvCamera *camera;
    ArvStream *stream;
    int payload;
} AravisContext;

typedef struct {
    enum PixelFormat avpf;
    ArvPixelFormat arpf;
} FormatMap;

static FormatMap fmt_conversion_table[] = {
/*    { ARV_PIXEL_FORMAT_YUV_411_PACKED, },
    { ARV_PIXEL_FORMAT_YUV_422_PACKED, },
    { ARV_PIXEL_FORMAT_YUV_444_PACKED, }, */
    { ARV_PIXEL_FORMAT_YUV_422_YUYV_PACKED, PIX_FMT_YUYV422 },
/*
        ARV_PIXEL_FORMAT_RGB_8_PACKED
        ARV_PIXEL_FORMAT_BGR_8_PACKED

        ARV_PIXEL_FORMAT_RGBA_8_PACKED
        ARV_PIXEL_FORMAT_BGRA_8_PACKED

        ARV_PIXEL_FORMAT_RGB_10_PACKED
        ARV_PIXEL_FORMAT_BGR_10_PACKED */
};

static int aravis_read_header(AVFormatContext *s)
{
    AravisContext *c = s->priv_data;
    AVStream *st;
    int i;

    g_thread_init(NULL);
    g_type_init();

    c->camera = arv_camera_new(s->filename);
    c->stream = arv_camera_create_stream(c->camera, NULL, NULL);

    c->payload = arv_camera_get_payload(c->camera);

    for (i = 0; i < 50; i++)
        arv_stream_push_buffer(c->stream, arv_buffer_new(c->payload, NULL));

    arv_camera_set_acquisition_mode(c->camera, ARV_ACQUISITION_MODE_CONTINUOUS);


    st = avformat_new_stream(s, NULL);

    avpriv_set_pts_info(st, 64, 1, 1000000);

    st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    st->codec->pix_fmt = PIX_FMT_YUYV422;
    st->codec->codec_id = CODEC_ID_RAWVIDEO;
    st->codec->width = 1280;
    st->codec->height = 720;

    arv_camera_start_acquisition(c->camera);

    return 0;
}

typedef struct {
    ArvBuffer *buffer;
    ArvStream *stream;
} DestructContext;

static void aravis_free_buffer(AVPacket *pkt)
{
    DestructContext *c = pkt->priv;

    arv_stream_push_buffer (c->stream, c->buffer);
}

static int aravis_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    AravisContext *c = s->priv_data;
    ArvBuffer *buffer = arv_stream_timed_pop_buffer(c->stream, 2000000);
    DestructContext *des = av_malloc(sizeof(DestructContext));

    if (!buffer || buffer->status != ARV_BUFFER_STATUS_SUCCESS)
        return AVERROR(EAGAIN);

    av_init_packet(pkt);

    pkt->data = buffer->data;
    pkt->size = buffer->size;
    pkt->pts  = buffer->timestamp_ns;
    pkt->destruct = aravis_free_buffer;

    des->buffer = buffer;
    des->stream = c->stream;

    pkt->priv = des;

    return pkt->size;
}

static int aravis_read_close(AVFormatContext *s)
{
    AravisContext *c = s->priv_data;

    arv_camera_stop_acquisition(c->camera);

    g_object_unref(c->stream);

    g_object_unref(c->camera);
}


AVInputFormat ff_aravis_demuxer = {
    .name           = "aravis",
    .long_name      = NULL_IF_CONFIG_SMALL("Aravis capture"),
    .priv_data_size = sizeof(AravisContext),
    .read_header    = aravis_read_header,
    .read_packet    = aravis_read_packet,
    .read_close     = aravis_read_close,
    .flags          = AVFMT_NOFILE,
//    .priv_class     = &aravis_class,
};
