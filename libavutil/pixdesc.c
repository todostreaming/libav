/*
 * pixel format descriptor
 * Copyright (c) 2009 Michael Niedermayer <michaelni@gmx.at>
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

#include <stdio.h>
#include <string.h>
#include "pixfmt.h"
#include "pixdesc.h"

#include "intreadwrite.h"

void av_read_image_line(uint16_t *dst, const uint8_t *data[4], const int linesize[4],
                        const AVPixFmtDescriptor *desc, int x, int y, int c, int w,
                        int read_pal_component)
{
    AVComponentDescriptor comp = desc->comp[c];
    int plane = comp.plane;
    int depth = comp.depth_minus1 + 1;
    int mask  = (1 << depth) - 1;
    int shift = comp.shift;
    int step  = comp.step_minus1 + 1;
    int flags = desc->flags;

    if (flags & PIX_FMT_BITSTREAM) {
        int skip = x * step + comp.offset_plus1 - 1;
        const uint8_t *p = data[plane] + y * linesize[plane] + (skip >> 3);
        int shift = 8 - depth - (skip & 7);

        while (w--) {
            int val = (*p >> shift) & mask;
            if (read_pal_component)
                val = data[1][4*val + c];
            shift -= step;
            p -= shift >> 3;
            shift &= 7;
            *dst++ = val;
        }
    } else {
        const uint8_t *p = data[plane] + y * linesize[plane] + x * step + comp.offset_plus1 - 1;
        int is_8bit = shift + depth <= 8;

        if (is_8bit)
            p += !!(flags & PIX_FMT_BE);

        while (w--) {
            int val = is_8bit ? *p :
                flags & PIX_FMT_BE ? AV_RB16(p) : AV_RL16(p);
            val = (val >> shift) & mask;
            if (read_pal_component)
                val = data[1][4 * val + c];
            p += step;
            *dst++ = val;
        }
    }
}

void av_write_image_line(const uint16_t *src, uint8_t *data[4], const int linesize[4],
                         const AVPixFmtDescriptor *desc, int x, int y, int c, int w)
{
    AVComponentDescriptor comp = desc->comp[c];
    int plane = comp.plane;
    int depth = comp.depth_minus1 + 1;
    int step  = comp.step_minus1 + 1;
    int flags = desc->flags;

    if (flags & PIX_FMT_BITSTREAM) {
        int skip = x * step + comp.offset_plus1 - 1;
        uint8_t *p = data[plane] + y * linesize[plane] + (skip >> 3);
        int shift = 8 - depth - (skip & 7);

        while (w--) {
            *p |= *src++ << shift;
            shift -= step;
            p -= shift >> 3;
            shift &= 7;
        }
    } else {
        int shift = comp.shift;
        uint8_t *p = data[plane] + y * linesize[plane] + x * step + comp.offset_plus1 - 1;

        if (shift + depth <= 8) {
            p += !!(flags & PIX_FMT_BE);
            while (w--) {
                *p |= (*src++ << shift);
                p += step;
            }
        } else {
            while (w--) {
                if (flags & PIX_FMT_BE) {
                    uint16_t val = AV_RB16(p) | (*src++ << shift);
                    AV_WB16(p, val);
                } else {
                    uint16_t val = AV_RL16(p) | (*src++ << shift);
                    AV_WL16(p, val);
                }
                p += step;
            }
        }
    }
}

const AVPixFmtDescriptor av_pix_fmt_descriptors[PIX_FMT_NB] = {
    /* PIX_FMT_YUV420P */ {
        "yuv420p",
        3,
        1,
        1,
        PIX_FMT_PLANAR,
        {
            { 0, 0, 1, 0, 7 },        /* Y */
            { 1, 0, 1, 0, 7 },        /* U */
            { 2, 0, 1, 0, 7 },        /* V */
        },
    },
    /* PIX_FMT_YUYV422 */ {
        "yuyv422",
        3,
        1,
        0,
        0, {
            { 0, 1, 1, 0, 7 },        /* Y */
            { 0, 3, 2, 0, 7 },        /* U */
            { 0, 3, 4, 0, 7 },        /* V */
        },
    },
    /* PIX_FMT_RGB24 */ {
        "rgb24",
        3,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 2, 1, 0, 7 },        /* R */
            { 0, 2, 2, 0, 7 },        /* G */
            { 0, 2, 3, 0, 7 },        /* B */
        },
    },
    /* PIX_FMT_BGR24 */ {
        "bgr24",
        3,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 2, 1, 0, 7 },        /* B */
            { 0, 2, 2, 0, 7 },        /* G */
            { 0, 2, 3, 0, 7 },        /* R */
        },
    },
    /* PIX_FMT_YUV422P */ {
        "yuv422p",
        3,
        1,
        0,
        PIX_FMT_PLANAR,
        {
            { 0, 0, 1, 0, 7 },        /* Y */
            { 1, 0, 1, 0, 7 },        /* U */
            { 2, 0, 1, 0, 7 },        /* V */
        },
    },
    /* PIX_FMT_YUV444P */ {
        "yuv444p",
        3,
        0,
        0,
        PIX_FMT_PLANAR,
        {
            { 0, 0, 1, 0, 7 },        /* Y */
            { 1, 0, 1, 0, 7 },        /* U */
            { 2, 0, 1, 0, 7 },        /* V */
        },
    },
    /* PIX_FMT_YUV410P */ {
        "yuv410p",
        3,
        2,
        2,
        PIX_FMT_PLANAR,
        {
            { 0, 0, 1, 0, 7 },        /* Y */
            { 1, 0, 1, 0, 7 },        /* U */
            { 2, 0, 1, 0, 7 },        /* V */
        },
    },
    /* PIX_FMT_YUV411P */ {
        "yuv411p",
        3,
        2,
        0,
        PIX_FMT_PLANAR,
        {
            { 0, 0, 1, 0, 7 },        /* Y */
            { 1, 0, 1, 0, 7 },        /* U */
            { 2, 0, 1, 0, 7 },        /* V */
        },
    },
    /* PIX_FMT_GRAY8 */ {
        "gray",
        1,
        0,
        0,
        0, {
            { 0, 0, 1, 0, 7 },        /* Y */
        },
    },
    /* PIX_FMT_MONOWHITE */ {
        "monow",
        1,
        0,
        0,
        PIX_FMT_BITSTREAM,
        {
            { 0, 0, 1, 0, 0 },        /* Y */
        },
    },
    /* PIX_FMT_MONOBLACK */ {
        "monob",
        1,
        0,
        0,
        PIX_FMT_BITSTREAM,
        {
            { 0, 0, 1, 7, 0 },        /* Y */
        },
    },
    /* PIX_FMT_PAL8 */ {
        "pal8",
        1,
        0,
        0,
        PIX_FMT_PAL,
        {
            { 0, 0, 1, 0, 7 },
        },
    },
    /* PIX_FMT_YUVJ420P */ {
        "yuvj420p",
        3,
        1,
        1,
        PIX_FMT_PLANAR,
        {
            { 0, 0, 1, 0, 7 },        /* Y */
            { 1, 0, 1, 0, 7 },        /* U */
            { 2, 0, 1, 0, 7 },        /* V */
        },
    },
    /* PIX_FMT_YUVJ422P */ {
        "yuvj422p",
        3,
        1,
        0,
        PIX_FMT_PLANAR,
        {
            { 0, 0, 1, 0, 7 },        /* Y */
            { 1, 0, 1, 0, 7 },        /* U */
            { 2, 0, 1, 0, 7 },        /* V */
        },
    },
    /* PIX_FMT_YUVJ444P */ {
        "yuvj444p",
        3,
        0,
        0,
        PIX_FMT_PLANAR,
        {
            {0, 0, 1, 0, 7},        /* Y */
            {1, 0, 1, 0, 7},        /* U */
            {2, 0, 1, 0, 7},        /* V */
        },
    },
    /* PIX_FMT_XVMC_MPEG2_MC */ {
        "xvmcmc",
        0, 0, 0, PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_XVMC_MPEG2_IDCT */ {
        "xvmcidct",
        0, 0, 0, PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_UYVY422 */ {
        "uyvy422",
        3,
        1,
        0,
        0, {
            { 0, 1, 2, 0, 7 },        /* Y */
            { 0, 3, 1, 0, 7 },        /* U */
            { 0, 3, 3, 0, 7 },        /* V */
        },
    },
    /* PIX_FMT_UYYVYY411 */ {
        "uyyvyy411",
        3,
        2,
        0,
        0, {
            { 0, 3, 2, 0, 7 },        /* Y */
            { 0, 5, 1, 0, 7 },        /* U */
            { 0, 5, 4, 0, 7 },        /* V */
        },
    },
    /* PIX_FMT_BGR8 */ {
        "bgr8",
        3,
        0,
        0,
        PIX_FMT_RGB | PIX_FMT_PSEUDOPAL,
        {
            { 0, 0, 1, 6, 1 },        /* B */
            { 0, 0, 1, 3, 2 },        /* G */
            { 0, 0, 1, 0, 2 },        /* R */
        },
    },
    /* PIX_FMT_BGR4 */ {
        "bgr4",
        3,
        0,
        0,
        PIX_FMT_BITSTREAM | PIX_FMT_RGB,
        {
            { 0, 3, 1, 0, 0 },        /* B */
            { 0, 3, 2, 0, 1 },        /* G */
            { 0, 3, 4, 0, 0 },        /* R */
        },
    },
    /* PIX_FMT_BGR4_BYTE */ {
        "bgr4_byte",
        3,
        0,
        0,
        PIX_FMT_RGB | PIX_FMT_PSEUDOPAL,
        {
            { 0, 0, 1, 3, 0 },        /* B */
            { 0, 0, 1, 1, 1 },        /* G */
            { 0, 0, 1, 0, 0 },        /* R */
        },
    },
    /* PIX_FMT_RGB8 */ {
        "rgb8",
        3,
        0,
        0,
        PIX_FMT_RGB | PIX_FMT_PSEUDOPAL,
        {
            { 0, 0, 1, 6, 1 },        /* R */
            { 0, 0, 1, 3, 2 },        /* G */
            { 0, 0, 1, 0, 2 },        /* B */
        },
    },
    /* PIX_FMT_RGB4 */ {
        "rgb4",
        3,
        0,
        0,
        PIX_FMT_BITSTREAM | PIX_FMT_RGB,
        {
            { 0, 3, 1, 0, 0 },        /* R */
            { 0, 3, 2, 0, 1 },        /* G */
            { 0, 3, 4, 0, 0 },        /* B */
        },
    },
    /* PIX_FMT_RGB4_BYTE */ {
        "rgb4_byte",
        3,
        0,
        0,
        PIX_FMT_RGB | PIX_FMT_PSEUDOPAL,
        {
            { 0, 0, 1, 3, 0 },        /* R */
            { 0, 0, 1, 1, 1 },        /* G */
            { 0, 0, 1, 0, 0 },        /* B */
        },
    },
    /* PIX_FMT_NV12 */ {
        "nv12",
        3,
        1,
        1,
        PIX_FMT_PLANAR,
        {
            { 0,0,1,0,7 },        /* Y */
            { 1,1,1,0,7 },        /* U */
            { 1,1,2,0,7 },        /* V */
        },
    },
    /* PIX_FMT_NV21 */ {
        "nv21",
        3,
        1,
        1,
        PIX_FMT_PLANAR,
        {
            { 0, 0, 1, 0, 7 },        /* Y */
            { 1, 1, 1, 0, 7 },        /* V */
            { 1, 1, 2, 0, 7 },        /* U */
        },
    },
    /* PIX_FMT_ARGB */ {
        "argb",
        4,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 3, 1, 0, 7 },        /* A */
            { 0, 3, 2, 0, 7 },        /* R */
            { 0, 3, 3, 0, 7 },        /* G */
            { 0, 3, 4, 0, 7 },        /* B */
        },
    },
    /* PIX_FMT_RGBA */ {
        "rgba",
        4,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 3, 1, 0, 7 },        /* R */
            { 0, 3, 2, 0, 7 },        /* G */
            { 0, 3, 3, 0, 7 },        /* B */
            { 0, 3, 4, 0, 7 },        /* A */
        },
    },
    /* PIX_FMT_ABGR */ {
        "abgr",
        4,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 3, 1, 0, 7 },        /* A */
            { 0, 3, 2, 0, 7 },        /* B */
            { 0, 3, 3, 0, 7 },        /* G */
            { 0, 3, 4, 0, 7 },        /* R */
        },
    },
    /* PIX_FMT_BGRA */ {
        "bgra",
        4,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 3, 1, 0, 7 },        /* B */
            { 0, 3, 2, 0, 7 },        /* G */
            { 0, 3, 3, 0, 7 },        /* R */
            { 0, 3, 4, 0, 7 },        /* A */
        },
    },
    /* PIX_FMT_GRAY16BE */ {
        "gray16be",
        1,
        0,
        0,
        PIX_FMT_BE,
        {
            { 0, 1, 1, 0, 15 },       /* Y */
        },
    },
    /* PIX_FMT_GRAY16LE */ {
        "gray16le",
        1,
        0,
        0,
        0, {
            { 0, 1, 1, 0, 15 },       /* Y */
        },
    },
    /* PIX_FMT_YUV440P */ {
        "yuv440p",
        3,
        0,
        1,
        PIX_FMT_PLANAR,
        {
            { 0, 0, 1, 0, 7 },        /* Y */
            { 1, 0, 1, 0, 7 },        /* U */
            { 2, 0, 1, 0, 7 },        /* V */
        },
    },
    /* PIX_FMT_YUVJ440P */ {
        "yuvj440p",
        3,
        0,
        1,
        PIX_FMT_PLANAR,
        {
            { 0, 0, 1, 0, 7 },        /* Y */
            { 1, 0, 1, 0, 7 },        /* U */
            { 2, 0, 1, 0, 7 },        /* V */
        },
    },
    /* PIX_FMT_YUVA420P */ {
        "yuva420p",
        4,
        1,
        1,
        PIX_FMT_PLANAR,
        {
            { 0, 0, 1, 0, 7 },        /* Y */
            { 1, 0, 1, 0, 7 },        /* U */
            { 2, 0, 1, 0, 7 },        /* V */
            { 3, 0, 1, 0, 7 },        /* A */
        },
    },
    /* PIX_FMT_VDPAU_H264 */ {
        "vdpau_h264",
        0, 1,
        1,
        PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_VDPAU_MPEG1 */ {
        "vdpau_mpeg1",
        0, 1,
        1,
        PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_VDPAU_MPEG2 */ {
        "vdpau_mpeg2",
        0, 1,
        1,
        PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_VDPAU_WMV3 */ {
        "vdpau_wmv3",
        0, 1,
        1,
        PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_VDPAU_VC1 */ {
        "vdpau_vc1",
        0, 1,
        1,
        PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_RGB48BE */ {
        "rgb48be",
        3,
        0,
        0,
        PIX_FMT_RGB | PIX_FMT_BE,
        {
            { 0, 5, 1, 0, 15 },       /* R */
            { 0, 5, 3, 0, 15 },       /* G */
            { 0, 5, 5, 0, 15 },       /* B */
        },
    },
    /* PIX_FMT_RGB48LE */ {
        "rgb48le",
        3,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 5, 1, 0, 15 },       /* R */
            { 0, 5, 3, 0, 15 },       /* G */
            { 0, 5, 5, 0, 15 },       /* B */
        },
    },
    /* PIX_FMT_RGB565BE */ {
        "rgb565be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_RGB,
        {
            { 0, 1, 0, 3, 4 },        /* R */
            { 0, 1, 1, 5, 5 },        /* G */
            { 0, 1, 1, 0, 4 },        /* B */
        },
    },
    /* PIX_FMT_RGB565LE */ {
        "rgb565le",
        3,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 1, 2, 3, 4 },        /* R */
            { 0, 1, 1, 5, 5 },        /* G */
            { 0, 1, 1, 0, 4 },        /* B */
        },
    },
    /* PIX_FMT_RGB555BE */ {
        "rgb555be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_RGB,
        {
            { 0, 1, 0, 2, 4 },        /* R */
            { 0, 1, 1, 5, 4 },        /* G */
            { 0, 1, 1, 0, 4 },        /* B */
        },
    },
    /* PIX_FMT_RGB555LE */ {
        "rgb555le",
        3,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 1, 2, 2, 4 },        /* R */
            { 0, 1, 1, 5, 4 },        /* G */
            { 0, 1, 1, 0, 4 },        /* B */
        },
    },
    /* PIX_FMT_BGR565BE */ {
        "bgr565be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_RGB,
        {
            { 0, 1, 0, 3, 4 },        /* B */
            { 0, 1, 1, 5, 5 },        /* G */
            { 0, 1, 1, 0, 4 },        /* R */
        },
    },
    /* PIX_FMT_BGR565LE */ {
        "bgr565le",
        3,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 1, 2, 3, 4 },        /* B */
            { 0, 1, 1, 5, 5 },        /* G */
            { 0, 1, 1, 0, 4 },        /* R */
        },
    },
    /* PIX_FMT_BGR555BE */ {
        "bgr555be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_RGB,
        {
            { 0, 1, 0, 2, 4 },       /* B */
            { 0, 1, 1, 5, 4 },       /* G */
            { 0, 1, 1, 0, 4 },       /* R */
        },
     },
    /* PIX_FMT_BGR555LE */ {
        "bgr555le",
        3,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 1, 2, 2, 4 },        /* B */
            { 0, 1, 1, 5, 4 },        /* G */
            { 0, 1, 1, 0, 4 },        /* R */
        },
    },
    /* PIX_FMT_VAAPI_MOCO */ {
        "vaapi_moco",
        0, 1,
        1,
        PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_VAAPI_IDCT */ {
        "vaapi_idct",
        0, 1,
        1,
        PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_VAAPI_VLD */ {
        "vaapi_vld",
        0, 1,
        1,
        PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_YUV420P16LE */ {
        "yuv420p16le",
        3,
        1,
        1,
        PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 15 },        /* Y */
            { 1, 1, 1, 0, 15 },        /* U */
            { 2, 1, 1, 0, 15 },        /* V */
        },
    },
    /* PIX_FMT_YUV420P16BE */ {
        "yuv420p16be",
        3,
        1,
        1,
        PIX_FMT_BE | PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 15 },        /* Y */
            { 1, 1, 1, 0, 15 },        /* U */
            { 2, 1, 1, 0, 15 },        /* V */
        },
    },
    /* PIX_FMT_YUV422P16LE */ {
        "yuv422p16le",
        3,
        1,
        0,
        PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 15 },        /* Y */
            { 1, 1, 1, 0, 15 },        /* U */
            { 2, 1, 1, 0, 15 },        /* V */
        },
    },
    /* PIX_FMT_YUV422P16BE */ {
        "yuv422p16be",
        3,
        1,
        0,
        PIX_FMT_BE | PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 15 },        /* Y */
            { 1, 1, 1, 0, 15 },        /* U */
            { 2, 1, 1, 0, 15 },        /* V */
        },
    },
    /* PIX_FMT_YUV444P16LE */ {
        "yuv444p16le",
        3,
        0,
        0,
        PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 15 },        /* Y */
            { 1, 1, 1, 0, 15 },        /* U */
            { 2, 1, 1, 0, 15 },        /* V */
        },
    },
    /* PIX_FMT_YUV444P16BE */ {
        "yuv444p16be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 15 },        /* Y */
            { 1, 1, 1, 0, 15 },        /* U */
            { 2, 1, 1, 0, 15 },        /* V */
        },
    },
    /* PIX_FMT_VDPAU_MPEG4 */ {
        "vdpau_mpeg4",
        0, 1,
        1,
        PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_DXVA2_VLD */ {
        "dxva2_vld",
        0, 1,
        1,
        PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_RGB444LE */ {
        "rgb444le",
        3,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 1, 2, 0, 3 },        /* R */
            { 0, 1, 1, 4, 3 },        /* G */
            { 0, 1, 1, 0, 3 },        /* B */
        },
    },
    /* PIX_FMT_RGB444BE */ {
        "rgb444be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_RGB,
        {
            { 0, 1, 0, 0, 3 },        /* R */
            { 0, 1, 1, 4, 3 },        /* G */
            { 0, 1, 1, 0, 3 },        /* B */
        },
    },
    /* PIX_FMT_BGR444LE */ {
        "bgr444le",
        3,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 1, 2, 0, 3 },        /* B */
            { 0, 1, 1, 4, 3 },        /* G */
            { 0, 1, 1, 0, 3 },        /* R */
        },
    },
    /* PIX_FMT_BGR444BE */ {
        "bgr444be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_RGB,
        {
            { 0, 1, 0, 0, 3 },       /* B */
            { 0, 1, 1, 4, 3 },       /* G */
            { 0, 1, 1, 0, 3 },       /* R */
        },
     },
    /* PIX_FMT_Y400A */ {
        "y400a",
        2,
        0, 0, 0, {
            { 0, 1, 1, 0, 7 },        /* Y */
            { 0, 1, 2, 0, 7 },        /* A */
        },
    },
    /* PIX_FMT_BGR48BE */ {
        "bgr48be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_RGB,
        {
            { 0, 5, 1, 0, 15 },       /* B */
            { 0, 5, 3, 0, 15 },       /* G */
            { 0, 5, 5, 0, 15 },       /* R */
        },
    },
    /* PIX_FMT_BGR48LE */ {
        "bgr48le",
        3,
        0,
        0,
        PIX_FMT_RGB,
        {
            { 0, 5, 1, 0, 15 },       /* B */
            { 0, 5, 3, 0, 15 },       /* G */
            { 0, 5, 5, 0, 15 },       /* R */
        },
    },
    /* PIX_FMT_YUV420P9BE */ {
        "yuv420p9be",
        3,
        1,
        1,
        PIX_FMT_BE | PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 8 },        /* Y */
            { 1, 1, 1, 0, 8 },        /* U */
            { 2, 1, 1, 0, 8 },        /* V */
        },
    },
    /* PIX_FMT_YUV420P9LE */ {
        "yuv420p9le",
        3,
        1,
        1,
        PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 8 },        /* Y */
            { 1, 1, 1, 0, 8 },        /* U */
            { 2, 1, 1, 0, 8 },        /* V */
        },
    },
    /* PIX_FMT_YUV420P10BE */ {
        "yuv420p10be",
        3,
        1,
        1,
        PIX_FMT_BE | PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 9 },        /* Y */
            { 1, 1, 1, 0, 9 },        /* U */
            { 2, 1, 1, 0, 9 },        /* V */
        },
    },
    /* PIX_FMT_YUV420P10LE */ {
        "yuv420p10le",
        3,
        1,
        1,
        PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 9 },        /* Y */
            { 1, 1, 1, 0, 9 },        /* U */
            { 2, 1, 1, 0, 9 },        /* V */
        },
    },
    /* PIX_FMT_YUV422P10BE */ {
        "yuv422p10be",
        3,
        1,
        0,
        PIX_FMT_BE | PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 9 },        /* Y */
            { 1, 1, 1, 0, 9 },        /* U */
            { 2, 1, 1, 0, 9 },        /* V */
        },
    },
    /* PIX_FMT_YUV422P10LE */ {
        "yuv422p10le",
        3,
        1,
        0,
        PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 9 },        /* Y */
            { 1, 1, 1, 0, 9 },        /* U */
            { 2, 1, 1, 0, 9 },        /* V */
        },
    },
    /* PIX_FMT_YUV444P9BE */ {
        "yuv444p9be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 8 },        /* Y */
            { 1, 1, 1, 0, 8 },        /* U */
            { 2, 1, 1, 0, 8 },        /* V */
        },
    },
    /* PIX_FMT_YUV444P9LE */ {
        "yuv444p9le",
        3,
        0,
        0,
        PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 8 },        /* Y */
            { 1, 1, 1, 0, 8 },        /* U */
            { 2, 1, 1, 0, 8 },        /* V */
        },
    },
    /* PIX_FMT_YUV444P10BE */ {
        "yuv444p10be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 9 },        /* Y */
            { 1, 1, 1, 0, 9 },        /* U */
            { 2, 1, 1, 0, 9 },        /* V */
        },
    },
    /* PIX_FMT_YUV444P10LE */ {
        "yuv444p10le",
        3,
        0,
        0,
        PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 9 },        /* Y */
            { 1, 1, 1, 0, 9 },        /* U */
            { 2, 1, 1, 0, 9 },        /* V */
        },
    },
    /* PIX_FMT_YUV422P9BE */ {
        "yuv422p9be",
        3,
        1,
        0,
        PIX_FMT_BE | PIX_FMT_PLANAR,
        {
            {0,1,1,0,8},        /* Y */
            {1,1,1,0,8},        /* U */
            {2,1,1,0,8},        /* V */
        },
    },
    /* PIX_FMT_YUV422P9LE */ {
        "yuv422p9le",
        3,
        1,
        0,
        PIX_FMT_PLANAR,
        {
            { 0, 1, 1, 0, 8 },        /* Y */
            { 1, 1, 1, 0, 8 },        /* U */
            { 2, 1, 1, 0, 8 },        /* V */
        },
    },
    /* PIX_FMT_VDA_VLD */ {
        "vda_vld",
        0, 1,
        1,
        PIX_FMT_HWACCEL,
    },
    /* PIX_FMT_GBRP */ {
        "gbrp",
        3,
        0,
        0,
        PIX_FMT_PLANAR | PIX_FMT_RGB,
        {
            { 0, 0, 1, 0, 7 },        /* G */
            { 1, 0, 1, 0, 7 },        /* B */
            { 2, 0, 1, 0, 7 },        /* R */
        },
    },
    /* PIX_FMT_GBRP9BE */ {
        "gbrp9be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_PLANAR | PIX_FMT_RGB,
        {
            { 0, 1, 1, 0, 8 },        /* G */
            { 1, 1, 1, 0, 8 },        /* B */
            { 2, 1, 1, 0, 8 },        /* R */
        },
    },
    /* PIX_FMT_GBRP9LE */ {
        "gbrp9le",
        3,
        0,
        0,
        PIX_FMT_PLANAR | PIX_FMT_RGB,
        {
            { 0, 1, 1, 0, 8 },        /* G */
            { 1, 1, 1, 0, 8 },        /* B */
            { 2, 1, 1, 0, 8 },        /* R */
        },
    },
    /* PIX_FMT_GBRP10BE */ {
        "gbrp10be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_PLANAR | PIX_FMT_RGB,
        {
            { 0, 1, 1, 0, 9 },        /* G */
            { 1, 1, 1, 0, 9 },        /* B */
            { 2, 1, 1, 0, 9 },        /* R */
        },
    },
    /* PIX_FMT_GBRP10LE */ {
        "gbrp10le",
        3,
        0,
        0,
        PIX_FMT_PLANAR | PIX_FMT_RGB,
        {
            { 0, 1, 1, 0, 9 },        /* G */
            { 1, 1, 1, 0, 9 },        /* B */
            { 2, 1, 1, 0, 9 },        /* R */
        },
    },
    /* PIX_FMT_GBRP16BE */ {
        "gbrp16be",
        3,
        0,
        0,
        PIX_FMT_BE | PIX_FMT_PLANAR | PIX_FMT_RGB,
        {
            { 0, 1, 1, 0, 15 },       /* G */
            { 1, 1, 1, 0, 15 },       /* B */
            { 2, 1, 1, 0, 15 },       /* R */
        },
    },
    /* PIX_FMT_GBRP16LE */ {
        "gbrp16le",
        3,
        0,
        0,
        PIX_FMT_PLANAR | PIX_FMT_RGB,
        {
            { 0, 1, 1, 0, 15 },       /* G */
            { 1, 1, 1, 0, 15 },       /* B */
            { 2, 1, 1, 0, 15 },       /* R */
        },
    },
};

