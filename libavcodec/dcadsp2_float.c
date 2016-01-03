/*
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

#include "dca2.h"
#include "dca2_math.h"

static void lfe_fir_c(float *pcm_samples, int *lfe_samples, int npcmblocks,
                      const float *filter_coeff, int dec_select)
{
    // Select decimation factor
    int factor = 64 << dec_select;
    int ncoeffs = 8 >> dec_select;
    int nlfesamples = npcmblocks >> (dec_select + 1);
    int i, j, k;

    for (i = 0; i < nlfesamples; i++) {
        // One decimated sample generates 64 or 128 interpolated ones
        for (j = 0; j < factor / 2; j++) {
            float a = 0.0f;
            float b = 0.0f;

            for (k = 0; k < ncoeffs; k++) {
                a += filter_coeff[      j * ncoeffs + k] * lfe_samples[-k];
                b += filter_coeff[255 - j * ncoeffs - k] * lfe_samples[-k];
            }

            pcm_samples[             j] = a;
            pcm_samples[factor / 2 + j] = b;
        }

        lfe_samples++;
        pcm_samples += factor;
    }
}

static void lfe_fir1_c(float *pcm_samples, int *lfe_samples, int npcmblocks)
{
    lfe_fir_c(pcm_samples, lfe_samples, npcmblocks, ff_dca_lfe_fir_64, 0);
}

static void lfe_fir2_c(float *pcm_samples, int *lfe_samples, int npcmblocks)
{
    lfe_fir_c(pcm_samples, lfe_samples, npcmblocks, ff_dca_lfe_fir_128, 1);
}

static void sub_qmf32_c(FFTContext *imdct,
                        SynthFilterContext *synth,
                        float *pcm_samples,
                        int **subband_samples_lo,
                        int **subband_samples_hi,
                        DCA2DspData *dsp,
                        int nsamples, int perfect)
{
    LOCAL_ALIGNED(32, float, input, [32]);
    int i, sample;
    const float *filter_coeff = perfect ? ff_dca_fir_32bands_perfect :
                                          ff_dca_fir_32bands_nonperfect;

    for (sample = 0; sample < nsamples; sample++) {
        // Load in one sample from each subband
        for (i = 0; i < 32; i++) {
            if ((i - 1) & 2)
                input[i] = -subband_samples_lo[i][sample];
            else
                input[i] =  subband_samples_lo[i][sample];
        }

        // One subband sample generates 32 interpolated ones
        synth->synth_filter_float(imdct, dsp->u.flt32.hist1, &dsp->offset,
                                  dsp->u.flt32.hist2, filter_coeff,
                                  pcm_samples, input, 1.0f / (1 << 17));

        // Advance output pointer
        pcm_samples += 32;
    }
}

static void sub_qmf64_c(FFTContext *imdct,
                        SynthFilterContext *synth,
                        float *pcm_samples,
                        int **subband_samples_lo,
                        int **subband_samples_hi,
                        DCA2DspData *dsp,
                        int nsamples, int perfect)
{
    LOCAL_ALIGNED(32, float, input, [64]);
    int i, j, k, sample;
    float *hist_ptr;

    for (sample = 0; sample < nsamples; sample++) {
        // Load in one sample from each subband
        if (subband_samples_hi) {
            // Full 64 subbands, first 32 are residual coded
            for (i =  0; i < 32; i++) {
                if ((i - 1) & 2)
                    input[i] = -subband_samples_lo[i][sample] - subband_samples_hi[i][sample];
                else
                    input[i] =  subband_samples_lo[i][sample] + subband_samples_hi[i][sample];
            }
            for (i = 32; i < 64; i++) {
                if ((i - 1) & 2)
                    input[i] = -subband_samples_hi[i][sample];
                else
                    input[i] =  subband_samples_hi[i][sample];
            }
        } else {
            // Only first 32 subbands
            for (i =  0; i < 32; i++) {
                if ((i - 1) & 2)
                    input[i] = -subband_samples_lo[i][sample];
                else
                    input[i] =  subband_samples_lo[i][sample];
            }
            for (i = 32; i < 64; i++)
                input[i] = 0;
        }

        // Get history pointer
        hist_ptr = dsp->u.flt64.hist1 + dsp->offset;

        // Inverse DCT
        imdct->imdct_half(imdct, hist_ptr, input);

        // One subband sample generates 64 interpolated ones
        for (i = 0, k = 31; i < 32; i++, k--) {
            float a = dsp->u.flt64.hist2[     i];
            float b = dsp->u.flt64.hist2[32 + i];
            float c = 0.0f;
            float d = 0.0f;

            for (j = 0; j < 1024 - dsp->offset; j += 128) {
                a -= hist_ptr[     k + j] * ff_dca_fir_64bands[     i + j];
                b += hist_ptr[     i + j] * ff_dca_fir_64bands[32 + i + j];
                c += hist_ptr[32 + i + j] * ff_dca_fir_64bands[64 + i + j];
                d += hist_ptr[32 + k + j] * ff_dca_fir_64bands[96 + i + j];
            }

            for (; j < 1024; j += 128) {
                a -= hist_ptr[     k + j - 1024] * ff_dca_fir_64bands[     i + j];
                b += hist_ptr[     i + j - 1024] * ff_dca_fir_64bands[32 + i + j];
                c += hist_ptr[32 + i + j - 1024] * ff_dca_fir_64bands[64 + i + j];
                d += hist_ptr[32 + k + j - 1024] * ff_dca_fir_64bands[96 + i + j];
            }

            // Save interpolated samples
            pcm_samples[     i] = a * (1.0f / (1 << 16));
            pcm_samples[32 + i] = b * (1.0f / (1 << 16));

            // Save intermediate history
            dsp->u.flt64.hist2[     i] = c;
            dsp->u.flt64.hist2[32 + i] = d;
        }

        // Advance output pointer
        pcm_samples += 64;

        // Shift history
        dsp->offset = (dsp->offset - 64) & 1023;
    }
}

av_cold void ff_dcadsp2_float_init(DCA2FloatDspContext *s)
{
    s->lfe_fir[0] = lfe_fir1_c;
    s->lfe_fir[1] = lfe_fir2_c;
    s->sub_qmf[0] = sub_qmf32_c;
    s->sub_qmf[1] = sub_qmf64_c;
}
