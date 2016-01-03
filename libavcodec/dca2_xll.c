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

static int get_linear(GetBitContext *gb, int n)
{
    unsigned int v;

    if (n == 0)
        return 0;

    v = get_bits(gb, n);
    return (v >> 1) ^ -(v & 1);
}

static int get_rice_un(GetBitContext *gb, int k)
{
    unsigned int v = get_unary(gb, 1, 128);
    return k ? (v << k) | get_bits(gb, k) : v;
}

static int get_rice(GetBitContext *gb, int k)
{
    unsigned int v = get_rice_un(gb, k);
    return (v >> 1) ^ -(v & 1);
}

static void get_array(GetBitContext *gb, int *array, int size, int n)
{
    int i;

    for (i = 0; i < size; i++)
        array[i] = get_bits(gb, n);
}

static void get_linear_array(GetBitContext *gb, int *array, int size, int n)
{
    int i;

    if (n == 0)
        memset(array, 0, sizeof(*array) * size);
    else for (i = 0; i < size; i++)
        array[i] = get_linear(gb, n);
}

static void get_rice_array(GetBitContext *gb, int *array, int size, int k)
{
    int i;

    for (i = 0; i < size; i++)
        array[i] = get_rice(gb, k);
}

static int parse_dmix_coeffs(DCA2XllDecoder *s, DCA2XllChSet *c)
{
    int i, j, m, n, *coeff_ptr;

    // Size of downmix coefficient matrix
    m = c->primary_chset ? ff_dca2_dmix_primary_nch[c->dmix_type] : c->hier_ofs;
    n = c->nchannels;

    coeff_ptr = c->dmix_coeff;
    for (i = 0; i < m; i++) {
        int code, sign, coeff, scale, scale_inv = 0;
        unsigned int index;

        // Downmix scale (only for non-primary channel sets)
        if (!c->primary_chset) {
            code = get_bits(&s->gb, 9);
            sign = (code >> 8) - 1;
            index = (code & 0xff) - 41;
            if (index >= FF_DCA_INV_DMIXTABLE_SIZE) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid XLL downmix scale index\n");
                return AVERROR_INVALIDDATA;
            }
            scale = ff_dca_dmixtable[index + 41];
            scale_inv = ff_dca_inv_dmixtable[index];
            c->dmix_scale[i] = (scale ^ sign) - sign;
            c->dmix_scale_inv[i] = (scale_inv ^ sign) - sign;
        }

        // Downmix coefficients
        for (j = 0; j < n; j++) {
            code = get_bits(&s->gb, 9);
            sign = (code >> 8) - 1;
            index = code & 0xff;
            if (index >= FF_DCA_DMIXTABLE_SIZE) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid XLL downmix coefficient index\n");
                return AVERROR_INVALIDDATA;
            }
            coeff = ff_dca_dmixtable[index];
            if (!c->primary_chset)
                // Multiply by |InvDmixScale| to get |UndoDmixScale|
                coeff = mul16(scale_inv, coeff);
            *coeff_ptr++ = (coeff ^ sign) - sign;
        }
    }

    return 0;
}