static enum PixelFormat get_pix_fmt_internal(const char *name)
{
    enum PixelFormat pix_fmt;

    for (pix_fmt = 0; pix_fmt < PIX_FMT_NB; pix_fmt++)
        if (av_pix_fmt_descriptors[pix_fmt].name &&
            !strcmp(av_pix_fmt_descriptors[pix_fmt].name, name))
            return pix_fmt;

    return PIX_FMT_NONE;
}

const char *av_get_pix_fmt_name(enum PixelFormat pix_fmt)
{
    return (unsigned)pix_fmt < PIX_FMT_NB ?
        av_pix_fmt_descriptors[pix_fmt].name : NULL;
}

#if HAVE_BIGENDIAN
#   define X_NE(be, le) be
#else
#   define X_NE(be, le) le
#endif

enum PixelFormat av_get_pix_fmt(const char *name)
{
    enum PixelFormat pix_fmt;

    if (!strcmp(name, "rgb32"))
        name = X_NE("argb", "bgra");
    else if (!strcmp(name, "bgr32"))
        name = X_NE("abgr", "rgba");

    pix_fmt = get_pix_fmt_internal(name);
    if (pix_fmt == PIX_FMT_NONE) {
        char name2[32];

        snprintf(name2, sizeof(name2), "%s%s", name, X_NE("be", "le"));
        pix_fmt = get_pix_fmt_internal(name2);
    }
    return pix_fmt;
}

int av_get_bits_per_pixel(const AVPixFmtDescriptor *pixdesc)
{
    int c, bits = 0;
    int log2_pixels = pixdesc->log2_chroma_w + pixdesc->log2_chroma_h;

    for (c = 0; c < pixdesc->nb_components; c++) {
        int s = c == 1 || c == 2 ? 0 : log2_pixels;
        bits += (pixdesc->comp[c].depth_minus1 + 1) << s;
    }

    return bits >> log2_pixels;
}

char *av_get_pix_fmt_string (char *buf, int buf_size, enum PixelFormat pix_fmt)
{
    /* print header */
    if (pix_fmt < 0) {
       snprintf (buf, buf_size, "name" " nb_components" " nb_bits");
    } else {
        const AVPixFmtDescriptor *pixdesc = &av_pix_fmt_descriptors[pix_fmt];
        snprintf(buf, buf_size, "%-11s %7d %10d", pixdesc->name,
                 pixdesc->nb_components, av_get_bits_per_pixel(pixdesc));
    }

    return buf;
}
