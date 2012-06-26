/*
 * PNM image format
 * Copyright (c) 2002, 2003 Fabrice Bellard
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

#include "avcodec.h"
#include "bytestream.h"
#include "internal.h"
#include "pnm.h"


static int pnm_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                            const AVFrame *pict, int *got_packet)
{
    PNMContext *s     = avctx->priv_data;
    AVFrame * const p = &s->picture;
    int i, h, h1, c, n, linesize, ret;
    uint8_t *ptr, *ptr1, *ptr2;

    if ((ret = ff_alloc_packet(pkt, avpicture_get_size(avctx->pix_fmt,
                                                       avctx->width,
                                                       avctx->height) + 200)) < 0) {
        av_log(avctx, AV_LOG_ERROR, "encoded frame too large\n");
        return ret;
    }

    *p           = *pict;
    p->pict_type = AV_PICTURE_TYPE_I;
    p->key_frame = 1;

    s->bytestream_start =
    s->bytestream       = pkt->data;
    s->bytestream_end   = pkt->data + pkt->size;

    h  = avctx->height;
    h1 = h;
    switch (avctx->pix_fmt) {
    case PIX_FMT_MONOWHITE:
        c  = '4';
        n  = (avctx->width + 7) >> 3;
        break;
    case PIX_FMT_GRAY8:
        c  = '5';
        n  = avctx->width;
        break;
    case PIX_FMT_GRAY16BE:
        c  = '5';
        n  = avctx->width * 2;
        break;
    case PIX_FMT_RGB24:
        c  = '6';
        n  = avctx->width * 3;
        break;
    case PIX_FMT_RGB48BE:
        c  = '6';
        n  = avctx->width * 6;
        break;
    case PIX_FMT_YUV420P:
        c  = '5';
        n  = avctx->width;
        h1 = (h * 3) / 2;
        break;
    default:
        return -1;
    }
    snprintf(s->bytestream, s->bytestream_end - s->bytestream,
             "P%c\n%d %d\n", c, avctx->width, h1);
    s->bytestream += strlen(s->bytestream);
    if (avctx->pix_fmt != PIX_FMT_MONOWHITE) {
        snprintf(s->bytestream, s->bytestream_end - s->bytestream,
                 "%d\n", (avctx->pix_fmt != PIX_FMT_GRAY16BE && avctx->pix_fmt != PIX_FMT_RGB48BE) ? 255 : 65535);
        s->bytestream += strlen(s->bytestream);
    }

    ptr      = p->data[0];
    linesize = p->linesize[0];
    for (i = 0; i < h; i++) {
        memcpy(s->bytestream, ptr, n);
        s->bytestream += n;
        ptr           += linesize;
    }

    if (avctx->pix_fmt == PIX_FMT_YUV420P) {
        h >>= 1;
        n >>= 1;
        ptr1 = p->data[1];
        ptr2 = p->data[2];
        for (i = 0; i < h; i++) {
            memcpy(s->bytestream, ptr1, n);
            s->bytestream += n;
            memcpy(s->bytestream, ptr2, n);
            s->bytestream += n;
                ptr1 += p->linesize[1];
                ptr2 += p->linesize[2];
        }
    }
    pkt->size   = s->bytestream - s->bytestream_start;
    pkt->flags |= AV_PKT_FLAG_KEY;
    *got_packet = 1;

    return 0;
}


#if CONFIG_PGM_ENCODER
static const enum PixelFormat tmp__0[] = {
        PIX_FMT_GRAY8, PIX_FMT_GRAY16BE, PIX_FMT_NONE
    };
AVCodec ff_pgm_encoder = {
    "pgm",
    NULL_IF_CONFIG_SMALL("PGM (Portable GrayMap) image"),
    AVMEDIA_TYPE_VIDEO,
    CODEC_ID_PGM,
    0, 0, tmp__0,
    0, 0, 0, 0, 0, 0, sizeof(PNMContext),
    0, 0, 0, 0, 0, ff_pnm_init,
    0, pnm_encode_frame,
};
#endif

#if CONFIG_PGMYUV_ENCODER
static const enum PixelFormat tmp__1[] = { PIX_FMT_YUV420P, PIX_FMT_NONE };
AVCodec ff_pgmyuv_encoder = {
    "pgmyuv",
    NULL_IF_CONFIG_SMALL("PGMYUV (Portable GrayMap YUV) image"),
    AVMEDIA_TYPE_VIDEO,
    CODEC_ID_PGMYUV,
    0, 0, tmp__1,
    0, 0, 0, 0, 0, 0, sizeof(PNMContext),
    0, 0, 0, 0, 0, ff_pnm_init,
    0, pnm_encode_frame,
};
#endif

#if CONFIG_PPM_ENCODER
static const enum PixelFormat tmp__2[] = {
        PIX_FMT_RGB24, PIX_FMT_RGB48BE, PIX_FMT_NONE
    };
AVCodec ff_ppm_encoder = {
    "ppm",
    NULL_IF_CONFIG_SMALL("PPM (Portable PixelMap) image"),
    AVMEDIA_TYPE_VIDEO,
    CODEC_ID_PPM,
    0, 0, tmp__2,
    0, 0, 0, 0, 0, 0, sizeof(PNMContext),
    0, 0, 0, 0, 0, ff_pnm_init,
    0, pnm_encode_frame,
};
#endif

#if CONFIG_PBM_ENCODER
static const enum PixelFormat tmp__3[] = { PIX_FMT_MONOWHITE,
                                                  PIX_FMT_NONE };
AVCodec ff_pbm_encoder = {
    "pbm",
    NULL_IF_CONFIG_SMALL("PBM (Portable BitMap) image"),
    AVMEDIA_TYPE_VIDEO,
    CODEC_ID_PBM,
    0, 0, tmp__3,
    0, 0, 0, 0, 0, 0, sizeof(PNMContext),
    0, 0, 0, 0, 0, ff_pnm_init,
    0, pnm_encode_frame,
};
#endif
