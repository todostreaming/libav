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
    uint64_t init;
    uint64_t start;
    uint64_t origin;
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


static void
control_lost_cb (ArvDevice *device)
{
    av_log(NULL, AV_LOG_ERROR, "Control Lost\n");
}

static void stream_cb(void *user_data, ArvStreamCallbackType type,
                              ArvBuffer *buffer)
{
    AravisContext *t = user_data;
    uint64_t time_us = av_gettime();

    switch (type) {
    case ARV_STREAM_CALLBACK_TYPE_INIT:
	t->init = time_us;
	av_log(NULL, AV_LOG_INFO, "Init! %"G_GINT64_FORMAT"\n",
		time_us-t->origin);
//        av_log(NULL, AV_LOG_INFO, "%ld ARV Init\n", (time - g->start)/1000);
    break;
    case ARV_STREAM_CALLBACK_TYPE_EXIT:
//        av_log(NULL, AV_LOG_INFO, "%ld ARV Exit\n", (time - g->start)/1000);
    break;
    case ARV_STREAM_CALLBACK_TYPE_START_BUFFER:
        t->start = time_us;
	av_log(NULL, AV_LOG_INFO, "Buffer start at %"G_GINT64_FORMAT"\n",
               time_us-t->init);
//        av_log(NULL, AV_LOG_INFO, "%ld ARV Start Buffer\n", (time - g->start)/1000);
    break;
    case ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE:
	 av_log(NULL, AV_LOG_INFO, "Buffer %d done at %"G_GINT64_FORMAT""
                " timestamp %"G_GINT64_FORMAT" (%"G_GINT64_FORMAT")\n",
                buffer->frame_id,
                time_us-t->init,
		buffer->timestamp_ns,
		time_us-t->start);
        //av_log(NULL, AV_LOG_INFO, "%ld ARV Buffer Done\n", (time - g->start)/1000);
        //new_buffer_cb (buffer, g);
     break;
    }
}

static int aravis_read_header(AVFormatContext *s)
{
    AravisContext *c = s->priv_data;
    AVStream *st;
    int i;

    g_thread_init(NULL);
    g_type_init();

    c->camera = arv_camera_new(s->filename);

    arv_camera_stop_acquisition (c->camera);

    c->stream = arv_camera_create_stream(c->camera, stream_cb, c);

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

    c->origin = av_gettime();

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
    uint32_t *frame_id;
    uint64_t *start;

    if (!buffer || buffer->status != ARV_BUFFER_STATUS_SUCCESS)
        return AVERROR(EAGAIN);

    av_init_packet(pkt);

    pkt->data = buffer->data;
    pkt->size = buffer->size;
    pkt->pts  = buffer->timestamp_ns;
    pkt->destruct = aravis_free_buffer;

    frame_id = av_packet_new_side_data(pkt, 0, sizeof(uint32_t));

    *frame_id = buffer->frame_id;

    start = av_packet_new_side_data(pkt, 1, sizeof(uint64_t));

    *start = c->start;

    av_log(NULL, AV_LOG_INFO, "Got frame %d - %"PRId64"\n",
           buffer->frame_id, av_gettime() - c->start);

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
