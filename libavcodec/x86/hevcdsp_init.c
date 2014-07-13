/*
 * Copyright (c) 2013 Seppo Tomperi
 * Copyright (c) 2013 - 2014 Pierre-Edouard Lepere
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

#include "libavutil/cpu.h"
#include "libavutil/x86/cpu.h"

#include "libavcodec/hevcdsp.h"

#define LFC_FUNC(DIR, DEPTH, OPT) \
void ff_hevc_ ## DIR ## _loop_filter_chroma_ ## DEPTH ## _ ## OPT(uint8_t *pix, ptrdiff_t stride, int *tc, uint8_t *no_p, uint8_t *no_q);

#define LFL_FUNC(DIR, DEPTH, OPT) \
void ff_hevc_ ## DIR ## _loop_filter_luma_ ## DEPTH ## _ ## OPT(uint8_t *pix, ptrdiff_t stride, int beta, int *tc, uint8_t *no_p, uint8_t *no_q);

#define LFC_FUNCS(type, depth) \
    LFC_FUNC(h, depth, sse2)   \
    LFC_FUNC(v, depth, sse2)

#define LFL_FUNCS(type, depth) \
    LFL_FUNC(h, depth, ssse3)  \
    LFL_FUNC(v, depth, ssse3)

LFC_FUNCS(uint8_t, 8)
LFC_FUNCS(uint8_t, 10)
LFL_FUNCS(uint8_t, 8)
LFL_FUNCS(uint8_t, 10)

#define IDCT_DC_PROTO(size, bitd, opt) \
void ff_hevc_idct ## size ## _dc_add_ ## bitd ##_## opt(uint8_t *dst, int coeff, ptrdiff_t stride)

IDCT_DC_PROTO(4,  8, mmxext);
IDCT_DC_PROTO(8,  8, mmxext);
IDCT_DC_PROTO(16, 8, sse2);
IDCT_DC_PROTO(32, 8, sse2);

IDCT_DC_PROTO(4,  10, mmxext);
IDCT_DC_PROTO(8,  10, sse2);
IDCT_DC_PROTO(16, 10, sse2);
IDCT_DC_PROTO(32, 10, sse2);
IDCT_DC_PROTO(8,  10, avx);
IDCT_DC_PROTO(16, 10, avx);
IDCT_DC_PROTO(32, 10, avx);

#if HAVE_SSE2_EXTERNAL
static void hevc_idct32_dc_add_8_sse2(uint8_t *dst, int coeff, ptrdiff_t stride)
{
    ff_hevc_idct16_dc_add_8_sse2(dst,                    coeff, stride);
    ff_hevc_idct16_dc_add_8_sse2(dst + 16,               coeff, stride);
    ff_hevc_idct16_dc_add_8_sse2(dst + 16 * stride,      coeff, stride);
    ff_hevc_idct16_dc_add_8_sse2(dst + 16 * stride + 16, coeff, stride);
}

static void hevc_idct16_dc_add_10_sse2(uint8_t *dst, int coeff, ptrdiff_t stride)
{
    ff_hevc_idct8_dc_add_10_sse2(dst,                   coeff, stride);
    ff_hevc_idct8_dc_add_10_sse2(dst + 16,              coeff, stride);
    ff_hevc_idct8_dc_add_10_sse2(dst + 8 * stride,      coeff, stride);
    ff_hevc_idct8_dc_add_10_sse2(dst + 8 * stride + 16, coeff, stride);
}

static void hevc_idct32_dc_add_10_sse2(uint8_t *dst, int coeff, ptrdiff_t stride)
{
    hevc_idct16_dc_add_10_sse2(dst,                    coeff, stride);
    hevc_idct16_dc_add_10_sse2(dst + 32,               coeff, stride);
    hevc_idct16_dc_add_10_sse2(dst + 16 * stride,      coeff, stride);
    hevc_idct16_dc_add_10_sse2(dst + 16 * stride + 32, coeff, stride);
}
#endif //HAVE_SSE2_EXTERNAL

#if HAVE_AVX_EXTERNAL
static void hevc_idct16_dc_add_10_avx(uint8_t *dst, int coeff, ptrdiff_t stride)
{
    ff_hevc_idct8_dc_add_10_avx(dst,                   coeff, stride);
    ff_hevc_idct8_dc_add_10_avx(dst + 16,              coeff, stride);
    ff_hevc_idct8_dc_add_10_avx(dst + 8 * stride,      coeff, stride);
    ff_hevc_idct8_dc_add_10_avx(dst + 8 * stride + 16, coeff, stride);
}

static void hevc_idct32_dc_add_10_avx(uint8_t *dst, int coeff, ptrdiff_t stride)
{
    hevc_idct16_dc_add_10_avx(dst,                    coeff, stride);
    hevc_idct16_dc_add_10_avx(dst + 32,               coeff, stride);
    hevc_idct16_dc_add_10_avx(dst + 16 * stride,      coeff, stride);
    hevc_idct16_dc_add_10_avx(dst + 16 * stride + 32, coeff, stride);
}
#endif //HAVE_AVX_EXTERNAL

void ff_hevc_dsp_init_x86(HEVCDSPContext *c, const int bit_depth)
{
    int cpu_flags = av_get_cpu_flags();

    if (bit_depth == 8) {
        if (EXTERNAL_MMXEXT(cpu_flags)) {
            c->transform_dc_add[0] = ff_hevc_idct4_dc_add_8_mmxext;
            c->transform_dc_add[1] = ff_hevc_idct8_dc_add_8_mmxext;
        }
        if (EXTERNAL_SSE2(cpu_flags)) {
            c->hevc_v_loop_filter_chroma = ff_hevc_v_loop_filter_chroma_8_sse2;
            c->hevc_h_loop_filter_chroma = ff_hevc_h_loop_filter_chroma_8_sse2;

            c->transform_dc_add[2] = ff_hevc_idct16_dc_add_8_sse2;
            c->transform_dc_add[3] = hevc_idct32_dc_add_8_sse2;
        }
        if (EXTERNAL_SSSE3(cpu_flags) && ARCH_X86_64) {
            c->hevc_v_loop_filter_luma = ff_hevc_v_loop_filter_luma_8_ssse3;
            c->hevc_h_loop_filter_luma = ff_hevc_h_loop_filter_luma_8_ssse3;
        }
    } else if (bit_depth == 10) {
        if (EXTERNAL_MMXEXT(cpu_flags)) {
            c->transform_dc_add[0] = ff_hevc_idct4_dc_add_10_mmxext;
        }
        if (EXTERNAL_SSE2(cpu_flags)) {
            c->hevc_v_loop_filter_chroma = ff_hevc_v_loop_filter_chroma_10_sse2;
            c->hevc_h_loop_filter_chroma = ff_hevc_h_loop_filter_chroma_10_sse2;

            c->transform_dc_add[1] = ff_hevc_idct8_dc_add_10_sse2;
            c->transform_dc_add[2] = hevc_idct16_dc_add_10_sse2;
            c->transform_dc_add[3] = hevc_idct32_dc_add_10_sse2;
        }
        if (EXTERNAL_SSSE3(cpu_flags) && ARCH_X86_64) {
            c->hevc_v_loop_filter_luma = ff_hevc_v_loop_filter_luma_10_ssse3;
            c->hevc_h_loop_filter_luma = ff_hevc_h_loop_filter_luma_10_ssse3;
        }
        if (EXTERNAL_AVX(cpu_flags)) {
            c->transform_dc_add[1] = ff_hevc_idct8_dc_add_10_avx;
            c->transform_dc_add[2] = hevc_idct16_dc_add_10_avx;
            c->transform_dc_add[3] = hevc_idct32_dc_add_10_avx;
        }
    }
}
