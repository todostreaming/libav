/*
 * Intel MediaSDK based H.264 QSV HWAccel
 *
 * Copyright (C) 2014 Luca Barbato
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


#include <stdint.h>
#include <sys/types.h>
#include <mfx/mfxvideo.h>

#include "libavutil/opt.h"

#include "avcodec.h"
#include "internal.h"
#include "h264.h"
#include "qsv.h"
#include "qsv_internal.h"

typedef struct QSVH264Context {
    AVClass *class;
    QSVContext qsv;
    AVBitStreamFilterContext *bsf;
} QSVH264Context;

static const uint8_t fake_idr[] = { 0x00, 0x00, 0x01, 0x65 };

av_cold int av_qsv_default_free(AVCodecContext *avctx)
{
    QSVH264Context *q = avctx->hwaccel_context;
    int ret = ff_qsv_close(&q->qsv);

    av_bitstream_filter_close(q->bsf);
    av_freep(&q->qsv.bs.Data);
    av_freep(&q);

    return ret;
}

static int qsv_dec_frame(AVCodecContext *avctx, void *data,
                         int *got_frame, AVPacket *avpkt)
{
    QSVH264Context *q = avctx->hwaccel_context;
    AVFrame *frame    = data;
    uint8_t *p;
    int size;
    int ret;

    av_bitstream_filter_filter(q->bsf, avctx, NULL,
                               &p, &size,
                               avpkt->data, avpkt->size, 0);

    if (p != avpkt->data) {
        AVPacket pkt = { 0 };

        av_packet_copy_props(&pkt, avpkt);

        pkt.data = p;
        pkt.size = size;

        ret = ff_qsv_decode(avctx, &q->qsv, frame, got_frame, &pkt);

        av_free(p);
    } else
        ret = ff_qsv_decode(avctx, &q->qsv, frame, got_frame, avpkt);

    if (ret < 0) {
        char buf[128];
        av_strerror(ret, buf, sizeof(buf));
        av_log(NULL, AV_LOG_ERROR, "Error %s\n", buf);
    }

    return ret;
}

static void qsv_dec_flush(AVCodecContext *avctx)
{
    QSVH264Context *q = avctx->hwaccel_context;

    ff_qsv_flush(&q->qsv);
}

AVHWAccel ff_h264_qsv_hwaccel = {
    .name           = "h264_qsv",
    .priv_data_size = sizeof(QSVH264Context),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_H264,
    .decode         = qsv_dec_frame,
    .flush          = qsv_dec_flush,
};

av_cold int av_qsv_default_init(AVCodecContext *avctx)
{
    QSVH264Context *q;
    mfxBitstream *bs;
    int ret;

    if (avctx->codec_id != AV_CODEC_ID_H264)
        return AVERROR(ENOSYS);

    q = av_mallocz(sizeof(*q))
    if (!q)
        return AVERROR(ENOMEM);

    bs = &q->qsv.bs;

    q->qsv.async_depth = ASYNC_DEPTH_DEFAULT;

    avctx->hwaccel_context = q;

    avctx->pix_fmt = AV_PIX_FMT_NV12;

    if (!(q->bsf = av_bitstream_filter_init("h264_mp4toannexb"))) {
        av_qsv_default_free(avctx);
        return AVERROR(ENOMEM);
    }

    // Data and DataLength passed as dummy pointers
    av_bitstream_filter_filter(q->bsf, avctx, NULL,
                               &bs->Data, &bs->DataLength,
                               NULL, 0, 0);

    //FIXME feed it a fake IDR directly
    bs->Data = av_malloc(avctx->extradata_size + sizeof(fake_idr));
    if (!bs->Data) {
        av_qsv_default_free(avctx);
        return AVERROR(ENOMEM);
    }

    bs->DataLength = avctx->extradata_size;

    memcpy(bs->Data, avctx->extradata, avctx->extradata_size);
    memcpy(bs->Data + bs->DataLength, fake_idr, sizeof(fake_idr));

    bs->DataLength += sizeof(fake_idr);

    bs->MaxLength = bs->DataLength;

    ret = ff_qsv_init(avctx, &q->qsv);
    if (ret < 0)
        av_qsv_default_free(avctx);
    else
        avctx->hwaccel = &ff_h264_qsv_hwaccel;
    return ret;
}
