/*
 * libf265 encoder
 *
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

#include <f265/f265.h>

#include "libavutil/internal.h"
#include "libavutil/common.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "avcodec.h"
#include "internal.h"

typedef struct LibF265Context {
    const AVClass *class;
    char *preset;
    char *tune;
    char *f265_opts;
} LibF265Context;


static av_cold int libf265_encode_close(AVCodecContext *avctx)
{
    LibF265Context *ctx = avctx->priv_data;

    av_frame_free(&avctx->coded_frame);

    return 0;
}

static av_cold int libf265_encode_init(AVCodecContext *avctx)
{
    LibF265Context *ctx = avctx->priv_data;

    avctx->coded_frame = av_frame_alloc();

    if (!avctx->coded_frame) {
        av_log(avctx, AV_LOG_ERROR, "Could not allocate frame.\n");
        return AVERROR(ENOMEM);
    }

    switch (avctx->pix_fmt) {
    case AV_PIX_FMT_YUV420P:
        break;
    }

    if (avctx->bit_rate > 0) {
    }

    if (!(avctx->flags & CODEC_FLAG_GLOBAL_HEADER)) {
    }


    if (avctx->flags & CODEC_FLAG_GLOBAL_HEADER) {
        avctx->extradata_size = 1;
        if (avctx->extradata_size <= 0) {
            av_log(avctx, AV_LOG_ERROR, "Cannot encode headers.\n");
            libf265_encode_close(avctx);
            return AVERROR_INVALIDDATA;
        }

        avctx->extradata = av_malloc(avctx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
        if (!avctx->extradata) {
            av_log(avctx, AV_LOG_ERROR,
                   "Cannot allocate HEVC header of size %d.\n", avctx->extradata_size);
            libf265_encode_close(avctx);
            return AVERROR(ENOMEM);
        }
    }

    return 0;
}

static int libf265_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                                const AVFrame *pic, int *got_packet)
{
    LibF265Context *ctx = avctx->priv_data;
    uint8_t *dst;
    int payload = 0;
    int ret;
    int i;


    ret = ff_alloc_packet(pkt, payload);
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Error getting output packet.\n");
        return ret;
    }

    dst = pkt->data;

    pkt->pts;
    pkt->dts;

    *got_packet = 1;
    return 0;
}

static av_cold void libf265_encode_init_static_data(AVCodec *codec)
{
}

#define OFFSET(x) offsetof(LibF265Context, x)
#define VE AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    { "preset",      "set the f265 preset",                                                         OFFSET(preset),    AV_OPT_TYPE_STRING, { 0 }, 0, 0, VE },
    { "tune",        "set the f265 tune parameter",                                                 OFFSET(tune),      AV_OPT_TYPE_STRING, { 0 }, 0, 0, VE },
    { "f265-params", "set the f265 configuration using a :-separated list of key=value parameters", OFFSET(f265_opts), AV_OPT_TYPE_STRING, { 0 }, 0, 0, VE },
    { NULL }
};

static const AVClass class = {
    .class_name = "libf265",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

static const enum AVPixelFormat f265_pix_fmts[] = {
    AV_PIX_FMT_YUV420P,
    AV_PIX_FMT_YUV422P,
    AV_PIX_FMT_YUV444P,
    AV_PIX_FMT_NONE
};

AVCodec ff_libf265_encoder = {
    .name             = "libf265",
    .long_name        = NULL_IF_CONFIG_SMALL("libf265 H.265 / HEVC"),
    .type             = AVMEDIA_TYPE_VIDEO,
    .id               = AV_CODEC_ID_HEVC,
    .init             = libf265_encode_init,
    .init_static_data = libf265_encode_init_static_data,
    .encode2          = libf265_encode_frame,
    .close            = libf265_encode_close,
    .priv_data_size   = sizeof(LibF265Context),
    .priv_class       = &class,
    .pix_fmts         = f265_pix_fmts,
    .capabilities     = CODEC_CAP_DELAY | CODEC_CAP_AUTO_THREADS,
};