static int chs_parse_header(DCA2XllDecoder *s, DCA2XllChSet *c, DCA2ExssAsset *asset)
{
    int i, j, k, ret, band, header_size, header_pos = get_bits_count(&s->gb);
    DCA2XllChSet *p = &s->chset[0];
    DCA2XllBand *b;

    // Size of channel set sub-header
    header_size = get_bits(&s->gb, 10) + 1;

    // Check CRC
    if (ff_dca2_check_crc(&s->gb, header_pos, header_pos + header_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid XLL sub-header checksum\n");
        return AVERROR_INVALIDDATA;
    }

    // Number of channels in the channel set
    c->nchannels = get_bits(&s->gb, 4) + 1;
    if (c->nchannels > DCA2_XLL_CHANNELS_MAX) {
        av_log(s->avctx, AV_LOG_WARNING, "Unsupported number of XLL channels (%d)\n", c->nchannels);
        return AVERROR_PATCHWELCOME;
    }

    // Residual type
    c->residual_encode = get_bits(&s->gb, c->nchannels);

    // PCM bit resolution
    c->pcm_bit_res = get_bits(&s->gb, 5) + 1;

    // Storage unit width
    c->storage_bit_res = get_bits(&s->gb, 5) + 1;
    if (c->storage_bit_res != 16 && c->storage_bit_res != 24) {
        av_log(s->avctx, AV_LOG_WARNING, "Unsupported storage bit resolution for XLL channel set (%d)", c->storage_bit_res);
        return AVERROR_PATCHWELCOME;
    }

    if (c->pcm_bit_res > c->storage_bit_res) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid PCM bit resolution for XLL channel set (%d > %d)", c->pcm_bit_res, c->storage_bit_res);
        return AVERROR_INVALIDDATA;
    }

    // Original sampling frequency
    c->freq = ff_dca_sampling_freqs[get_bits(&s->gb, 4)];
    if (c->freq > 192000) {
        av_log(s->avctx, AV_LOG_WARNING, "Unsupported XLL sampling frequency (%d)\n", c->freq);
        return AVERROR_PATCHWELCOME;
    }

    // Sampling frequency modifier
    if (get_bits(&s->gb, 2)) {
        av_log(s->avctx, AV_LOG_WARNING, "Unsupported XLL sampling frequency modifier\n");
        return AVERROR_PATCHWELCOME;
    }

    // Which replacement set this channel set is member of
    if (get_bits(&s->gb, 2)) {
        av_log(s->avctx, AV_LOG_WARNING, "XLL replacement sets are not supported\n");
        return AVERROR_PATCHWELCOME;
    }

    if (asset->one_to_one_map_ch_to_spkr) {
        // Primary channel set flag
        c->primary_chset = get_bits1(&s->gb);
        if (c->primary_chset != (c == p)) {
            av_log(s->avctx, AV_LOG_ERROR, "The first (and only) XLL channel set must be primary\n");
            return AVERROR_INVALIDDATA;
        }

        // Downmix coefficients present in stream
        c->dmix_coeffs_present = get_bits1(&s->gb);

        // Downmix already performed by encoder
        c->dmix_embedded = c->dmix_coeffs_present && get_bits1(&s->gb);

        // Downmix type
        if (c->dmix_coeffs_present && c->primary_chset) {
            c->dmix_type = get_bits(&s->gb, 3);
            if (c->dmix_type >= DCA2_DMIX_TYPE_COUNT) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid XLL primary channel set downmix type\n");
                return AVERROR_INVALIDDATA;
            }
        }

        // Whether the channel set is part of a hierarchy
        c->hier_chset = get_bits1(&s->gb);
        if (!c->hier_chset && s->nchsets != 1) {
            av_log(s->avctx, AV_LOG_WARNING, "XLL channel sets outside of hierarchy are not supported\n");
            return AVERROR_PATCHWELCOME;
        }

        // Downmix coefficients
        if (c->dmix_coeffs_present && (ret = parse_dmix_coeffs(s, c)) < 0)
            return ret;

        // Channel mask enabled
        if (!get_bits1(&s->gb)) {
            av_log(s->avctx, AV_LOG_WARNING, "XLL channel sets with disabled channel mask are not supported\n");
            return AVERROR_PATCHWELCOME;
        }

        // Channel mask for set
        c->ch_mask = get_bits_long(&s->gb, s->ch_mask_nbits);
        if (av_popcount(c->ch_mask) != c->nchannels) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid XLL channel mask\n");
            return AVERROR_INVALIDDATA;
        }

        // Build the channel to speaker map
        for (i = 0, j = 0; i < s->ch_mask_nbits; i++)
            if (c->ch_mask & (1U << i))
                c->ch_remap[j++] = i;
    } else {
        // Mapping coeffs present flag
        if (c->nchannels != 2 || s->nchsets != 1 || get_bits1(&s->gb)) {
            av_log(s->avctx, AV_LOG_WARNING, "XLL channel sets with custom channel to speaker mapping are not supported\n");
            return AVERROR_PATCHWELCOME;
        }

        // Setup for LtRt decoding
        c->primary_chset = 1;
        c->dmix_coeffs_present = 0;
        c->dmix_embedded = 0;
        c->hier_chset = 0;
        c->ch_mask = DCA2_SPEAKER_LAYOUT_STEREO;
        c->ch_remap[0] = DCA2_SPEAKER_L;
        c->ch_remap[1] = DCA2_SPEAKER_R;
    }

    if (c->freq > 96000) {
        // Extra frequency bands flag
        if (get_bits1(&s->gb)) {
            av_log(s->avctx, AV_LOG_WARNING, "Extra XLL frequency bands are not supported\n");
            return AVERROR_PATCHWELCOME;
        }
        c->nfreqbands = 2;
    } else {
        c->nfreqbands = 1;
    }

    // Set the sampling frequency to that of the first frequency band.
    // Frequency will be doubled again after bands assembly.
    c->freq >>= c->nfreqbands - 1;

    // Verify that all channel sets have the same audio characteristics
    if (c != p && (c->freq != p->freq || c->pcm_bit_res != p->pcm_bit_res
        || c->storage_bit_res != p->storage_bit_res)) {
        av_log(s->avctx, AV_LOG_WARNING, "XLL channel sets with different audio characteristics are not supported\n");
        return AVERROR_PATCHWELCOME;
    }

    // Determine number of bits to read bit allocation coding parameter
    if (c->storage_bit_res > 16)
        c->nabits = 5;
    else if (c->storage_bit_res > 8)
        c->nabits = 4;
    else
        c->nabits = 3;

    // Account for embedded downmix and decimator saturation
    if ((s->nchsets > 1 || c->nfreqbands > 1) && c->nabits < 5)
        c->nabits++;

    for (band = 0, b = c->bands; band < c->nfreqbands; band++, b++) {
        // Pairwise channel decorrelation
        if ((b->decor_enabled = get_bits1(&s->gb)) && c->nchannels > 1) {
            int ch_nbits = av_ceil_log2(c->nchannels);

            // Original channel order
            for (i = 0; i < c->nchannels; i++) {
                b->orig_order[i] = get_bits(&s->gb, ch_nbits);
                if (b->orig_order[i] >= c->nchannels) {
                    av_log(s->avctx, AV_LOG_ERROR, "Invalid XLL original channel order\n");
                    return AVERROR_INVALIDDATA;
                }
            }

            // Pairwise channel coefficients
            for (i = 0; i < c->nchannels / 2; i++)
                b->decor_coeff[i] = get_bits1(&s->gb) ? get_linear(&s->gb, 7) : 0;
        } else {
            for (i = 0; i < c->nchannels; i++)
                b->orig_order[i] = i;
            for (i = 0; i < c->nchannels / 2; i++)
                b->decor_coeff[i] = 0;
        }

        // Adaptive predictor order
        b->highest_pred_order = 0;
        for (i = 0; i < c->nchannels; i++) {
            b->adapt_pred_order[i] = get_bits(&s->gb, 4);
            if (b->adapt_pred_order[i] > b->highest_pred_order)
                b->highest_pred_order = b->adapt_pred_order[i];
        }
        if (b->highest_pred_order > s->nsegsamples) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid XLL adaptive predicition order\n");
            return AVERROR_INVALIDDATA;
        }

        // Fixed predictor order
        for (i = 0; i < c->nchannels; i++)
            b->fixed_pred_order[i] = b->adapt_pred_order[i] ? 0 : get_bits(&s->gb, 2);

        // Adaptive predictor quantized reflection coefficients
        for (i = 0; i < c->nchannels; i++) {
            for (j = 0; j < b->adapt_pred_order[i]; j++) {
                k = get_linear(&s->gb, 8);
                if (k == -128) {
                    av_log(s->avctx, AV_LOG_ERROR, "Invalid XLL reflection coefficient index\n");
                    return AVERROR_INVALIDDATA;
                }
                if (k < 0)
                    b->adapt_refl_coeff[i][j] = -(int)ff_dca2_xll_refl_coeff[-k];
                else
                    b->adapt_refl_coeff[i][j] =  (int)ff_dca2_xll_refl_coeff[ k];
            }
        }

        // Downmix performed by encoder in extension frequency band
        b->dmix_embedded = c->dmix_embedded && (band == 0 || get_bits1(&s->gb));

        // MSB/LSB split flag in extension frequency band
        if ((band == 0 && s->scalable_lsbs) || (band != 0 && get_bits1(&s->gb))) {
            // Size of LSB section in any segment
            b->lsb_section_size = get_bits_long(&s->gb, s->seg_size_nbits);
            if (b->lsb_section_size < 0 || b->lsb_section_size > s->frame_size) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid LSB section size\n");
                return AVERROR_INVALIDDATA;
            }

            // Account for optional CRC bytes after LSB section
            if (b->lsb_section_size && (s->band_crc_present > 2 ||
                                        (band == 0 && s->band_crc_present > 1)))
                b->lsb_section_size += 2;

            // Number of bits to represent the samples in LSB part
            for (i = 0; i < c->nchannels; i++) {
                b->nscalablelsbs[i] = get_bits(&s->gb, 4);
                if (b->nscalablelsbs[i] && !b->lsb_section_size) {
                    av_log(s->avctx, AV_LOG_ERROR, "LSB section missing with non-zero LSB width\n");
                    return AVERROR_INVALIDDATA;
                }
            }
        } else {
            b->lsb_section_size = 0;
            for (i = 0; i < c->nchannels; i++)
                b->nscalablelsbs[i] = 0;
        }

        // Scalable resolution flag in extension frequency band
        if ((band == 0 && s->scalable_lsbs) || (band != 0 && get_bits1(&s->gb))) {
            // Number of bits discarded by authoring
            for (i = 0; i < c->nchannels; i++)
                b->bit_width_adjust[i] = get_bits(&s->gb, 4);
        } else {
            for (i = 0; i < c->nchannels; i++)
                b->bit_width_adjust[i] = 0;
        }
    }

    // Reserved
    // Byte align
    // CRC16 of channel set sub-header
    if (ff_dca2_seek_bits(&s->gb, header_pos + header_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Read past end of XLL sub-header\n");
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

static int chs_alloc_msb_band_data(DCA2XllDecoder *s, DCA2XllChSet *c)
{
    int ndecisamples = c->nfreqbands > 1 ? DCA2_XLL_DECI_HISTORY_MAX : 0;
    int nchsamples = s->nframesamples + ndecisamples;
    int nsamples = nchsamples * c->nchannels * c->nfreqbands;
    int i, j, *ptr;

    // Reallocate MSB sample buffer
    av_fast_malloc(&c->sample_buffer[0], &c->sample_size[0], nsamples * sizeof(int));
    if (!c->sample_buffer[0])
        return AVERROR(ENOMEM);

    ptr = c->sample_buffer[0] + ndecisamples;
    for (i = 0; i < c->nfreqbands; i++) {
        for (j = 0; j < c->nchannels; j++) {
            c->bands[i].msb_sample_buffer[j] = ptr;
            ptr += nchsamples;
        }
    }

    return 0;
}

static int chs_alloc_lsb_band_data(DCA2XllDecoder *s, DCA2XllChSet *c)
{
    int nsamples = 0;
    int i, j, *ptr;

    // Determine number of frequency bands that have MSB/LSB split
    for (i = 0; i < c->nfreqbands; i++)
        if (c->bands[i].lsb_section_size)
            nsamples += s->nframesamples * c->nchannels;
    if (!nsamples)
        return 0;

    // Reallocate LSB sample buffer
    av_fast_malloc(&c->sample_buffer[1], &c->sample_size[1], nsamples * sizeof(int));
    if (!c->sample_buffer[1])
        return AVERROR(ENOMEM);

    ptr = c->sample_buffer[1];
    for (i = 0; i < c->nfreqbands; i++) {
        if (c->bands[i].lsb_section_size) {
            for (j = 0; j < c->nchannels; j++) {
                c->bands[i].lsb_sample_buffer[j] = ptr;
                ptr += s->nframesamples;
            }
        } else {
            for (j = 0; j < c->nchannels; j++)
                c->bands[i].lsb_sample_buffer[j] = NULL;
        }
    }

    return 0;
}

static int chs_parse_band_data(DCA2XllDecoder *s, DCA2XllChSet *c, int band, int seg, int band_data_end)
{
    DCA2XllBand *b = &c->bands[band];
    int i, j, k;

    // Start unpacking MSB portion of the segment
    if (!(seg && get_bits1(&s->gb))) {
        // Unpack segment type
        // 0 - distinct coding parameters for each channel
        // 1 - common coding parameters for all channels
        c->seg_common = get_bits1(&s->gb);

        // Determine number of coding parameters encoded in segment
        k = c->seg_common ? 1 : c->nchannels;

        // Unpack Rice coding parameters
        for (i = 0; i < k; i++) {
            // Unpack Rice coding flag
            // 0 - linear code, 1 - Rice code
            c->rice_code_flag[i] = get_bits1(&s->gb);
            if (!c->seg_common && c->rice_code_flag[i]) {
                // Unpack Hybrid Rice coding flag
                // 0 - Rice code, 1 - Hybrid Rice code
                if (get_bits1(&s->gb))
                    // Unpack binary code length for isolated samples
                    c->bitalloc_hybrid_linear[i] = get_bits(&s->gb, c->nabits) + 1;
                else
                    // 0 indicates no Hybrid Rice coding
                    c->bitalloc_hybrid_linear[i] = 0;
            } else {
                // 0 indicates no Hybrid Rice coding
                c->bitalloc_hybrid_linear[i] = 0;
            }
        }

        // Unpack coding parameters
        for (i = 0; i < k; i++) {
            if (seg == 0) {
                // Unpack coding parameter for part A of segment 0
                c->bitalloc_part_a[i] = get_bits(&s->gb, c->nabits);

                // Adjust for the linear code
                if (!c->rice_code_flag[i] && c->bitalloc_part_a[i])
                    c->bitalloc_part_a[i]++;

                if (!c->seg_common)
                    c->nsamples_part_a[i] = b->adapt_pred_order[i];
                else
                    c->nsamples_part_a[i] = b->highest_pred_order;
            } else {
                c->bitalloc_part_a[i] = 0;
                c->nsamples_part_a[i] = 0;
            }

            // Unpack coding parameter for part B of segment
            c->bitalloc_part_b[i] = get_bits(&s->gb, c->nabits);

            // Adjust for the linear code
            if (!c->rice_code_flag[i] && c->bitalloc_part_b[i])
                c->bitalloc_part_b[i]++;
        }
    }

    // Unpack entropy codes
    for (i = 0; i < c->nchannels; i++) {
        int *part_a, *part_b, nsamples_part_b;

        // Select index of coding parameters
        k = c->seg_common ? 0 : i;

        // Slice the segment into parts A and B
        part_a = b->msb_sample_buffer[i] + seg * s->nsegsamples;
        part_b = part_a + c->nsamples_part_a[k];
        nsamples_part_b = s->nsegsamples - c->nsamples_part_a[k];

        if (get_bits_left(&s->gb) < 0)
            return AVERROR_INVALIDDATA;

        if (!c->rice_code_flag[k]) {
            // Linear codes
            // Unpack all residuals of part A of segment 0
            get_linear_array(&s->gb, part_a, c->nsamples_part_a[k],
                             c->bitalloc_part_a[k]);

            // Unpack all residuals of part B of segment 0 and others
            get_linear_array(&s->gb, part_b, nsamples_part_b,
                             c->bitalloc_part_b[k]);
        } else {
            // Rice codes
            // Unpack all residuals of part A of segment 0
            get_rice_array(&s->gb, part_a, c->nsamples_part_a[k],
                           c->bitalloc_part_a[k]);

            if (c->bitalloc_hybrid_linear[k]) {
                // Hybrid Rice codes
                // Unpack the number of isolated samples
                int nisosamples = get_bits(&s->gb, s->nsegsamples_log2);

                // Set all locations to 0
                memset(part_b, 0, sizeof(*part_b) * nsamples_part_b);

                // Extract the locations of isolated samples and flag by -1
                for (j = 0; j < nisosamples; j++) {
                    int loc = get_bits(&s->gb, s->nsegsamples_log2);
                    if (loc >= nsamples_part_b) {
                        av_log(s->avctx, AV_LOG_ERROR, "Invalid isolated sample location\n");
                        return AVERROR_INVALIDDATA;
                    }
                    part_b[loc] = -1;
                }

                // Unpack all residuals of part B of segment 0 and others
                for (j = 0; j < nsamples_part_b; j++) {
                    if (part_b[j])
                        part_b[j] = get_linear(&s->gb, c->bitalloc_hybrid_linear[k]);
                    else
                        part_b[j] = get_rice(&s->gb, c->bitalloc_part_b[k]);
                }
            } else {
                // Rice codes
                // Unpack all residuals of part B of segment 0 and others
                get_rice_array(&s->gb, part_b, nsamples_part_b, c->bitalloc_part_b[k]);
            }
        }
    }

    // Unpack decimator history for frequency band 1
    if (seg == 0 && band == 1) {
        int nbits = get_bits(&s->gb, 5) + 1;
        for (i = 0; i < c->nchannels; i++)
            for (j = 1; j < DCA2_XLL_DECI_HISTORY_MAX; j++)
                c->deci_history[i][j] = get_sbits_long(&s->gb, nbits);
    }

    // Start unpacking LSB portion of the segment
    if (b->lsb_section_size) {
        // Skip to the start of LSB portion
        if (ff_dca2_seek_bits(&s->gb, band_data_end - b->lsb_section_size * 8)) {
            av_log(s->avctx, AV_LOG_ERROR, "Read past end of band data\n");
            return AVERROR_INVALIDDATA;
        }

        // Unpack all LSB parts of residuals of this segment
        for (i = 0; i < c->nchannels; i++) {
            if (b->nscalablelsbs[i]) {
                get_array(&s->gb,
                          b->lsb_sample_buffer[i] + seg * s->nsegsamples,
                          s->nsegsamples, b->nscalablelsbs[i]);
            }
        }
    }

    // Skip to the end of band data
    if (ff_dca2_seek_bits(&s->gb, band_data_end)) {
        av_log(s->avctx, AV_LOG_ERROR, "Read past end of band data\n");
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

static void av_cold chs_clear_band_data(DCA2XllDecoder *s, DCA2XllChSet *c, int band, int seg)
{
    DCA2XllBand *b = &c->bands[band];
    int i, offset, nsamples;

    if (seg < 0) {
        offset = 0;
        nsamples = s->nframesamples;
    } else {
        offset = seg * s->nsegsamples;
        nsamples = s->nsegsamples;
    }

    for (i = 0; i < c->nchannels; i++) {
        memset(b->msb_sample_buffer[i] + offset, 0, nsamples * sizeof(int));
        if (b->lsb_section_size)
            memset(b->lsb_sample_buffer[i] + offset, 0, nsamples * sizeof(int));
    }

    if (seg <= 0 && band)
        memset(c->deci_history, 0, sizeof(c->deci_history));

    if (seg < 0) {
        memset(b->nscalablelsbs, 0, sizeof(b->nscalablelsbs));
        memset(b->bit_width_adjust, 0, sizeof(b->bit_width_adjust));
    }
}

static void chs_filter_band_data(DCA2XllDecoder *s, DCA2XllChSet *c, int band)
{
    DCA2XllBand *b = &c->bands[band];
    int nsamples = s->nframesamples;
    int i, j, k;

    // Inverse adaptive or fixed prediction
    for (i = 0; i < c->nchannels; i++) {
        int *buf = b->msb_sample_buffer[i];
        int order = b->adapt_pred_order[i];
        if (order > 0) {
            int coeff[DCA2_XLL_ADAPT_PRED_ORDER_MAX];
            // Conversion from reflection coefficients to direct form coefficients
            for (j = 0; j < order; j++) {
                int rc = b->adapt_refl_coeff[i][j];
                for (k = 0; k < (j + 1) / 2; k++) {
                    int tmp1 = coeff[    k    ];
                    int tmp2 = coeff[j - k - 1];
                    coeff[    k    ] = tmp1 + mul16(rc, tmp2);
                    coeff[j - k - 1] = tmp2 + mul16(rc, tmp1);
                }
                coeff[j] = rc;
            }
            // Inverse adaptive prediction
            for (j = 0; j < nsamples - order; j++) {
                int64_t err = INT64_C(0);
                for (k = 0; k < order; k++)
                    err += (int64_t)buf[j + k] * coeff[order - k - 1];
                buf[j + k] -= clip23(norm16(err));
            }
        } else {
            // Inverse fixed coefficient prediction
            for (j = 0; j < b->fixed_pred_order[i]; j++)
                for (k = 1; k < nsamples; k++)
                    buf[k] += buf[k - 1];
        }
    }

    // Inverse pairwise channel decorrellation
    if (b->decor_enabled) {
        int *tmp[DCA2_XLL_CHANNELS_MAX];

        for (i = 0; i < c->nchannels / 2; i++) {
            int coeff = b->decor_coeff[i];
            if (coeff) {
                int *src = b->msb_sample_buffer[i * 2    ];
                int *dst = b->msb_sample_buffer[i * 2 + 1];
                for (j = 0; j < nsamples; j++)
                    dst[j] += mul3(src[j], coeff);
            }
        }

        // Reorder channel pointers to the original order
        for (i = 0; i < c->nchannels; i++)
            tmp[i] = b->msb_sample_buffer[i];

        for (i = 0; i < c->nchannels; i++)
            b->msb_sample_buffer[b->orig_order[i]] = tmp[i];
    }

    // Map output channel pointers for frequency band 0
    if (c->nfreqbands == 1)
        for (i = 0; i < c->nchannels; i++)
            s->output_samples[c->ch_remap[i]] = b->msb_sample_buffer[i];

}

static int chs_get_lsb_width(DCA2XllDecoder *s, DCA2XllChSet *c, int band, int ch)
{
    int adj = c->bands[band].bit_width_adjust[ch];
    int shift = c->bands[band].nscalablelsbs[ch];

    if (s->fixed_lsb_width)
        shift = s->fixed_lsb_width;
    else if (shift && adj)
        shift += adj - 1;
    else
        shift += adj;

    return shift;
}

static void chs_assemble_msbs_lsbs(DCA2XllDecoder *s, DCA2XllChSet *c, int band)
{
    DCA2XllBand *b = &c->bands[band];
    int n, ch, nsamples = s->nframesamples;

    for (ch = 0; ch < c->nchannels; ch++) {
        int shift = chs_get_lsb_width(s, c, band, ch);
        if (shift) {
            int *msb = b->msb_sample_buffer[ch];
            if (b->nscalablelsbs[ch]) {
                int *lsb = b->lsb_sample_buffer[ch];
                int adj = b->bit_width_adjust[ch];
                for (n = 0; n < nsamples; n++)
                    msb[n] = msb[n] * (1 << shift) + (lsb[n] << adj);
            } else {
                for (n = 0; n < nsamples; n++)
                    msb[n] = msb[n] * (1 << shift);
            }
        }
    }
}

static int chs_assemble_freq_bands(DCA2XllDecoder *s, DCA2XllChSet *c)
{
    static const int32_t band_coeff1[DCA2_XLL_DECI_HISTORY_MAX] = {
          -20577,  122631,  -393647,  904476,
        -1696305, 2825313, -4430736, 6791313
    };

    static const int32_t band_coeff2[DCA2_XLL_DECI_HISTORY_MAX] = {
          41153,  -245210,  785564, -1788164,
        3259333, -5074941, 6928550, -8204883
    };

    int i, ch, nsamples = s->nframesamples, *ptr;

    // Reallocate frequency band assembly buffer
    av_fast_malloc(&c->sample_buffer[2], &c->sample_size[2],
                   2 * nsamples * c->nchannels * sizeof(int));
    if (!c->sample_buffer[2])
        return AVERROR(ENOMEM);

    // Assemble frequency bands 0 and 1
    ptr = c->sample_buffer[2];
    for (ch = 0; ch < c->nchannels; ch++) {
        int *band0 = c->bands[0].msb_sample_buffer[ch];
        int *band1 = c->bands[1].msb_sample_buffer[ch];

        // Copy decimator history
        for (i = 1; i < DCA2_XLL_DECI_HISTORY_MAX; i++)
            band0[i - DCA2_XLL_DECI_HISTORY_MAX] = c->deci_history[ch][i];

        // Filter
        vmul22_sub(band0, band1,   868669, nsamples);
        vmul22_sub(band1, band0, -5931642, nsamples);
        vmul22_sub(band0, band1, -1228483, nsamples);
        vmul22_sub(band1, band0,  1 << 22, nsamples);

        for (i = 0; i < DCA2_XLL_DECI_HISTORY_MAX; i++) {
            vmul23_sub(band0, band1, band_coeff1[i], nsamples);
            vmul23_sub(band1, band0, band_coeff2[i], nsamples);
            vmul23_sub(band0, band1, band_coeff1[i], nsamples);
            band0--;
        }

        // Remap output channel pointer to assembly buffer
        s->output_samples[c->ch_remap[ch]] = ptr;

        // Assemble
        for (i = 0; i < nsamples; i++) {
            *ptr++ = *band1++;
            *ptr++ = *++band0;
        }
    }

    return 0;
}

static int is_hier_dmix_chset(DCA2XllChSet *c)
{
    return !c->primary_chset && c->dmix_embedded && c->hier_chset;
}

static DCA2XllChSet *find_next_hier_dmix_chset(DCA2XllDecoder *s, DCA2XllChSet *c)
{
    if (c->hier_chset)
        while (++c < &s->chset[s->nchsets])
            if (is_hier_dmix_chset(c))
                return c;

    return NULL;
}

static void prescale_down_mix(DCA2XllChSet *c, DCA2XllChSet *o)
{
    int i, j, *coeff_ptr = c->dmix_coeff;

    for (i = 0; i < c->hier_ofs; i++) {
        int scale = o->dmix_scale[i];
        int scale_inv = o->dmix_scale_inv[i];
        c->dmix_scale[i] = mul15(c->dmix_scale[i], scale);
        c->dmix_scale_inv[i] = mul16(c->dmix_scale_inv[i], scale_inv);
        for (j = 0; j < c->nchannels; j++) {
            int coeff = mul16(*coeff_ptr, scale_inv);
            *coeff_ptr++ = mul15(coeff, o->dmix_scale[c->hier_ofs + j]);
        }
    }
}

static void undo_down_mix(DCA2XllDecoder *s, DCA2XllChSet *o, int band)
{
    int i, j, k, nchannels = 0, nsamples = s->nframesamples, *coeff_ptr = o->dmix_coeff;
    DCA2XllChSet *c;

    for (i = 0, c = s->chset; i < s->nactivechsets; i++, c++) {
        if (!c->hier_chset)
            continue;
        for (j = 0; j < c->nchannels; j++) {
            for (k = 0; k < o->nchannels; k++) {
                int coeff = *coeff_ptr++;
                if (coeff) {
                    vmul15_sub(c->bands[band].msb_sample_buffer[j],
                               o->bands[band].msb_sample_buffer[k],
                               coeff, nsamples);
                    if (band)
                        vmul15_sub(c->deci_history[j],
                                   o->deci_history[k],
                                   coeff, DCA2_XLL_DECI_HISTORY_MAX);
                }
            }
        }
        nchannels += c->nchannels;
        if (nchannels >= o->hier_ofs)
            break;
    }
}

static void scale_down_mix(DCA2XllDecoder *s, DCA2XllChSet *o, int band)
{
    int i, j, nchannels = 0, nsamples = s->nframesamples;
    DCA2XllChSet *c;

    for (i = 0, c = s->chset; i < s->nactivechsets; i++, c++) {
        if (!c->hier_chset)
            continue;
        for (j = 0; j < c->nchannels; j++) {
            int scale = o->dmix_scale[nchannels++];
            if (scale != (1 << 15)) {
                vmul15(c->bands[band].msb_sample_buffer[j], scale, nsamples);
                if (band)
                    vmul15(c->deci_history[j], scale, DCA2_XLL_DECI_HISTORY_MAX);
            }
        }
        if (nchannels >= o->hier_ofs)
            break;
    }
}

static void av_cold force_lossy_output(DCA2XllDecoder *s, DCA2CoreDecoder *core, DCA2XllChSet *c)
{
    int band, ch;

    // Clear all band data
    for (band = 0; band < c->nfreqbands; band++)
        chs_clear_band_data(s, c, band, -1);

    // Replace non-residual encoded channels with lossy counterparts
    for (ch = 0; ch < c->nchannels; ch++) {
        if (!(c->residual_encode & (1 << ch)))
            continue;
        if (ff_dca2_core_map_spkr(core, c->ch_remap[ch]) < 0)
            continue;
        c->residual_encode &= ~(1 << ch);
    }
}

static void combine_residual_frame(DCA2XllDecoder *s, DCA2CoreDecoder *core, DCA2XllChSet *c)
{
    int ch, nsamples = s->nframesamples;
    DCA2XllChSet *o;

    av_assert0(c->freq == core->output_rate);
    av_assert0(nsamples == core->npcmsamples);

    // See if this channel set is downmixed and find the next channel set in
    // hierarchy. If downmixed, undo core pre-scaling before combining with
    // residual (residual is not scaled).
    o = find_next_hier_dmix_chset(s, c);

    // Reduce core bit width and combine with residual
    for (ch = 0; ch < c->nchannels; ch++) {
        int n, *src, *dst, spkr, shift, round;

        if (c->residual_encode & (1 << ch))
            continue;

        spkr = ff_dca2_core_map_spkr(core, c->ch_remap[ch]);
        av_assert0(spkr >= 0);

        // Account for LSB width
        shift = 24 - c->pcm_bit_res + chs_get_lsb_width(s, c, 0, ch);
        round = shift > 0 ? 1 << (shift - 1) : 0;

        dst = c->bands[0].msb_sample_buffer[ch];
        src = core->output_samples[spkr];
        if (o) {
            // Undo embedded core downmix pre-scaling
            int scale_inv = o->dmix_scale_inv[c->hier_ofs + ch];
            for (n = 0; n < nsamples; n++)
                dst[n] += clip23((mul16(src[n], scale_inv) + round) >> shift);
        } else {
            // No downmix scaling
            for (n = 0; n < nsamples; n++)
                dst[n] += (src[n] + round) >> shift;
        }
    }
}

int ff_dca2_xll_filter(DCA2XllDecoder *s)
{
    DCA2Context *dca = s->avctx->priv_data;
    DCA2XllChSet *c;
    int i, j, ret;

    // Force lossy downmixed output during recovery
    if (dca->packet & DCA2_PACKET_RECOVERY) {
        for (i = 0, c = s->chset; i < s->nchsets; i++, c++) {
            if (i < s->nactivechsets)
                force_lossy_output(s, &dca->core, c);

            if (!c->primary_chset)
                c->dmix_embedded = 0;
        }

        s->scalable_lsbs = 0;
        s->fixed_lsb_width = 0;
    }

    // Filter frequency bands for active channel sets
    s->output_mask = 0;
    for (i = 0, c = s->chset; i < s->nactivechsets; i++, c++) {
        chs_filter_band_data(s, c, 0);

        if (c->residual_encode != (1 << c->nchannels) - 1)
            combine_residual_frame(s, &dca->core, c);

        if (s->scalable_lsbs)
            chs_assemble_msbs_lsbs(s, c, 0);

        if (c->nfreqbands > 1) {
            chs_filter_band_data(s, c, 1);
            chs_assemble_msbs_lsbs(s, c, 1);
        }

        s->output_mask |= c->ch_mask;
    }

    // Undo hierarchial downmix and/or apply scaling
    for (i = 1, c = &s->chset[1]; i < s->nchsets; i++, c++) {
        if (!is_hier_dmix_chset(c))
            continue;

        if (i >= s->nactivechsets) {
            for (j = 0; j < c->nfreqbands; j++)
                if (c->bands[j].dmix_embedded)
                    scale_down_mix(s, c, j);
            break;
        }

        for (j = 0; j < c->nfreqbands; j++)
            if (c->bands[j].dmix_embedded)
                undo_down_mix(s, c, j);
    }

    // Assemble frequency bands for active channel sets
    if (s->nfreqbands > 1) {
        for (i = 0; i < s->nactivechsets; i++)
            if ((ret = chs_assemble_freq_bands(s, &s->chset[i])) < 0)
                return ret;
    }

    // Normalize to regular 5.1 layout if downmixing
    if (dca->request_channel_layout) {
        if (s->output_mask & DCA2_SPEAKER_MASK_Lss) {
            s->output_samples[DCA2_SPEAKER_Ls] = s->output_samples[DCA2_SPEAKER_Lss];
            s->output_mask = (s->output_mask & ~DCA2_SPEAKER_MASK_Lss) | DCA2_SPEAKER_MASK_Ls;
        }
        if (s->output_mask & DCA2_SPEAKER_MASK_Rss) {
            s->output_samples[DCA2_SPEAKER_Rs] = s->output_samples[DCA2_SPEAKER_Rss];
            s->output_mask = (s->output_mask & ~DCA2_SPEAKER_MASK_Rss) | DCA2_SPEAKER_MASK_Rs;
        }
    }

    return 0;
}

static int parse_common_header(DCA2XllDecoder *s)
{
    int stream_ver, header_size, frame_size_nbits, nframesegs_log2;

    // XLL extension sync word
    if (get_bits_long(&s->gb, 32) != DCA_SYNCWORD_XLL) {
        av_log(s->avctx, AV_LOG_VERBOSE, "Invalid XLL sync word\n");
        return AVERROR(EAGAIN);
    }

    // Version number
    stream_ver = get_bits(&s->gb, 4) + 1;
    if (stream_ver > 1) {
        av_log(s->avctx, AV_LOG_WARNING, "Unsupported XLL stream version (%d)\n", stream_ver);
        return AVERROR_PATCHWELCOME;
    }

    // Lossless frame header length
    header_size = get_bits(&s->gb, 8) + 1;

    // Check CRC
    if (ff_dca2_check_crc(&s->gb, 32, header_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid XLL common header checksum\n");
        return AVERROR_INVALIDDATA;
    }

    // Number of bits used to read frame size
    frame_size_nbits = get_bits(&s->gb, 5) + 1;

    // Number of bytes in a lossless frame
    s->frame_size = get_bits_long(&s->gb, frame_size_nbits);
    if (s->frame_size < 0 || s->frame_size >= DCA2_XLL_PBR_BUFFER_MAX) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid XLL frame size (%d)\n", s->frame_size);
        return AVERROR_INVALIDDATA;
    }
    s->frame_size++;

    // Number of channels sets per frame
    s->nchsets = get_bits(&s->gb, 4) + 1;
    if (s->nchsets > DCA2_XLL_CHSETS_MAX) {
        av_log(s->avctx, AV_LOG_WARNING, "Unsupported number of XLL channel sets (%d)\n", s->nchsets);
        return AVERROR_PATCHWELCOME;
    }

    // Number of segments per frame
    nframesegs_log2 = get_bits(&s->gb, 4);
    s->nframesegs = 1 << nframesegs_log2;
    if (s->nframesegs > 1024) {
        av_log(s->avctx, AV_LOG_ERROR, "Too many segments per XLL frame\n");
        return AVERROR_INVALIDDATA;
    }

    // Samples in segment per one frequency band for the first channel set
    // Maximum value is 256 for sampling frequencies <= 48 kHz
    // Maximum value is 512 for sampling frequencies > 48 kHz
    s->nsegsamples_log2 = get_bits(&s->gb, 4);
    if (!s->nsegsamples_log2) {
        av_log(s->avctx, AV_LOG_ERROR, "Too few samples per XLL segment\n");
        return AVERROR_INVALIDDATA;
    }
    s->nsegsamples = 1 << s->nsegsamples_log2;
    if (s->nsegsamples > 512) {
        av_log(s->avctx, AV_LOG_ERROR, "Too many samples per XLL segment\n");
        return AVERROR_INVALIDDATA;
    }

    // Samples in frame per one frequency band for the first channel set
    s->nframesamples_log2 = s->nsegsamples_log2 + nframesegs_log2;
    s->nframesamples = 1 << s->nframesamples_log2;
    if (s->nframesamples > 65536) {
        av_log(s->avctx, AV_LOG_ERROR, "Too many samples per XLL frame\n");
        return AVERROR_INVALIDDATA;
    }

    // Number of bits used to read segment size
    s->seg_size_nbits = get_bits(&s->gb, 5) + 1;

    // Presence of CRC16 within each frequency band
    // 0 - No CRC16 within band
    // 1 - CRC16 placed at the end of MSB0
    // 2 - CRC16 placed at the end of MSB0 and LSB0
    // 3 - CRC16 placed at the end of MSB0 and LSB0 and other frequency bands
    s->band_crc_present = get_bits(&s->gb, 2);

    // MSB/LSB split flag
    s->scalable_lsbs = get_bits1(&s->gb);

    // Channel position mask
    s->ch_mask_nbits = get_bits(&s->gb, 5) + 1;

    // Fixed LSB width
    if (s->scalable_lsbs)
        s->fixed_lsb_width = get_bits(&s->gb, 4);
    else
        s->fixed_lsb_width = 0;

    // Reserved
    // Byte align
    // Header CRC16 protection
    if (ff_dca2_seek_bits(&s->gb, header_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Read past end of XLL common header\n");
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

static int parse_sub_headers(DCA2XllDecoder *s, DCA2ExssAsset *asset)
{
    DCA2Context *dca = s->avctx->priv_data;
    DCA2XllChSet *c;
    int i, ret;

    // Parse channel set headers
    s->nfreqbands = 0;
    s->nchannels = 0;
    for (i = 0, c = s->chset; i < s->nchsets; i++, c++) {
        c->hier_ofs = s->nchannels;
        if ((ret = chs_parse_header(s, c, asset)) < 0)
            return ret;
        if (c->nfreqbands > s->nfreqbands)
            s->nfreqbands = c->nfreqbands;
        if (c->hier_chset)
            s->nchannels += c->nchannels;
    }

    // Pre-scale downmixing coefficients for all non-primary channel sets
    for (i = s->nchsets - 1, c = &s->chset[i]; i > 0; i--, c--) {
        if (is_hier_dmix_chset(c)) {
            DCA2XllChSet *o = find_next_hier_dmix_chset(s, c);
            if (o)
                prescale_down_mix(c, o);
        }
    }

    // Determine number of active channel sets to decode
    switch (dca->request_channel_layout) {
    case DCA2_SPEAKER_LAYOUT_STEREO:
        s->nactivechsets = 1;
        break;
    case DCA2_SPEAKER_LAYOUT_5POINT0:
    case DCA2_SPEAKER_LAYOUT_5POINT1:
        s->nactivechsets = (s->chset[0].nchannels < 5 && s->nchsets > 1) ? 2 : 1;
        break;
    default:
        s->nactivechsets = s->nchsets;
        break;
    }

    return 0;
}

static int parse_navi_table(DCA2XllDecoder *s)
{
    int chs, seg, band, navi_nb, navi_pos, *navi_ptr;
    DCA2XllChSet *c;

    // Determine size of NAVI table
    navi_nb = s->nfreqbands * s->nframesegs * s->nchsets;
    if (navi_nb > 1024) {
        av_log(s->avctx, AV_LOG_ERROR, "Too many NAVI entries (%d)\n", navi_nb);
        return AVERROR_INVALIDDATA;
    }

    // Reallocate NAVI table
    av_fast_malloc(&s->navi, &s->navi_size, navi_nb * sizeof(*s->navi));
    if (!s->navi)
        return AVERROR(ENOMEM);

    // Parse NAVI
    navi_pos = get_bits_count(&s->gb);
    navi_ptr = s->navi;
    for (band = 0; band < s->nfreqbands; band++) {
        for (seg = 0; seg < s->nframesegs; seg++) {
            for (chs = 0, c = s->chset; chs < s->nchsets; chs++, c++) {
                int size = 0;
                if (c->nfreqbands > band) {
                    size = get_bits_long(&s->gb, s->seg_size_nbits);
                    if (size < 0 || size >= s->frame_size) {
                        av_log(s->avctx, AV_LOG_ERROR, "Invalid NAVI segment size (%d)\n", size);
                        return AVERROR_INVALIDDATA;
                    }
                    size++;
                }
                *navi_ptr++ = size;
            }
        }
    }

    // Byte align
    // CRC16
    skip_bits(&s->gb, -get_bits_count(&s->gb) & 7);
    skip_bits(&s->gb, 16);

    // Check CRC
    if (ff_dca2_check_crc(&s->gb, navi_pos, get_bits_count(&s->gb))) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid NAVI checksum\n");
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

static int parse_band_data(DCA2XllDecoder *s)
{
    int ret, chs, seg, band, navi_pos, *navi_ptr;
    DCA2XllChSet *c;

    for (chs = 0, c = s->chset; chs < s->nactivechsets; chs++, c++) {
        if ((ret = chs_alloc_msb_band_data(s, c)) < 0)
            return ret;
        if ((ret = chs_alloc_lsb_band_data(s, c)) < 0)
            return ret;
    }

    navi_pos = get_bits_count(&s->gb);
    navi_ptr = s->navi;
    for (band = 0; band < s->nfreqbands; band++) {
        for (seg = 0; seg < s->nframesegs; seg++) {
            for (chs = 0, c = s->chset; chs < s->nchsets; chs++, c++) {
                if (c->nfreqbands > band) {
                    navi_pos += *navi_ptr * 8;
                    if (navi_pos > s->gb.size_in_bits) {
                        av_log(s->avctx, AV_LOG_ERROR, "Invalid NAVI position\n");
                        return AVERROR_INVALIDDATA;
                    }
                    if (chs < s->nactivechsets &&
                        (ret = chs_parse_band_data(s, c, band, seg, navi_pos)) < 0) {
                        if (s->avctx->err_recognition & AV_EF_EXPLODE)
                            return ret;
                        chs_clear_band_data(s, c, band, seg);
                    }
                    s->gb.index = navi_pos;
                }
                navi_ptr++;
            }
        }
    }

    return 0;
}

static int parse_frame(DCA2XllDecoder *s, uint8_t *data, int size, DCA2ExssAsset *asset)
{
    int ret;

    if ((ret = init_get_bits8(&s->gb, data, size)) < 0)
        return ret;
    if ((ret = parse_common_header(s)) < 0)
        return ret;
    if ((ret = parse_sub_headers(s, asset)) < 0)
        return ret;
    if ((ret = parse_navi_table(s)) < 0)
        return ret;
    if ((ret = parse_band_data(s)) < 0)
        return ret;
    if (ff_dca2_seek_bits(&s->gb, s->frame_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Read past end of XLL frame\n");
        return AVERROR_INVALIDDATA;
    }
    return ret;
}

static void clear_pbr(DCA2XllDecoder *s)
{
    s->pbr_length = 0;
    s->pbr_delay = 0;
}

static int copy_to_pbr(DCA2XllDecoder *s, uint8_t *data, int size, int delay)
{
    if (size > DCA2_XLL_PBR_BUFFER_MAX)
        return AVERROR(ENOSPC);

    if (!s->pbr_buffer && !(s->pbr_buffer = av_malloc(DCA2_XLL_PBR_BUFFER_MAX + DCA2_BUFFER_PADDING_SIZE)))
        return AVERROR(ENOMEM);

    memcpy(s->pbr_buffer, data, size);
    s->pbr_length = size;
    s->pbr_delay = delay;
    return 0;
}

static int parse_frame_no_pbr(DCA2XllDecoder *s, uint8_t *data, int size, DCA2ExssAsset *asset)
{
    int ret = parse_frame(s, data, size, asset);

    // If XLL packet data didn't start with a sync word, we must have jumped
    // right into the middle of PBR smoothing period
    if (ret == AVERROR(EAGAIN) && asset->xll_sync_present && asset->xll_sync_offset < size) {
        // Skip to the next sync word in this packet
        data += asset->xll_sync_offset;
        size -= asset->xll_sync_offset;

        // If decoding delay is set, put the frame into PBR buffer and return
        // failure code. Higher level decoder is expected to switch to lossy
        // core decoding or mute its output until decoding delay expires.
        if (asset->xll_delay_nframes > 0) {
            if ((ret = copy_to_pbr(s, data, size, asset->xll_delay_nframes)) < 0)
                return ret;
            return AVERROR(EAGAIN);
        }

        // No decoding delay, just parse the frame in place
        ret = parse_frame(s, data, size, asset);
    }

    if (ret < 0)
        return ret;

    if (s->frame_size > size)
        return AVERROR(EINVAL);

    // If the XLL decoder didn't consume full packet, start PBR smoothing period
    if (s->frame_size < size)
        if ((ret = copy_to_pbr(s, data + s->frame_size, size - s->frame_size, 0)) < 0)
            return ret;

    return 0;
}

static int parse_frame_pbr(DCA2XllDecoder *s, uint8_t *data, int size, DCA2ExssAsset *asset)
{
    int ret;

    if (size > DCA2_XLL_PBR_BUFFER_MAX - s->pbr_length) {
        ret = AVERROR(ENOSPC);
        goto fail;
    }

    memcpy(s->pbr_buffer + s->pbr_length, data, size);
    s->pbr_length += size;

    // Respect decoding delay after synchronization error
    if (s->pbr_delay > 0 && --s->pbr_delay)
        return AVERROR(EAGAIN);

    if ((ret = parse_frame(s, s->pbr_buffer, s->pbr_length, asset)) < 0)
        goto fail;

    if (s->frame_size > s->pbr_length) {
        ret = AVERROR(EINVAL);
        goto fail;
    }

    if (s->frame_size == s->pbr_length) {
        // End of PBR smoothing period
        clear_pbr(s);
    } else {
        s->pbr_length -= s->frame_size;
        memmove(s->pbr_buffer, s->pbr_buffer + s->frame_size, s->pbr_length);
    }

    return 0;

fail:
    // For now, throw out all PBR state on failure.
    // Perhaps we can be smarter and try to resync somehow.
    clear_pbr(s);
    return ret;
}

int ff_dca2_xll_parse(DCA2XllDecoder *s, uint8_t *data, DCA2ExssAsset *asset)
{
    int ret;

    if (s->hd_stream_id != asset->hd_stream_id) {
        clear_pbr(s);
        s->hd_stream_id = asset->hd_stream_id;
    }

    if (s->pbr_length)
        ret = parse_frame_pbr(s, data + asset->xll_offset, asset->xll_size, asset);
    else
        ret = parse_frame_no_pbr(s, data + asset->xll_offset, asset->xll_size, asset);

    return ret;
}

av_cold void ff_dca2_xll_flush(DCA2XllDecoder *s)
{
    clear_pbr(s);
}

av_cold void ff_dca2_xll_close(DCA2XllDecoder *s)
{
    DCA2XllChSet *c;
    int i, j;

    for (i = 0, c = s->chset; i < DCA2_XLL_CHSETS_MAX; i++, c++) {
        for (j = 0; j < DCA2_XLL_SAMPLE_BUFFERS_MAX; j++) {
            av_freep(&c->sample_buffer[j]);
            c->sample_size[j] = 0;
        }
    }

    av_freep(&s->navi);
    s->navi_size = 0;

    av_freep(&s->pbr_buffer);
    clear_pbr(s);
}
