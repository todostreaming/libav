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

    /* Encoder object. */
    f265_enc* enc;
    uint8_t* enc_buf;

    /* Encoder parameters. */
    f265_enc_params params;

    /* Encoder command context */
    f265_enc_req enc_req;

    /* Encoder input and output frames. */
    f265_input_frame input_frame;
    f265_output_frame output_frame;

    char *preset;
    char *tune;
    char *f265_opts;
} LibF265Context;


static av_cold int libf265_encode_close(AVCodecContext *avctx)
{
    LibF265Context *ctx = avctx->priv_data;

    av_frame_free(&avctx->coded_frame);

    av_free(ctx->output_frame.bs);
    av_free(ctx->enc_buf);

    return 0;
}

static av_cold int libf265_encode_init(AVCodecContext *avctx)
{
    LibF265Context *ctx     = avctx->priv_data;
    f265_output_frame *out  = ctx->enc_req.outputs = &ctx->output_frame;
    f265_enc_params *params = &ctx->params;
    char *err;
    int ret;

    ctx->enc_req.nb_outputs = 1;

    out->aud_flag     = 1;
    out->nal_size_len = 4;
    out->vps_flag     = 1;
    out->sps_flag     = 1;
    out->pps_flag     = 1;

    f265_parse_params(params, ctx->f265_opts, &err);
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "Error parsing f265-params: %s\n",
               err);
        return AVERROR(EINVAL);
    }

    params->clip_dim[0] = avctx->width;
    params->clip_dim[1] = avctx->height;

    if (avctx->max_b_frames >= 0)
        params->nb_b_frames = avctx->max_b_frames;

    if (avctx->bit_rate > 0) {
        params->bitrate[0] = avctx->bit_rate * 1024;
        params->rc_method  = F265_RCM_ABR;
    }

    if (avctx->gop_size >= 0)
        params->key_interval = avctx->gop_size;

    params->frame_rate_num = avctx->time_base.den;
    params->frame_rate_den = avctx->time_base.num * avctx->ticks_per_frame;

    if (avctx->qmin >= 0)
        params->qp_bounds[0] = avctx->qmin;
    if (avctx->qmax >= 0)
        params->qp_bounds[1] = avctx->qmax;

    switch (avctx->pix_fmt) {
    case AV_PIX_FMT_YUV420P:
        params->chroma_format = 1;
        break;
    case AV_PIX_FMT_YUV422P:
        params->chroma_format = 2;
        break;
    case AV_PIX_FMT_YUV444P:
        params->chroma_format = 3;
        break;
    }

    f265_normalize_params(params);
    f265_analyze_params(params);

    ctx->output_frame.format = params->format;
    ctx->output_frame.bs     = av_mallocz(params->bs_mem);

    if (!ctx->output_frame.bs) {
        libf265_encode_close(avctx);
        return AVERROR(ENOMEM);
    }

    ctx->enc_buf = av_mallocz(params->enc_mem);

    if (!ctx->enc_buf) {
        libf265_encode_close(avctx);
        return AVERROR(ENOMEM);
    }


    f265_init_enc(params, ctx->enc_buf, &ctx->enc, &err);

    avctx->coded_frame = av_frame_alloc();

    if (!avctx->coded_frame) {
        av_log(avctx, AV_LOG_ERROR, "Could not allocate frame.\n");
        return AVERROR(ENOMEM);
    }

    if (avctx->flags & CODEC_FLAG_GLOBAL_HEADER) {
        avctx->extradata_size =
            f265_encode_stream_headers_byte_format(ctx->enc, NULL);
        if (avctx->extradata_size <= 0) {
            av_log(avctx, AV_LOG_ERROR, "Cannot encode headers.\n");
            libf265_encode_close(avctx);
            return AVERROR_INVALIDDATA;
        }

        avctx->extradata =
            av_malloc(avctx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
        if (!avctx->extradata) {
            av_log(avctx, AV_LOG_ERROR,
                   "Cannot allocate HEVC header of size %d.\n",
                   avctx->extradata_size);
            libf265_encode_close(avctx);
            return AVERROR(ENOMEM);
        }
        f265_encode_stream_headers_byte_format(ctx->enc, avctx->extradata);

        memset(avctx->extradata, 0, avctx->extradata_size);
    }

    return 0;
}

#define F265_TIMEBASE (AVRational){ 1, 1000 * 1000 * 1000 }

static void load_frame(AVCodecContext *avctx,
                       f265_input_frame *in, const AVFrame *pic)
{
    int i;
    in->timestamp = av_rescale_q(pic->pts, avctx->time_base,
                                 F265_TIMEBASE);
    // FIXME use a framerate estimation
    in->duration  = av_rescale_q(1, avctx->time_base,
                                 F265_TIMEBASE);

    for (i = 0; i < 3; i++) {
        in->planes[i] = pic->data[i];
        in->stride[i] = pic->linesize[i];
    }
}

static int libf265_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                                const AVFrame *pic, int *got_packet)
{
    LibF265Context *ctx    = avctx->priv_data;
    f265_input_frame *in   = NULL;
    f265_output_frame *out = &ctx->enc_output_frame;

    uint8_t *dst;
    int payload = 0;
    int ret;
    int i;


    out->bs_size = 0;

    if (pic) {
        in = ctx->input_frame;
        load_frame(avctx, in, pic);
    }

    ctx->enc_req.input = in;

    ret = f265_process_enc_req(ctx->enc, &ctx->enc_req);

    *got_packet = 0;

    switch (ret) {
    case F265_RET_EMPTY:
        return 0;
    case F265_RET_NORES:
    case F265_RET_ERROR :
        av_log(avctx, AV_LOG_ERROR, "Codec error: %s.\n",
               ctx->enc_req.error_string);
        return AVERROR_UNKNOWN;
    default:
        return AVERROR_UNKNONW;
    case F265_RET_VALID:
        *got_packet = 1;
        break;
    }

    ret = ff_alloc_packet(pkt, out->bs_size);
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Error getting output packet.\n");
        return ret;
    }

    memcpy(pkt->data, out->bs, out->bs_size);

    pkt->pts = av_rescale_q(out->timestamp, F264_TIMEBASE, avctx->time_base);

    // FIXME no idea which is which.
    pkt->dts;

    return 0;
}

static av_cold void libf265_encode_init_static_data(AVCodec *codec)
{
    f265_init_global(0);
    f265_init_parsing();
}

#define OFFSET(x) offsetof(LibF265Context, x)
#define VE AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
/*    { "preset",      "set the f265 preset",                                                         OFFSET(preset),    AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, VE },
    { "tune",        "set the f265 tune parameter",                                                 OFFSET(tune),      AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, VE }, */
    { "f265-params", "set the f265 configuration using a :-separated list of key=value parameters", OFFSET(f265_opts), AV_OPT_TYPE_STRING, { .str = "" }, 0, 0, VE },
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
