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

#include "dca.h"    // for avpriv_dca_sample_rates[]
#include "dca2.h"
#include "dca2_math.h"

#define  DCA_SYNCWORD_REV1AUX   0x9A1105A0u

enum HeaderType {
    HEADER_CORE,
    HEADER_XCH,
    HEADER_XXCH
};

enum AudioMode {
    AMODE_MONO,             // Mode 0: A (mono)
    AMODE_MONO_DUAL,        // Mode 1: A + B (dual mono)
    AMODE_STEREO,           // Mode 2: L + R (stereo)
    AMODE_STEREO_SUMDIFF,   // Mode 3: (L+R) + (L-R) (sum-diff)
    AMODE_STEREO_TOTAL,     // Mode 4: LT + RT (left and right total)
    AMODE_3F,               // Mode 5: C + L + R
    AMODE_2F1R,             // Mode 6: L + R + S
    AMODE_3F1R,             // Mode 7: C + L + R + S
    AMODE_2F2R,             // Mode 8: L + R + SL + SR
    AMODE_3F2R,             // Mode 9: C + L + R + SL + SR

    AMODE_COUNT
};

enum ExtAudioType {
    EXT_AUDIO_XCH   = 0,
    EXT_AUDIO_X96   = 2,
    EXT_AUDIO_XXCH  = 6
};

enum LFEFlag {
    LFE_FLAG_NONE,
    LFE_FLAG_128,
    LFE_FLAG_64,
    LFE_FLAG_INVALID
};

static const int8_t prm_ch_to_spkr_map[AMODE_COUNT][5] = {
    { DCA2_SPEAKER_C,             -1,              -1,              -1,              -1 },
    { DCA2_SPEAKER_L, DCA2_SPEAKER_R,              -1,              -1,              -1 },
    { DCA2_SPEAKER_L, DCA2_SPEAKER_R,              -1,              -1,              -1 },
    { DCA2_SPEAKER_L, DCA2_SPEAKER_R,              -1,              -1,              -1 },
    { DCA2_SPEAKER_L, DCA2_SPEAKER_R,              -1,              -1,              -1 },
    { DCA2_SPEAKER_C, DCA2_SPEAKER_L, DCA2_SPEAKER_R ,              -1,              -1 },
    { DCA2_SPEAKER_L, DCA2_SPEAKER_R, DCA2_SPEAKER_Cs,              -1,              -1 },
    { DCA2_SPEAKER_C, DCA2_SPEAKER_L, DCA2_SPEAKER_R , DCA2_SPEAKER_Cs,              -1 },
    { DCA2_SPEAKER_L, DCA2_SPEAKER_R, DCA2_SPEAKER_Ls, DCA2_SPEAKER_Rs,              -1 },
    { DCA2_SPEAKER_C, DCA2_SPEAKER_L, DCA2_SPEAKER_R,  DCA2_SPEAKER_Ls, DCA2_SPEAKER_Rs }
};

static const uint8_t audio_mode_ch_mask[AMODE_COUNT] = {
    DCA2_SPEAKER_LAYOUT_MONO,
    DCA2_SPEAKER_LAYOUT_STEREO,
    DCA2_SPEAKER_LAYOUT_STEREO,
    DCA2_SPEAKER_LAYOUT_STEREO,
    DCA2_SPEAKER_LAYOUT_STEREO,
    DCA2_SPEAKER_LAYOUT_3_0,
    DCA2_SPEAKER_LAYOUT_2_1,
    DCA2_SPEAKER_LAYOUT_3_1,
    DCA2_SPEAKER_LAYOUT_2_2,
    DCA2_SPEAKER_LAYOUT_5POINT0
};

// ============================================================================

#include "dcahuff.h"

static const uint8_t quant_index_group_size[DCA2_CODE_BOOKS] = {
    1, 3, 3, 3, 3, 7, 7, 7, 7, 7
};

typedef struct DCA2VLC {
    int offset;         ///< Code values offset
    int max_depth;      ///< Parameter for get_vlc2()
    VLC vlc[7];         ///< Actual codes
} DCA2VLC;

static DCA2VLC  vlc_bit_allocation;
static DCA2VLC  vlc_transition_mode;
static DCA2VLC  vlc_scale_factor;
static DCA2VLC  vlc_quant_index[DCA2_CODE_BOOKS];

static av_cold void dca2_init_vlcs(void)
{
    static VLC_TYPE dca_table[23622][2];
    static int vlcs_initialized = 0;
    int i, j, k;

    if (vlcs_initialized)
        return;

#define DCA2_INIT_VLC(vlc, a, b, c, d)                                     \
    do {                                                                   \
        vlc.table           = &dca_table[ff_dca_vlc_offs[k]];              \
        vlc.table_allocated = ff_dca_vlc_offs[k + 1] - ff_dca_vlc_offs[k]; \
        init_vlc(&vlc, a, b, c, 1, 1, d, 2, 2, INIT_VLC_USE_NEW_STATIC);   \
    } while (0)

    vlc_bit_allocation.offset    = 1;
    vlc_bit_allocation.max_depth = 2;
    for (i = 0, k = 0; i < 5; i++, k++)
        DCA2_INIT_VLC(vlc_bit_allocation.vlc[i], bitalloc_12_vlc_bits[i], 12,
                      bitalloc_12_bits[i], bitalloc_12_codes[i]);

    vlc_scale_factor.offset    = -64;
    vlc_scale_factor.max_depth = 2;
    for (i = 0; i < 5; i++, k++)
        DCA2_INIT_VLC(vlc_scale_factor.vlc[i], SCALES_VLC_BITS, 129,
                      scales_bits[i], scales_codes[i]);

    vlc_transition_mode.offset    = 0;
    vlc_transition_mode.max_depth = 1;
    for (i = 0; i < 4; i++, k++)
        DCA2_INIT_VLC(vlc_transition_mode.vlc[i], tmode_vlc_bits[i], 4,
                      tmode_bits[i], tmode_codes[i]);

    for (i = 0; i < DCA2_CODE_BOOKS; i++) {
        vlc_quant_index[i].offset    = bitalloc_offsets[i];
        vlc_quant_index[i].max_depth = 1 + (i > 5);
        for (j = 0; j < quant_index_group_size[i]; j++, k++)
            DCA2_INIT_VLC(vlc_quant_index[i].vlc[j], bitalloc_maxbits[i][j],
                          bitalloc_sizes[i], bitalloc_bits[i][j], bitalloc_codes[i][j]);
    }

    vlcs_initialized = 1;
}

static int get_vlc(GetBitContext *s, DCA2VLC *v, int i)
{
    return get_vlc2(s, v->vlc[i].table, v->vlc[i].bits, v->max_depth) + v->offset;
}

static void get_array(GetBitContext *s, int *array, int size, int n)
{
    int i;

    for (i = 0; i < size; i++)
        array[i] = get_sbits(s, n);
}

// ============================================================================

// 5.3.1 - Bit stream header
static int parse_frame_header(DCA2CoreDecoder *s)
{
    int normal_frame, pcmr_index;

    // Frame type
    normal_frame = get_bits1(&s->gb);

    // Deficit sample count
    if (get_bits(&s->gb, 5) != DCA2_PCMBLOCK_SAMPLES - 1) {
        av_log(s->avctx, AV_LOG_ERROR, "Deficit samples are not supported\n");
        return normal_frame ? AVERROR_INVALIDDATA : AVERROR_PATCHWELCOME;
    }

    // CRC present flag
    s->crc_present = get_bits1(&s->gb);

    // Number of PCM sample blocks
    s->npcmblocks = get_bits(&s->gb, 7) + 1;
    if (s->npcmblocks & (DCA2_SUBBAND_SAMPLES - 1)) {
        av_log(s->avctx, AV_LOG_ERROR, "Unsupported number of PCM sample blocks (%d)\n", s->npcmblocks);
        return (s->npcmblocks < 6 || normal_frame) ? AVERROR_INVALIDDATA : AVERROR_PATCHWELCOME;
    }

    // Primary frame byte size
    s->frame_size = get_bits(&s->gb, 14) + 1;
    if (s->frame_size < 96) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid core frame size (%d bytes)\n", s->frame_size);
        return AVERROR_INVALIDDATA;
    }

    // Audio channel arrangement
    s->audio_mode = get_bits(&s->gb, 6);
    if (s->audio_mode >= AMODE_COUNT) {
        av_log(s->avctx, AV_LOG_ERROR, "Unsupported audio channel arrangement (%d)\n", s->audio_mode);
        return AVERROR_PATCHWELCOME;
    }

    // Core audio sampling frequency
    s->sample_rate = avpriv_dca_sample_rates[get_bits(&s->gb, 4)];
    if (!s->sample_rate) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid core audio sampling frequency\n");
        return AVERROR_INVALIDDATA;
    }

    // Transmission bit rate
    s->bit_rate = ff_dca_bit_rates[get_bits(&s->gb, 5)];

    // Reserved field
    skip_bits1(&s->gb);

    // Embedded dynamic range flag
    s->drc_present = get_bits1(&s->gb);

    // Embedded time stamp flag
    s->ts_present = get_bits1(&s->gb);

    // Auxiliary data flag
    s->aux_present = get_bits1(&s->gb);

    // HDCD mastering flag
    skip_bits1(&s->gb);

    // Extension audio descriptor flag
    s->ext_audio_type = get_bits(&s->gb, 3);

    // Extended coding flag
    s->ext_audio_present = get_bits1(&s->gb);

    // Audio sync word insertion flag
    s->sync_ssf = get_bits1(&s->gb);

    // Low frequency effects flag
    s->lfe_present = get_bits(&s->gb, 2);
    if (s->lfe_present == LFE_FLAG_INVALID) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid low frequency effects flag\n");
        return AVERROR_INVALIDDATA;
    }

    // Predictor history flag switch
    s->predictor_history = get_bits1(&s->gb);

    // Header CRC check bytes
    if (s->crc_present)
        skip_bits(&s->gb, 16);

    // Multirate interpolator switch
    s->filter_perfect = get_bits1(&s->gb);

    // Encoder software revision
    skip_bits(&s->gb, 4);

    // Copy history
    skip_bits(&s->gb, 2);

    // Source PCM resolution
    s->source_pcm_res = ff_dca_bits_per_sample[pcmr_index = get_bits(&s->gb, 3)];
    if (!s->source_pcm_res) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid source PCM resolution\n");
        return AVERROR_INVALIDDATA;
    }
    s->es_format = pcmr_index & 1;

    // Front sum/difference flag
    s->sumdiff_front = get_bits1(&s->gb);

    // Surround sum/difference flag
    s->sumdiff_surround = get_bits1(&s->gb);

    // Dialog normalization / unspecified
    skip_bits(&s->gb, 4);

    return 0;
}

// 5.3.2 - Primary audio coding header
static int parse_coding_header(DCA2CoreDecoder *s, enum HeaderType header, int xch_base)
{
    int n, ch, nchannels, header_size = 0, header_pos = get_bits_count(&s->gb);
    unsigned int mask, index;

    if (get_bits_left(&s->gb) < 0)
        return AVERROR_INVALIDDATA;

    switch (header) {
    case HEADER_CORE:
        // Number of subframes
        s->nsubframes = get_bits(&s->gb, 4) + 1;

        // Number of primary audio channels
        s->nchannels = get_bits(&s->gb, 3) + 1;
        if (s->nchannels != ff_dca_channels[s->audio_mode]) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid number of primary audio channels (%d) for audio channel arrangement (%d)\n", s->nchannels, s->audio_mode);
            return AVERROR_INVALIDDATA;
        }
        av_assert0(s->nchannels <= DCA2_CHANNELS - 2);

        s->ch_mask = audio_mode_ch_mask[s->audio_mode];

        // Add LFE channel if present
        if (s->lfe_present)
            s->ch_mask |= DCA2_SPEAKER_MASK_LFE1;
        break;

    case HEADER_XCH:
        s->nchannels = ff_dca_channels[s->audio_mode] + 1;
        av_assert0(s->nchannels <= DCA2_CHANNELS - 1);
        s->ch_mask |= DCA2_SPEAKER_MASK_Cs;
        break;

    case HEADER_XXCH:
        // Channel set header length
        header_size = get_bits(&s->gb, 7) + 1;

        // Check CRC
        if (s->xxch_crc_present && ff_dca2_check_crc(&s->gb, header_pos, header_pos + header_size * 8)) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid XXCH channel set header checksum\n");
            return AVERROR_INVALIDDATA;
        }

        // Number of channels in a channel set
        nchannels = get_bits(&s->gb, 3) + 1;
        if (nchannels > DCA2_XXCH_CHANNELS_MAX) {
            av_log(s->avctx, AV_LOG_WARNING, "Unsupported number of XXCH channels (%d)\n", nchannels);
            return AVERROR_PATCHWELCOME;
        }
        s->nchannels = ff_dca_channels[s->audio_mode] + nchannels;
        av_assert0(s->nchannels <= DCA2_CHANNELS);

        // Loudspeaker layout mask
        mask = get_bits_long(&s->gb, s->xxch_mask_nbits - DCA2_SPEAKER_Cs);
        s->xxch_spkr_mask = mask << DCA2_SPEAKER_Cs;

        if (av_popcount(s->xxch_spkr_mask) != nchannels) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid XXCH speaker layout mask (%#x)\n", s->xxch_spkr_mask);
            return AVERROR_INVALIDDATA;
        }

        if (s->xxch_core_mask & s->xxch_spkr_mask) {
            av_log(s->avctx, AV_LOG_ERROR, "XXCH speaker layout mask (%#x) overlaps with core (%#x)\n", s->xxch_spkr_mask, s->xxch_core_mask);
            return AVERROR_INVALIDDATA;
        }

        // Combine core and XXCH masks together
        s->ch_mask = s->xxch_core_mask | s->xxch_spkr_mask;

        // Downmix coefficients present in stream
        if (get_bits1(&s->gb)) {
            int *coeff_ptr;

            // Downmix already performed by encoder
            s->xxch_dmix_embedded = get_bits1(&s->gb);

            // Downmix scale factor
            index = get_bits(&s->gb, 6) * 4 - 44;
            if (index >= FF_DCA_INV_DMIXTABLE_SIZE) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid XXCH downmix scale index (%d)\n", index);
                return AVERROR_INVALIDDATA;
            }
            s->xxch_dmix_scale_inv = ff_dca_inv_dmixtable[index];

            // Downmix channel mapping mask
            for (ch = 0; ch < nchannels; ch++) {
                mask = get_bits_long(&s->gb, s->xxch_mask_nbits);
                if ((mask & s->xxch_core_mask) != mask) {
                    av_log(s->avctx, AV_LOG_ERROR, "Invalid XXCH downmix channel mapping mask (%#x)\n", mask);
                    return AVERROR_INVALIDDATA;
                }
                s->xxch_dmix_mask[ch] = mask;
            }

            // Downmix coefficients
            coeff_ptr = s->xxch_dmix_coeff;
            for (ch = 0; ch < nchannels; ch++) {
                for (n = 0; n < s->xxch_mask_nbits; n++) {
                    if (s->xxch_dmix_mask[ch] & (1U << n)) {
                        int code = get_bits(&s->gb, 7);
                        int sign = (code >> 6) - 1;
                        if (code &= 63) {
                            index = code * 4 - 3;
                            if (index >= FF_DCA_DMIXTABLE_SIZE) {
                                av_log(s->avctx, AV_LOG_ERROR, "Invalid XXCH downmix coefficient index (%d)\n", index);
                                return AVERROR_INVALIDDATA;
                            }
                            *coeff_ptr++ = (ff_dca_dmixtable[index] ^ sign) - sign;
                        } else {
                            *coeff_ptr = 0;
                        }
                    }
                }
            }
        } else {
            s->xxch_dmix_embedded = 0;
        }

        break;
    }

    // Subband activity count
    for (ch = xch_base; ch < s->nchannels; ch++) {
        s->nsubbands[ch] = get_bits(&s->gb, 5) + 2;
        if (s->nsubbands[ch] > DCA2_SUBBANDS) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid subband activity count\n");
            return AVERROR_INVALIDDATA;
        }
    }

    // High frequency VQ start subband
    for (ch = xch_base; ch < s->nchannels; ch++)
        s->subband_vq_start[ch] = get_bits(&s->gb, 5) + 1;

    // Joint intensity coding index
    for (ch = xch_base; ch < s->nchannels; ch++) {
        if ((n = get_bits(&s->gb, 3)) && header == HEADER_XXCH)
            n += xch_base - 1;
        if (n > s->nchannels) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid joint intensity coding index\n");
            return AVERROR_INVALIDDATA;
        }
        s->joint_intensity_index[ch] = n;
    }

    // Transient mode code book
    for (ch = xch_base; ch < s->nchannels; ch++)
        s->transition_mode_sel[ch] = get_bits(&s->gb, 2);

    // Scale factor code book
    for (ch = xch_base; ch < s->nchannels; ch++) {
        s->scale_factor_sel[ch] = get_bits(&s->gb, 3);
        if (s->scale_factor_sel[ch] == 7) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid scale factor code book\n");
            return AVERROR_INVALIDDATA;
        }
    }

    // Bit allocation quantizer select
    for (ch = xch_base; ch < s->nchannels; ch++) {
        s->bit_allocation_sel[ch] = get_bits(&s->gb, 3);
        if (s->bit_allocation_sel[ch] == 7) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid bit allocation quantizer select\n");
            return AVERROR_INVALIDDATA;
        }
    }

    // Quantization index codebook select
    for (n = 0; n < DCA2_CODE_BOOKS; n++)
        for (ch = xch_base; ch < s->nchannels; ch++)
            s->quant_index_sel[ch][n] = get_bits(&s->gb, ff_dca2_quant_index_sel_nbits[n]);

    // Scale factor adjustment index
    for (n = 0; n < DCA2_CODE_BOOKS; n++)
        for (ch = xch_base; ch < s->nchannels; ch++)
            if (s->quant_index_sel[ch][n] < quant_index_group_size[n])
                s->scale_factor_adj[ch][n] = ff_dca2_scale_factor_adj[get_bits(&s->gb, 2)];

    if (header == HEADER_XXCH) {
        // Reserved
        // Byte align
        // CRC16 of channel set header
        if (ff_dca2_seek_bits(&s->gb, header_pos + header_size * 8)) {
            av_log(s->avctx, AV_LOG_ERROR, "Read past end of XXCH channel set header\n");
            return AVERROR_INVALIDDATA;
        }
    } else {
        // Audio header CRC check word
        if (s->crc_present)
            skip_bits(&s->gb, 16);
    }

    return 0;
}

static inline int parse_scale(DCA2CoreDecoder *s, int *scale_index, int sel)
{
    const uint32_t *scale_table;
    unsigned int scale_size;

    // Select the root square table
    if (sel > 5) {
        scale_table = ff_dca_scale_factor_quant7;
        scale_size = FF_ARRAY_ELEMS(ff_dca_scale_factor_quant7);
    } else {
        scale_table = ff_dca_scale_factor_quant6;
        scale_size = FF_ARRAY_ELEMS(ff_dca_scale_factor_quant6);
    }

    // If Huffman code was used, the difference of scales was encoded
    if (sel < 5)
        *scale_index += get_vlc(&s->gb, &vlc_scale_factor, sel);
    else
        *scale_index = get_bits(&s->gb, sel + 1);

    // Look up scale factor from the root square table
    if ((unsigned int)*scale_index >= scale_size) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid scale factor index\n");
        return AVERROR_INVALIDDATA;
    }

    return scale_table[*scale_index];
}

static inline int parse_joint_scale(DCA2CoreDecoder *s, int sel)
{
    int scale_index;

    if (sel < 5)
        scale_index = get_vlc(&s->gb, &vlc_scale_factor, sel);
    else
        scale_index = get_bits(&s->gb, sel + 1);

    // Bias by 64
    scale_index += 64;

    // Look up joint scale factor
    if ((unsigned int)scale_index >= FF_ARRAY_ELEMS(ff_dca2_joint_scale_factors)) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid joint scale factor index\n");
        return AVERROR_INVALIDDATA;
    }

    return ff_dca2_joint_scale_factors[scale_index];
}

// 5.4.1 - Primary audio coding side information
static int parse_subframe_header(DCA2CoreDecoder *s, int sf,
                                 enum HeaderType header, int xch_base)
{
    int ch, band, ret;

    if (get_bits_left(&s->gb) < 0)
        return AVERROR_INVALIDDATA;

    if (header == HEADER_CORE) {
        // Subsubframe count
        s->nsubsubframes[sf] = get_bits(&s->gb, 2) + 1;

        // Partial subsubframe sample count
        skip_bits(&s->gb, 3);
    }

    // Prediction mode
    for (ch = xch_base; ch < s->nchannels; ch++)
        for (band = 0; band < s->nsubbands[ch]; band++)
            s->prediction_mode[ch][band] = get_bits1(&s->gb);

    // Prediction coefficients VQ address
    for (ch = xch_base; ch < s->nchannels; ch++)
        for (band = 0; band < s->nsubbands[ch]; band++)
            if (s->prediction_mode[ch][band])
                s->prediction_vq_index[ch][band] = get_bits(&s->gb, 12);

    // Bit allocation index
    for (ch = xch_base; ch < s->nchannels; ch++) {
        // Select codebook
        int sel = s->bit_allocation_sel[ch];
        // Not high frequency VQ subbands
        for (band = 0; band < s->subband_vq_start[ch]; band++) {
            int abits;

            if (sel < 5)
                abits = get_vlc(&s->gb, &vlc_bit_allocation, sel);
            else
                abits = get_bits(&s->gb, sel - 1);

            if (abits >= 27) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid bit allocation index\n");
                return AVERROR_INVALIDDATA;
            }

            s->bit_allocation[ch][band] = abits;
        }
    }

    // Transition mode
    for (ch = xch_base; ch < s->nchannels; ch++) {
        // Clear transition mode for all subbands
        memset(s->transition_mode[sf][ch], 0, sizeof(s->transition_mode[0][0]));

        // Transient possible only if more than one subsubframe
        if (s->nsubsubframes[sf] > 1) {
            // Select codebook
            int sel = s->transition_mode_sel[ch];
            // Not high frequency VQ subbands
            for (band = 0; band < s->subband_vq_start[ch]; band++) {
                // Present only if bits allocated
                if (s->bit_allocation[ch][band]) {
                    int trans_ssf = get_vlc(&s->gb, &vlc_transition_mode, sel);
                    if (trans_ssf >= 4) {
                        av_log(s->avctx, AV_LOG_ERROR, "Invalid transition mode index\n");
                        return AVERROR_INVALIDDATA;
                    }
                    s->transition_mode[sf][ch][band] = trans_ssf;
                }
            }
        }
    }

    // Scale factors
    for (ch = xch_base; ch < s->nchannels; ch++) {
        // Select codebook
        int sel = s->scale_factor_sel[ch];

        // Clear accumulation
        int scale_index = 0;

        // Extract scales for subbands up to VQ
        for (band = 0; band < s->subband_vq_start[ch]; band++) {
            if (s->bit_allocation[ch][band]) {
                if ((ret = parse_scale(s, &scale_index, sel)) < 0)
                    return ret;
                s->scale_factors[ch][band][0] = ret;
                if (s->transition_mode[sf][ch][band]) {
                    if ((ret = parse_scale(s, &scale_index, sel)) < 0)
                        return ret;
                    s->scale_factors[ch][band][1] = ret;
                }
            } else {
                s->scale_factors[ch][band][0] = 0;
            }
        }

        // High frequency VQ subbands
        for (band = s->subband_vq_start[ch]; band < s->nsubbands[ch]; band++) {
            if ((ret = parse_scale(s, &scale_index, sel)) < 0)
                return ret;
            s->scale_factors[ch][band][0] = ret;
        }
    }

    // Joint subband codebook select
    for (ch = xch_base; ch < s->nchannels; ch++) {
        // Only if joint subband coding is enabled
        if (s->joint_intensity_index[ch]) {
            s->joint_scale_sel[ch] = get_bits(&s->gb, 3);
            if (s->joint_scale_sel[ch] == 7) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid joint scale factor code book\n");
                return AVERROR_INVALIDDATA;
            }
        }
    }

    // Scale factors for joint subband coding
    for (ch = xch_base; ch < s->nchannels; ch++) {
        // Only if joint subband coding is enabled
        if (s->joint_intensity_index[ch]) {
            // Select codebook
            int sel = s->joint_scale_sel[ch];
            // Get source channel
            int src_ch = s->joint_intensity_index[ch] - 1;
            for (band = s->nsubbands[ch]; band < s->nsubbands[src_ch]; band++) {
                if ((ret = parse_joint_scale(s, sel)) < 0)
                    return ret;
                s->joint_scale_factors[ch][band] = ret;
            }
        }
    }

    // Dynamic range coefficient
    if (s->drc_present && header == HEADER_CORE)
        skip_bits(&s->gb, 8);

    // Side information CRC check word
    if (s->crc_present)
        skip_bits(&s->gb, 16);

    return 0;
}

// Extract block code indices from the bit stream
static inline int parse_block_codes(DCA2CoreDecoder *s, int *audio, int abits)
{
    int code1 = get_bits(&s->gb, ff_dca2_block_code_nbits[abits - 1]);
    int code2 = get_bits(&s->gb, ff_dca2_block_code_nbits[abits - 1]);
    int levels = ff_dca2_quant_levels[abits];
    int offset = (levels - 1) / 2;
    int n, div;

    // Look up samples from the block code book
    for (n = 0; n < DCA2_SUBBAND_SAMPLES / 2; n++) {
        div = FASTDIV(code1, levels);
        audio[n] = code1 - div * levels - offset;
        code1 = div;
    }
    for (; n < DCA2_SUBBAND_SAMPLES; n++) {
        div = FASTDIV(code2, levels);
        audio[n] = code2 - div * levels - offset;
        code2 = div;
    }

    if (code1 || code2) {
        av_log(s->avctx, AV_LOG_ERROR, "Failed to decode block code(s)\n");
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

// Extract Huffman codes from the bit stream
static inline int parse_huffman_codes(DCA2CoreDecoder *s, int *audio, int abits, int sel)
{
    int i;

    for (i = 0; i < DCA2_SUBBAND_SAMPLES; i++)
        audio[i] = get_vlc(&s->gb, &vlc_quant_index[abits - 1], sel);

    return 1;
}

static inline int extract_audio(DCA2CoreDecoder *s, int *audio, int abits, int ch)
{
    av_assert0(abits >= 0 && abits < 27);

    if (abits == 0) {
        // No bits allocated
        memset(audio, 0, DCA2_SUBBAND_SAMPLES * sizeof(*audio));
        return 0;
    }

    if (abits <= DCA2_CODE_BOOKS) {
        int sel = s->quant_index_sel[ch][abits - 1];
        if (sel < quant_index_group_size[abits - 1]) {
            // Huffman codes
            return parse_huffman_codes(s, audio, abits, sel);
        }
        if (abits <= 7) {
            // Block codes
            return parse_block_codes(s, audio, abits);
        }
    }

    // No further encoding
    get_array(&s->gb, audio, DCA2_SUBBAND_SAMPLES, abits - 3);
    return 0;
}

static inline void dequantize(int *output, const int *input, int step_size,
                              int scale, int residual)
{
    // Account for quantizer step size
    int64_t step_scale = (int64_t)step_size * scale;
    int n, shift = 0;

    // Limit scale factor resolution to 22 bits
    if (step_scale > (1 << 23)) {
        shift = av_log2(step_scale >> 23) + 1;
        step_scale >>= shift;
    }

    // Scale the samples
    if (residual) {
        for (n = 0; n < DCA2_SUBBAND_SAMPLES; n++)
            output[n] += clip23(norm__(input[n] * step_scale, 22 - shift));
    } else {
        for (n = 0; n < DCA2_SUBBAND_SAMPLES; n++)
            output[n]  = clip23(norm__(input[n] * step_scale, 22 - shift));
    }
}

// 5.5 - Primary audio data arrays
static int parse_subframe_audio(DCA2CoreDecoder *s, int sf, enum HeaderType header,
                                int xch_base, int *sub_pos, int *lfe_pos)
{
    int audio[16], *samples;
    int m, n, ssf, ofs, ch, band, scale;

    // Number of subband samples in this subframe
    int nsamples = s->nsubsubframes[sf] * DCA2_SUBBAND_SAMPLES;
    if (*sub_pos + nsamples > s->npcmblocks) {
        av_log(s->avctx, AV_LOG_ERROR, "Subband sample buffer overflow\n");
        return AVERROR_INVALIDDATA;
    }

    if (get_bits_left(&s->gb) < 0)
        return AVERROR_INVALIDDATA;

    // VQ encoded subbands
    for (ch = xch_base; ch < s->nchannels; ch++) {
        for (band = s->subband_vq_start[ch]; band < s->nsubbands[ch]; band++) {
            // Extract the VQ address from the bit stream
            int vq_index = get_bits(&s->gb, 10);

            // Look up the VQ code book for 32 subband samples
            const int8_t *vq_samples = ff_dca_high_freq_vq[vq_index];

            // Get the scale factor
            scale = s->scale_factors[ch][band][0];

            // Scale and take the samples
            samples = s->subband_samples[ch][band] + *sub_pos;
            for (n = 0; n < nsamples; n++)
                samples[n] = clip23(mul4(scale, vq_samples[n]));
        }
    }

    // Low frequency effect data
    if (s->lfe_present && header == HEADER_CORE) {
        unsigned int index;

        // Number of LFE samples in this subframe
        int nlfesamples = 2 * s->lfe_present * s->nsubsubframes[sf];

        // Extract LFE samples from the bit stream
        get_array(&s->gb, audio, nlfesamples, 8);

        // Extract scale factor index from the bit stream
        index = get_bits(&s->gb, 8);
        if (index >= FF_ARRAY_ELEMS(ff_dca_scale_factor_quant7)) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid LFE scale factor index\n");
            return AVERROR_INVALIDDATA;
        }

        // Look up the 7-bit root square quantization table
        scale = ff_dca_scale_factor_quant7[index];

        // Account for quantizer step size which is 0.035
        scale = mul23(4697620 /* 0.035 * (1 << 27) */, scale);

        // Scale the LFE samples
        for (n = 0; n < nlfesamples; n++)
            s->lfe_samples[*lfe_pos + n] = clip23((audio[n] * scale) >> 4);

        // Advance LFE sample pointer for the next subframe
        *lfe_pos += nlfesamples;
    }

    // Audio data
    for (ssf = 0, ofs = *sub_pos; ssf < s->nsubsubframes[sf]; ssf++) {
        for (ch = xch_base; ch < s->nchannels; ch++) {
            if (get_bits_left(&s->gb) < 0)
                return AVERROR_INVALIDDATA;

            // Not high frequency VQ subbands
            for (band = 0; band < s->subband_vq_start[ch]; band++) {
                int abits = s->bit_allocation[ch][band];
                int ret, step_size, trans_ssf;

                // Extract bits from the bit stream
                if ((ret = extract_audio(s, audio, abits, ch)) < 0)
                    return ret;

                // Select quantization step size table
                // Look up quantization step size
                if (s->bit_rate == 3)
                    step_size = ff_dca_lossless_quant[abits];
                else
                    step_size = ff_dca_lossy_quant[abits];

                // Identify transient location
                trans_ssf = s->transition_mode[sf][ch][band];

                // Determine proper scale factor
                if (trans_ssf == 0 || ssf < trans_ssf)
                    scale = s->scale_factors[ch][band][0];
                else
                    scale = s->scale_factors[ch][band][1];

                // Adjustment of scale factor
                // Only when SEL indicates Huffman code
                if (ret > 0) {
                    int64_t adj = s->scale_factor_adj[ch][abits - 1];
                    scale = clip23((adj * scale) >> 22);
                }

                dequantize(s->subband_samples[ch][band] + ofs,
                           audio, step_size, scale, 0);
            }
        }

        // DSYNC
        if ((ssf == s->nsubsubframes[sf] - 1 || s->sync_ssf) && get_bits(&s->gb, 16) != 0xffff) {
            av_log(s->avctx, AV_LOG_ERROR, "DSYNC check failed\n");
            return AVERROR_INVALIDDATA;
        }

        ofs += DCA2_SUBBAND_SAMPLES;
    }

    // Inverse ADPCM
    for (ch = xch_base; ch < s->nchannels; ch++) {
        for (band = 0; band < s->nsubbands[ch]; band++) {
            // Only if prediction mode is on
            if (s->prediction_mode[ch][band]) {
                // Extract the VQ index
                int vq_index = s->prediction_vq_index[ch][band];

                // Look up the VQ table for prediction coefficients
                const int16_t *vq_coeffs = ff_dca_adpcm_vb[vq_index];

                samples = s->subband_samples[ch][band] + *sub_pos;
                for (m = 0; m < nsamples; m++) {
                    int64_t err = INT64_C(0);
                    for (n = 0; n < DCA2_ADPCM_COEFFS; n++)
                        err += (int64_t)samples[m - n - 1] * vq_coeffs[n];
                    samples[m] = clip23(samples[m] + clip23(norm13(err)));
                }
            }
        }
    }

    // Joint subband coding
    for (ch = xch_base; ch < s->nchannels; ch++) {
        // Only if joint subband coding is enabled
        if (s->joint_intensity_index[ch]) {
            // Get source channel
            int src_ch = s->joint_intensity_index[ch] - 1;
            for (band = s->nsubbands[ch]; band < s->nsubbands[src_ch]; band++) {
                int *src = s->subband_samples[src_ch][band] + *sub_pos;
                int *dst = s->subband_samples[    ch][band] + *sub_pos;
                int scale = s->joint_scale_factors[ch][band];
                for (n = 0; n < nsamples; n++)
                    dst[n] = clip23(mul17(src[n], scale));
            }
        }
    }

    // Advance subband sample pointer for the next subframe
    *sub_pos += nsamples;
    return 0;
}

static void erase_adpcm_history(DCA2CoreDecoder *s)
{
    int ch, band;

    // Erase ADPCM history from previous frame if
    // predictor history switch was disabled
    for (ch = 0; ch < DCA2_CHANNELS; ch++)
        for (band = 0; band < DCA2_SUBBANDS; band++)
            AV_ZERO128(s->subband_samples[ch][band] - DCA2_ADPCM_COEFFS);
}

static int alloc_sample_buffer(DCA2CoreDecoder *s)
{
    int nchsamples = DCA2_ADPCM_COEFFS + s->npcmblocks;
    int nframesamples = nchsamples * DCA2_CHANNELS * DCA2_SUBBANDS;
    int nlfesamples = DCA2_LFE_HISTORY + s->npcmblocks / 2;
    unsigned int size = s->subband_size;
    int ch, band;

    // Reallocate subband sample buffer
    av_fast_malloc(&s->subband_buffer, &s->subband_size,
                   (nframesamples + nlfesamples) * sizeof(int));
    if (!s->subband_buffer)
        return AVERROR(ENOMEM);

    if (size != s->subband_size) {
        memset(s->subband_buffer, 0, s->subband_size);
        for (ch = 0; ch < DCA2_CHANNELS; ch++)
            for (band = 0; band < DCA2_SUBBANDS; band++)
                s->subband_samples[ch][band] = s->subband_buffer +
                    (ch * DCA2_SUBBANDS + band) * nchsamples + DCA2_ADPCM_COEFFS;
        s->lfe_samples = s->subband_buffer + nframesamples;
    }

    if (!s->predictor_history)
        erase_adpcm_history(s);

    return 0;
}

static int parse_frame_data(DCA2CoreDecoder *s, enum HeaderType header, int xch_base)
{
    int sf, ch, ret, band, sub_pos, lfe_pos;

    if ((ret = parse_coding_header(s, header, xch_base)) < 0)
        return ret;

    for (sf = 0, sub_pos = 0, lfe_pos = DCA2_LFE_HISTORY; sf < s->nsubframes; sf++) {
        if ((ret = parse_subframe_header(s, sf, header, xch_base)) < 0)
            return ret;
        if ((ret = parse_subframe_audio(s, sf, header, xch_base, &sub_pos, &lfe_pos)) < 0)
            return ret;
    }

    for (ch = xch_base; ch < s->nchannels; ch++) {
        // Number of active subbands for this channel
        int nsubbands = s->nsubbands[ch];
        if (s->joint_intensity_index[ch])
            nsubbands = FFMAX(nsubbands, s->nsubbands[s->joint_intensity_index[ch] - 1]);

        // Update history for ADPCM
        for (band = 0; band < nsubbands; band++) {
            int *samples = s->subband_samples[ch][band] - DCA2_ADPCM_COEFFS;
            AV_COPY128(samples, samples + s->npcmblocks);
        }

        // Clear inactive subbands
        for (; band < DCA2_SUBBANDS; band++) {
            int *samples = s->subband_samples[ch][band] - DCA2_ADPCM_COEFFS;
            memset(samples, 0, (DCA2_ADPCM_COEFFS + s->npcmblocks) * sizeof(int));
        }
    }

    return 0;
}

static int parse_xch_frame(DCA2CoreDecoder *s)
{
    int ret;

    if (s->ch_mask & DCA2_SPEAKER_MASK_Cs) {
        av_log(s->avctx, AV_LOG_ERROR, "XCH with Cs speaker already present\n");
        return AVERROR_INVALIDDATA;
    }

    if ((ret = parse_frame_data(s, HEADER_XCH, s->nchannels)) < 0)
        return ret;

    // Seek to the end of core frame, don't trust XCH frame size
    if (ff_dca2_seek_bits(&s->gb, s->frame_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Read past end of XCH frame\n");
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

static int parse_xxch_frame(DCA2CoreDecoder *s)
{
    int xxch_nchsets, xxch_frame_size;
    int ret, mask, header_size, header_pos = get_bits_count(&s->gb);

    // XXCH sync word
    if (get_bits_long(&s->gb, 32) != DCA_SYNCWORD_XXCH) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid XXCH sync word\n");
        return AVERROR_INVALIDDATA;
    }

    // XXCH frame header length
    header_size = get_bits(&s->gb, 6) + 1;

    // Check XXCH frame header CRC
    if (ff_dca2_check_crc(&s->gb, header_pos + 32, header_pos + header_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid XXCH frame header checksum\n");
        return AVERROR_INVALIDDATA;
    }

    // CRC presence flag for channel set header
    s->xxch_crc_present = get_bits1(&s->gb);

    // Number of bits for loudspeaker mask
    s->xxch_mask_nbits = get_bits(&s->gb, 5) + 1;
    if (s->xxch_mask_nbits <= DCA2_SPEAKER_Cs) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid number of bits for XXCH speaker mask (%d)\n", s->xxch_mask_nbits);
        return AVERROR_INVALIDDATA;
    }

    // Number of channel sets
    xxch_nchsets = get_bits(&s->gb, 2) + 1;
    if (xxch_nchsets > 1) {
        av_log(s->avctx, AV_LOG_WARNING, "Unsupported number of XXCH channel sets (%d)\n", xxch_nchsets);
        return AVERROR_PATCHWELCOME;
    }

    // Channel set 0 data byte size
    xxch_frame_size = get_bits(&s->gb, 14) + 1;

    // Core loudspeaker activity mask
    s->xxch_core_mask = get_bits_long(&s->gb, s->xxch_mask_nbits);

    // Validate the core mask
    mask = s->ch_mask;

    if ((mask & DCA2_SPEAKER_MASK_Ls) && (s->xxch_core_mask & DCA2_SPEAKER_MASK_Lss))
        mask = (mask & ~DCA2_SPEAKER_MASK_Ls) | DCA2_SPEAKER_MASK_Lss;

    if ((mask & DCA2_SPEAKER_MASK_Rs) && (s->xxch_core_mask & DCA2_SPEAKER_MASK_Rss))
        mask = (mask & ~DCA2_SPEAKER_MASK_Rs) | DCA2_SPEAKER_MASK_Rss;

    if (mask != s->xxch_core_mask) {
        av_log(s->avctx, AV_LOG_ERROR, "XXCH core speaker activity mask (%#x) disagrees with core (%#x)\n", s->xxch_core_mask, mask);
        return AVERROR_INVALIDDATA;
    }

    // Reserved
    // Byte align
    // CRC16 of XXCH frame header
    if (ff_dca2_seek_bits(&s->gb, header_pos + header_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Read past end of XXCH frame header\n");
        return AVERROR_INVALIDDATA;
    }

    // Parse XXCH channel set 0
    if ((ret = parse_frame_data(s, HEADER_XXCH, s->nchannels)) < 0)
        return ret;

    if (ff_dca2_seek_bits(&s->gb, header_pos + header_size * 8 + xxch_frame_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Read past end of XXCH channel set\n");
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

static int parse_xbr_subframe(DCA2CoreDecoder *s, int xbr_base_ch, int xbr_nchannels,
                              int *xbr_nsubbands, int xbr_transition_mode, int sf, int *sub_pos)
{
    int     xbr_nabits[DCA2_CHANNELS];
    int     xbr_bit_allocation[DCA2_CHANNELS][DCA2_SUBBANDS];
    int     xbr_scale_nbits[DCA2_CHANNELS];
    int     xbr_scale_factors[DCA2_CHANNELS][DCA2_SUBBANDS][2];
    int     ssf, ch, band, ofs;

    // Number of subband samples in this subframe
    int nsamples = s->nsubsubframes[sf] * DCA2_SUBBAND_SAMPLES;
    if (*sub_pos + nsamples > s->npcmblocks) {
        av_log(s->avctx, AV_LOG_ERROR, "Subband sample buffer overflow\n");
        return AVERROR_INVALIDDATA;
    }

    if (get_bits_left(&s->gb) < 0)
        return AVERROR_INVALIDDATA;

    // Number of bits for XBR bit allocation index
    for (ch = xbr_base_ch; ch < xbr_nchannels; ch++)
        xbr_nabits[ch] = get_bits(&s->gb, 2) + 2;

    // XBR bit allocation index
    for (ch = xbr_base_ch; ch < xbr_nchannels; ch++)
        for (band = 0; band < xbr_nsubbands[ch]; band++)
            xbr_bit_allocation[ch][band] = get_bits(&s->gb, xbr_nabits[ch]);

    // Number of bits for scale indices
    for (ch = xbr_base_ch; ch < xbr_nchannels; ch++) {
        xbr_scale_nbits[ch] = get_bits(&s->gb, 3);
        if (!xbr_scale_nbits[ch]) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid number of bits for XBR scale factor index\n");
            return AVERROR_INVALIDDATA;
        }
    }

    // XBR scale factors
    for (ch = xbr_base_ch; ch < xbr_nchannels; ch++) {
        const uint32_t *scale_table;
        int scale_size;

        // Select the root square table
        if (s->scale_factor_sel[ch] > 5) {
            scale_table = ff_dca_scale_factor_quant7;
            scale_size = FF_ARRAY_ELEMS(ff_dca_scale_factor_quant7);
        } else {
            scale_table = ff_dca_scale_factor_quant6;
            scale_size = FF_ARRAY_ELEMS(ff_dca_scale_factor_quant6);
        }

        // Parse scale factor indices
        // Look up scale factors from the root square table
        for (band = 0; band < xbr_nsubbands[ch]; band++) {
            if (xbr_bit_allocation[ch][band] > 0) {
                int scale_index = get_bits(&s->gb, xbr_scale_nbits[ch]);
                if (scale_index >= scale_size) {
                    av_log(s->avctx, AV_LOG_ERROR, "Invalid XBR scale factor index\n");
                    return AVERROR_INVALIDDATA;
                }
                xbr_scale_factors[ch][band][0] = scale_table[scale_index];
                if (xbr_transition_mode && s->transition_mode[sf][ch][band]) {
                    scale_index = get_bits(&s->gb, xbr_scale_nbits[ch]);
                    if (scale_index >= scale_size) {
                        av_log(s->avctx, AV_LOG_ERROR, "Invalid XBR scale factor index\n");
                        return AVERROR_INVALIDDATA;
                    }
                    xbr_scale_factors[ch][band][1] = scale_table[scale_index];
                }
            }
        }
    }

    // Audio data
    for (ssf = 0, ofs = *sub_pos; ssf < s->nsubsubframes[sf]; ssf++) {
        for (ch = xbr_base_ch; ch < xbr_nchannels; ch++) {
            if (get_bits_left(&s->gb) < 0)
                return AVERROR_INVALIDDATA;

            for (band = 0; band < xbr_nsubbands[ch]; band++) {
                int abits = xbr_bit_allocation[ch][band];
                int audio[DCA2_SUBBAND_SAMPLES];
                int ret, step_size, trans_ssf, scale;

                // Extract bits from the bit stream
                if (abits > 7) {
                    // No further encoding
                    get_array(&s->gb, audio, DCA2_SUBBAND_SAMPLES, abits - 3);
                } else if (abits > 0) {
                    // Block codes
                    if ((ret = parse_block_codes(s, audio, abits)) < 0)
                        return ret;
                } else {
                    // No bits allocated
                    continue;
                }

                // Look up quantization step size
                step_size = ff_dca_lossless_quant[abits];

                // Identify transient location
                if (xbr_transition_mode)
                    trans_ssf = s->transition_mode[sf][ch][band];
                else
                    trans_ssf = 0;

                // Determine proper scale factor
                if (trans_ssf == 0 || ssf < trans_ssf)
                    scale = xbr_scale_factors[ch][band][0];
                else
                    scale = xbr_scale_factors[ch][band][1];

                dequantize(s->subband_samples[ch][band] + ofs,
                           audio, step_size, scale, 1);
            }
        }

        // DSYNC
        if ((ssf == s->nsubsubframes[sf] - 1 || s->sync_ssf) && get_bits(&s->gb, 16) != 0xffff) {
            av_log(s->avctx, AV_LOG_ERROR, "XBR-DSYNC check failed\n");
            return AVERROR_INVALIDDATA;
        }

        ofs += DCA2_SUBBAND_SAMPLES;
    }

    // Advance subband sample pointer for the next subframe
    *sub_pos += nsamples;
    return 0;
}

static int parse_xbr_frame(DCA2CoreDecoder *s)
{
    int     xbr_frame_size[DCA2_EXSS_CHSETS_MAX];
    int     xbr_nchannels[DCA2_EXSS_CHSETS_MAX];
    int     xbr_nsubbands[DCA2_EXSS_CHSETS_MAX * DCA2_EXSS_CHANNELS_MAX];
    int     xbr_nchsets, xbr_transition_mode, xbr_band_nbits, xbr_base_ch;
    int     i, ch1, ch2, ret, header_size, header_pos = get_bits_count(&s->gb);

    // XBR sync word
    if (get_bits_long(&s->gb, 32) != DCA_SYNCWORD_XBR) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid XBR sync word\n");
        return AVERROR_INVALIDDATA;
    }

    // XBR frame header length
    header_size = get_bits(&s->gb, 6) + 1;

    // Check XBR frame header CRC
    if (ff_dca2_check_crc(&s->gb, header_pos + 32, header_pos + header_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid XBR frame header checksum\n");
        return AVERROR_INVALIDDATA;
    }

    // Number of channel sets
    xbr_nchsets = get_bits(&s->gb, 2) + 1;

    // Channel set data byte size
    for (i = 0; i < xbr_nchsets; i++)
        xbr_frame_size[i] = get_bits(&s->gb, 14) + 1;

    // Transition mode flag
    xbr_transition_mode = get_bits1(&s->gb);

    // Channel set headers
    for (i = 0, ch2 = 0; i < xbr_nchsets; i++) {
        xbr_nchannels[i] = get_bits(&s->gb, 3) + 1;
        xbr_band_nbits = get_bits(&s->gb, 2) + 5;
        for (ch1 = 0; ch1 < xbr_nchannels[i]; ch1++, ch2++) {
            xbr_nsubbands[ch2] = get_bits(&s->gb, xbr_band_nbits) + 1;
            if (xbr_nsubbands[ch2] > DCA2_SUBBANDS) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid number of active XBR subbands (%d)\n", xbr_nsubbands[ch2]);
                return AVERROR_INVALIDDATA;
            }
        }
    }

    // Reserved
    // Byte align
    // CRC16 of XBR frame header
    if (ff_dca2_seek_bits(&s->gb, header_pos + header_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Read past end of XBR frame header\n");
        return AVERROR_INVALIDDATA;
    }

    // Channel set data
    for (i = 0, xbr_base_ch = 0; i < xbr_nchsets; i++) {
        header_pos = get_bits_count(&s->gb);

        if (xbr_base_ch + xbr_nchannels[i] <= s->nchannels) {
            int sf, sub_pos;

            for (sf = 0, sub_pos = 0; sf < s->nsubframes; sf++) {
                if ((ret = parse_xbr_subframe(s, xbr_base_ch,
                                              xbr_base_ch + xbr_nchannels[i],
                                              xbr_nsubbands, xbr_transition_mode,
                                              sf, &sub_pos)) < 0)
                    return ret;
            }
        }

        xbr_base_ch += xbr_nchannels[i];

        if (ff_dca2_seek_bits(&s->gb, header_pos + xbr_frame_size[i] * 8)) {
            av_log(s->avctx, AV_LOG_ERROR, "Read past end of XBR channel set\n");
            return AVERROR_INVALIDDATA;
        }
    }

    return 0;
}

// Modified ISO/IEC 9899 linear congruential generator
// Returns pseudorandom integer in range [-2^30, 2^30 - 1]
static int rand_x96(DCA2CoreDecoder *s)
{
    s->x96_rand = 1103515245U * s->x96_rand + 12345U;
    return (s->x96_rand & 0x7fffffff) - 0x40000000;
}

static int parse_x96_subframe_audio(DCA2CoreDecoder *s, int sf, int xch_base, int *sub_pos)
{
    int m, n, ssf, ch, band, ofs;

    // Number of subband samples in this subframe
    int nsamples = s->nsubsubframes[sf] * DCA2_SUBBAND_SAMPLES;
    if (*sub_pos + nsamples > s->npcmblocks) {
        av_log(s->avctx, AV_LOG_ERROR, "Subband sample buffer overflow\n");
        return AVERROR_INVALIDDATA;
    }

    if (get_bits_left(&s->gb) < 0)
        return AVERROR_INVALIDDATA;

    // VQ encoded or unallocated subbands
    for (ch = xch_base; ch < s->x96_nchannels; ch++) {
        for (band = s->x96_subband_start; band < s->nsubbands[ch]; band++) {
            // Get the sample pointer
            int *samples = s->x96_subband_samples[ch][band] + *sub_pos;

            // Get the scale factor
            int scale = s->scale_factors[ch][band >> 1][band & 1];

            int abits = s->bit_allocation[ch][band];
            if (abits == 0) {   // No bits allocated for subband
                if (scale <= 1) {
                    memset(samples, 0, nsamples * sizeof(int));
                } else {
                    // Generate scaled random samples as required by specification
                    for (n = 0; n < nsamples; n++)
                        samples[n] = mul31(rand_x96(s), scale);
                }
            } else if (abits == 1) {    // VQ encoded subband
                for (ssf = 0; ssf < (s->nsubsubframes[sf] + 1) / 2; ssf++) {
                    // Extract the VQ address from the bit stream
                    int vq_index = get_bits(&s->gb, 10);

                    // Look up the VQ code book for up to 16 subband samples
                    const int8_t *vq_samples = ff_dca_high_freq_vq[vq_index];

                    // Number of VQ samples to look up
                    int vq_nsamples = FFMIN(nsamples - ssf * 16, 16);

                    // Scale and take the samples
                    for (n = 0; n < vq_nsamples; n++)
                        *samples++ = clip23(mul4(scale, vq_samples[n]));
                }
            }
        }
    }

    // Audio data
    for (ssf = 0, ofs = *sub_pos; ssf < s->nsubsubframes[sf]; ssf++) {
        for (ch = xch_base; ch < s->x96_nchannels; ch++) {
            if (get_bits_left(&s->gb) < 0)
                return AVERROR_INVALIDDATA;

            for (band = s->x96_subband_start; band < s->nsubbands[ch]; band++) {
                int abits = s->bit_allocation[ch][band] - 1;
                int audio[DCA2_SUBBAND_SAMPLES];
                int ret, step_size, scale;

                // Not VQ encoded or unallocated subbands
                if (abits < 1)
                    continue;

                // Extract bits from the bit stream
                if ((ret = extract_audio(s, audio, abits, ch)) < 0)
                    return ret;

                // Select quantization step size table
                // Look up quantization step size
                if (s->bit_rate == 3)
                    step_size = ff_dca_lossless_quant[abits];
                else
                    step_size = ff_dca_lossy_quant[abits];

                // Determine proper scale factor
                scale = s->scale_factors[ch][band >> 1][band & 1];

                dequantize(s->x96_subband_samples[ch][band] + ofs,
                           audio, step_size, scale, 0);
            }
        }

        // DSYNC
        if ((ssf == s->nsubsubframes[sf] - 1 || s->sync_ssf) && get_bits(&s->gb, 16) != 0xffff) {
            av_log(s->avctx, AV_LOG_ERROR, "X96-DSYNC check failed\n");
            return AVERROR_INVALIDDATA;
        }

        ofs += DCA2_SUBBAND_SAMPLES;
    }

    // Inverse ADPCM
    for (ch = xch_base; ch < s->x96_nchannels; ch++) {
        for (band = s->x96_subband_start; band < s->nsubbands[ch]; band++) {
            // Only if prediction mode is on
            if (s->prediction_mode[ch][band]) {
                int *samples = s->x96_subband_samples[ch][band] + *sub_pos;

                // Extract the VQ index
                int vq_index = s->prediction_vq_index[ch][band];

                // Look up the VQ table for prediction coefficients
                const int16_t *vq_coeffs = ff_dca_adpcm_vb[vq_index];
                for (m = 0; m < nsamples; m++) {
                    int64_t err = INT64_C(0);
                    for (n = 0; n < DCA2_ADPCM_COEFFS; n++)
                        err += (int64_t)samples[m - n - 1] * vq_coeffs[n];
                    samples[m] = clip23(samples[m] + clip23(norm13(err)));
                }
            }
        }
    }

    // Joint subband coding
    for (ch = xch_base; ch < s->x96_nchannels; ch++) {
        // Only if joint subband coding is enabled
        if (s->joint_intensity_index[ch]) {
            // Get source channel
            int src_ch = s->joint_intensity_index[ch] - 1;
            for (band = s->nsubbands[ch]; band < s->nsubbands[src_ch]; band++) {
                int *src = s->x96_subband_samples[src_ch][band] + *sub_pos;
                int *dst = s->x96_subband_samples[    ch][band] + *sub_pos;
                int scale = s->joint_scale_factors[ch][band];
                for (n = 0; n < nsamples; n++)
                    dst[n] = clip23(mul17(src[n], scale));
            }
        }
    }

    // Advance subband sample pointer for the next subframe
    *sub_pos += nsamples;
    return 0;
}

static void erase_x96_adpcm_history(DCA2CoreDecoder *s)
{
    int ch, band;

    // Erase ADPCM history from previous frame if
    // predictor history switch was disabled
    for (ch = 0; ch < DCA2_CHANNELS; ch++)
        for (band = 0; band < DCA2_SUBBANDS_X96; band++)
            AV_ZERO128(s->x96_subband_samples[ch][band] - DCA2_ADPCM_COEFFS);
}

static int alloc_x96_sample_buffer(DCA2CoreDecoder *s)
{
    int nchsamples = DCA2_ADPCM_COEFFS + s->npcmblocks;
    int nframesamples = nchsamples * DCA2_CHANNELS * DCA2_SUBBANDS_X96;
    unsigned int size = s->x96_subband_size;
    int ch, band;

    // Reallocate subband sample buffer
    av_fast_malloc(&s->x96_subband_buffer, &s->x96_subband_size,
                   nframesamples * sizeof(int));
    if (!s->x96_subband_buffer)
        return AVERROR(ENOMEM);

    if (size != s->x96_subband_size) {
        memset(s->x96_subband_buffer, 0, s->x96_subband_size);
        for (ch = 0; ch < DCA2_CHANNELS; ch++)
            for (band = 0; band < DCA2_SUBBANDS_X96; band++)
                s->x96_subband_samples[ch][band] = s->x96_subband_buffer +
                    (ch * DCA2_SUBBANDS_X96 + band) * nchsamples + DCA2_ADPCM_COEFFS;
    }

    if (!s->predictor_history)
        erase_x96_adpcm_history(s);

    return 0;
}

static int parse_x96_subframe_header(DCA2CoreDecoder *s, int xch_base)
{
    int ch, band, ret;

    if (get_bits_left(&s->gb) < 0)
        return AVERROR_INVALIDDATA;

    // Prediction mode
    for (ch = xch_base; ch < s->x96_nchannels; ch++)
        for (band = s->x96_subband_start; band < s->nsubbands[ch]; band++)
            s->prediction_mode[ch][band] = get_bits1(&s->gb);

    // Prediction coefficients VQ address
    for (ch = xch_base; ch < s->x96_nchannels; ch++)
        for (band = s->x96_subband_start; band < s->nsubbands[ch]; band++)
            if (s->prediction_mode[ch][band])
                s->prediction_vq_index[ch][band] = get_bits(&s->gb, 12);

    // Bit allocation index
    for (ch = xch_base; ch < s->x96_nchannels; ch++) {
        // Select codebook
        int sel = s->bit_allocation_sel[ch];

        // Clear accumulation
        int abits = 0;

        for (band = s->x96_subband_start; band < s->nsubbands[ch]; band++) {
            // If Huffman code was used, the difference of abits was encoded
            if (sel < 7)
                abits += get_vlc(&s->gb, &vlc_quant_index[5 + 2 * s->x96_high_res], sel);
            else
                abits = get_bits(&s->gb, 3 + s->x96_high_res);

            if (abits < 0 || abits > 7 + 8 * s->x96_high_res) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid X96 bit allocation index\n");
                return AVERROR_INVALIDDATA;
            }

            s->bit_allocation[ch][band] = abits;
        }
    }

    // Scale factors
    for (ch = xch_base; ch < s->x96_nchannels; ch++) {
        // Select codebook
        int sel = s->scale_factor_sel[ch];

        // Clear accumulation
        int scale_index = 0;

        // Extract scales for subbands
        // Transmitted even for unallocated subbands
        for (band = s->x96_subband_start; band < s->nsubbands[ch]; band++) {
            if ((ret = parse_scale(s, &scale_index, sel)) < 0)
                return ret;
            s->scale_factors[ch][band >> 1][band & 1] = ret;
        }
    }

    // Joint subband codebook select
    for (ch = xch_base; ch < s->x96_nchannels; ch++) {
        // Only if joint subband coding is enabled
        if (s->joint_intensity_index[ch]) {
            s->joint_scale_sel[ch] = get_bits(&s->gb, 3);
            if (s->joint_scale_sel[ch] == 7) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid X96 joint scale factor code book\n");
                return AVERROR_INVALIDDATA;
            }
        }
    }

    // Scale factors for joint subband coding
    for (ch = xch_base; ch < s->x96_nchannels; ch++) {
        // Only if joint subband coding is enabled
        if (s->joint_intensity_index[ch]) {
            // Select codebook
            int sel = s->joint_scale_sel[ch];
            // Get source channel
            int src_ch = s->joint_intensity_index[ch] - 1;
            for (band = s->nsubbands[ch]; band < s->nsubbands[src_ch]; band++) {
                if ((ret = parse_joint_scale(s, sel)) < 0)
                    return ret;
                s->joint_scale_factors[ch][band] = ret;
            }
        }
    }

    // Side information CRC check word
    if (s->crc_present)
        skip_bits(&s->gb, 16);

    return 0;
}

static int parse_x96_coding_header(DCA2CoreDecoder *s, int exss, int xch_base)
{
    int n, ch, header_size = 0, header_pos = get_bits_count(&s->gb);

    if (get_bits_left(&s->gb) < 0)
        return AVERROR_INVALIDDATA;

    if (exss) {
        // Channel set header length
        header_size = get_bits(&s->gb, 7) + 1;

        // Check CRC
        if (s->x96_crc_present && ff_dca2_check_crc(&s->gb, header_pos, header_pos + header_size * 8)) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid X96 channel set header checksum\n");
            return AVERROR_INVALIDDATA;
        }
    }

    // High resolution flag
    s->x96_high_res = get_bits1(&s->gb);

    // First encoded subband
    if (s->x96_rev_no < 8) {
        s->x96_subband_start = get_bits(&s->gb, 5);
        if (s->x96_subband_start > 27) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid X96 subband start index (%d)\n", s->x96_subband_start);
            return AVERROR_INVALIDDATA;
        }
    } else {
        s->x96_subband_start = DCA2_SUBBANDS;
    }

    // Subband activity count
    for (ch = xch_base; ch < s->x96_nchannels; ch++) {
        s->nsubbands[ch] = get_bits(&s->gb, 6) + 1;
        if (s->nsubbands[ch] < DCA2_SUBBANDS) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid X96 subband activity count (%d)\n", s->nsubbands[ch]);
            return AVERROR_INVALIDDATA;
        }
    }

    // Joint intensity coding index
    for (ch = xch_base; ch < s->x96_nchannels; ch++) {
        if ((n = get_bits(&s->gb, 3)) && xch_base)
            n += xch_base - 1;
        if (n > s->x96_nchannels) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid X96 joint intensity coding index\n");
            return AVERROR_INVALIDDATA;
        }
        s->joint_intensity_index[ch] = n;
    }

    // Scale factor code book
    for (ch = xch_base; ch < s->x96_nchannels; ch++) {
        s->scale_factor_sel[ch] = get_bits(&s->gb, 3);
        if (s->scale_factor_sel[ch] >= 6) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid X96 scale factor code book\n");
            return AVERROR_INVALIDDATA;
        }
    }

    // Bit allocation quantizer select
    for (ch = xch_base; ch < s->x96_nchannels; ch++)
        s->bit_allocation_sel[ch] = get_bits(&s->gb, 3);

    // Quantization index codebook select
    for (n = 0; n < 6 + 4 * s->x96_high_res; n++)
        for (ch = xch_base; ch < s->x96_nchannels; ch++)
            s->quant_index_sel[ch][n] = get_bits(&s->gb, ff_dca2_quant_index_sel_nbits[n]);

    if (exss) {
        // Reserved
        // Byte align
        // CRC16 of channel set header
        if (ff_dca2_seek_bits(&s->gb, header_pos + header_size * 8)) {
            av_log(s->avctx, AV_LOG_ERROR, "Read past end of X96 channel set header\n");
            return AVERROR_INVALIDDATA;
        }
    } else {
        if (s->crc_present)
            skip_bits(&s->gb, 16);
    }

    return 0;
}

static int parse_x96_frame_data(DCA2CoreDecoder *s, int exss, int xch_base)
{
    int sf, ch, ret, band, sub_pos;

    if ((ret = parse_x96_coding_header(s, exss, xch_base)) < 0)
        return ret;

    for (sf = 0, sub_pos = 0; sf < s->nsubframes; sf++) {
        if ((ret = parse_x96_subframe_header(s, xch_base)) < 0)
            return ret;
        if ((ret = parse_x96_subframe_audio(s, sf, xch_base, &sub_pos)) < 0)
            return ret;
    }

    for (ch = xch_base; ch < s->x96_nchannels; ch++) {
        // Number of active subbands for this channel
        int nsubbands = s->nsubbands[ch];
        if (s->joint_intensity_index[ch])
            nsubbands = FFMAX(nsubbands, s->nsubbands[s->joint_intensity_index[ch] - 1]);

        // Update history for ADPCM
        // Clear inactive subbands
        for (band = 0; band < DCA2_SUBBANDS_X96; band++) {
            int *samples = s->x96_subband_samples[ch][band] - DCA2_ADPCM_COEFFS;
            if (band >= s->x96_subband_start && band < nsubbands)
                AV_COPY128(samples, samples + s->npcmblocks);
            else
                memset(samples, 0, (DCA2_ADPCM_COEFFS + s->npcmblocks) * sizeof(int));
        }
    }

    return 0;
}

static int parse_x96_frame(DCA2CoreDecoder *s)
{
    int ret;

    // Revision number
    s->x96_rev_no = get_bits(&s->gb, 4);
    if (s->x96_rev_no < 1 || s->x96_rev_no > 8) {
        av_log(s->avctx, AV_LOG_ERROR, "Unsupported X96 revision (%d)\n", s->x96_rev_no);
        return AVERROR_INVALIDDATA;
    }

    s->x96_crc_present = 0;
    s->x96_nchannels = s->nchannels;

    if ((ret = alloc_x96_sample_buffer(s)) < 0)
        return ret;

    if ((ret = parse_x96_frame_data(s, 0, 0)) < 0)
        return ret;

    // Seek to the end of core frame
    if (ff_dca2_seek_bits(&s->gb, s->frame_size * 8))
        return AVERROR_INVALIDDATA;

    return 0;
}

static int parse_x96_frame_exss(DCA2CoreDecoder *s)
{
    int     x96_frame_size[DCA2_EXSS_CHSETS_MAX];
    int     x96_nchannels[DCA2_EXSS_CHSETS_MAX];
    int     x96_nchsets, x96_base_ch;
    int     i, ret, header_size, header_pos = get_bits_count(&s->gb);

    // X96 sync word
    if (get_bits_long(&s->gb, 32) != DCA_SYNCWORD_X96) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid X96 sync word\n");
        return AVERROR_INVALIDDATA;
    }

    // X96 frame header length
    header_size = get_bits(&s->gb, 6) + 1;

    // Check X96 frame header CRC
    if (ff_dca2_check_crc(&s->gb, header_pos + 32, header_pos + header_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid X96 frame header checksum\n");
        return AVERROR_INVALIDDATA;
    }

    // Revision number
    s->x96_rev_no = get_bits(&s->gb, 4);
    if (s->x96_rev_no < 1 || s->x96_rev_no > 8) {
        av_log(s->avctx, AV_LOG_ERROR, "Unsupported X96 revision (%d)\n", s->x96_rev_no);
        return AVERROR_INVALIDDATA;
    }

    // CRC presence flag for channel set header
    s->x96_crc_present = get_bits1(&s->gb);

    // Number of channel sets
    x96_nchsets = get_bits(&s->gb, 2) + 1;

    // Channel set data byte size
    for (i = 0; i < x96_nchsets; i++)
        x96_frame_size[i] = get_bits(&s->gb, 12) + 1;

    // Number of channels in channel set
    for (i = 0; i < x96_nchsets; i++)
        x96_nchannels[i] = get_bits(&s->gb, 3) + 1;

    // Reserved
    // Byte align
    // CRC16 of X96 frame header
    if (ff_dca2_seek_bits(&s->gb, header_pos + header_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Read past end of X96 frame header\n");
        return AVERROR_INVALIDDATA;
    }

    if ((ret = alloc_x96_sample_buffer(s)) < 0)
        return ret;

    // Channel set data
    for (i = 0, x96_base_ch = 0; i < x96_nchsets; i++) {
        header_pos = get_bits_count(&s->gb);

        if (x96_base_ch + x96_nchannels[i] <= s->nchannels) {
            s->x96_nchannels = x96_base_ch + x96_nchannels[i];
            if ((ret = parse_x96_frame_data(s, 1, x96_base_ch)) < 0)
                return ret;
        }

        x96_base_ch += x96_nchannels[i];

        if (ff_dca2_seek_bits(&s->gb, header_pos + x96_frame_size[i] * 8)) {
            av_log(s->avctx, AV_LOG_ERROR, "Read past end of X96 channel set\n");
            return AVERROR_INVALIDDATA;
        }
    }

    return 0;
}

static int parse_aux_data(DCA2CoreDecoder *s)
{
    int aux_pos;

    if (get_bits_left(&s->gb) < 0)
        return AVERROR_INVALIDDATA;

    // Auxiliary data byte count (can't be trusted)
    skip_bits(&s->gb, 6);

    // 4-byte align
    skip_bits_long(&s->gb, -get_bits_count(&s->gb) & 31);

    // Auxiliary data sync word
    if (get_bits_long(&s->gb, 32) != DCA_SYNCWORD_REV1AUX) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid auxiliary data sync word\n");
        return AVERROR_INVALIDDATA;
    }

    aux_pos = get_bits_count(&s->gb);

    // Auxiliary decode time stamp flag
    if (get_bits1(&s->gb))
        skip_bits_long(&s->gb, 47);

    // Auxiliary dynamic downmix flag
    if (s->prim_dmix_embedded = get_bits1(&s->gb)) {
        int i, m, n;

        // Auxiliary primary channel downmix type
        s->prim_dmix_type = get_bits(&s->gb, 3);
        if (s->prim_dmix_type >= DCA2_DMIX_TYPE_COUNT) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid primary channel set downmix type\n");
            return AVERROR_INVALIDDATA;
        }

        // Size of downmix coefficients matrix
        m = ff_dca2_dmix_primary_nch[s->prim_dmix_type];
        n = ff_dca_channels[s->audio_mode] + !!s->lfe_present;

        // Dynamic downmix code coefficients
        for (i = 0; i < m * n; i++) {
            int code = get_bits(&s->gb, 9);
            int sign = (code >> 8) - 1;
            unsigned int index = code & 0xff;
            if (index >= FF_DCA_DMIXTABLE_SIZE) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid downmix coefficient index\n");
                return AVERROR_INVALIDDATA;
            }
            s->prim_dmix_coeff[i] = (ff_dca_dmixtable[index] ^ sign) - sign;
        }
    }

    // Byte align
    skip_bits(&s->gb, -get_bits_count(&s->gb) & 7);

    // CRC16 of auxiliary data
    skip_bits(&s->gb, 16);

    // Check CRC
    if (ff_dca2_check_crc(&s->gb, aux_pos, get_bits_count(&s->gb))) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid auxiliary data checksum\n");
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

static int parse_optional_info(DCA2CoreDecoder *s)
{
    DCA2Context *dca = s->avctx->priv_data;
    int ret = -1;

    // Time code stamp
    if (s->ts_present)
        skip_bits_long(&s->gb, 32);

    // Auxiliary data
    if (s->aux_present && (ret = parse_aux_data(s)) < 0
        && (s->avctx->err_recognition & AV_EF_EXPLODE))
        return ret;

    if (ret < 0)
        s->prim_dmix_embedded = 0;

    // Core extensions
    if (s->ext_audio_present && !dca->core_only) {
        int sync_pos = FFMIN(s->frame_size / 4, s->gb.size_in_bits / 32) - 1;
        int last_pos = get_bits_count(&s->gb) / 32;
        int size, dist;

        // Search for extension sync words aligned on 4-byte boundary. Search
        // must be done backwards from the end of core frame to work around
        // sync word aliasing issues.
        switch (s->ext_audio_type) {
        case EXT_AUDIO_XCH:
            if (dca->request_channel_layout)
                break;

            // The distance between XCH sync word and end of the core frame
            // must be equal to XCH frame size. Off by one error is allowed for
            // compatibility with legacy bitstreams. Minimum XCH frame size is
            // 96 bytes. AMODE and PCHS are further checked to reduce
            // probability of alias sync detection.
            for (; sync_pos >= last_pos; sync_pos--) {
                if (AV_RB32(s->gb.buffer + sync_pos * 4) == DCA_SYNCWORD_XCH) {
                    s->gb.index = (sync_pos + 1) * 32;
                    size = get_bits(&s->gb, 10) + 1;
                    dist = s->frame_size - sync_pos * 4;
                    if (size >= 96
                        && (size == dist || size - 1 == dist)
                        && get_bits(&s->gb, 7) == 0x08) {
                        s->xch_pos = get_bits_count(&s->gb);
                        break;
                    }
                }
            }

            if (s->avctx->err_recognition & AV_EF_EXPLODE) {
                av_log(s->avctx, AV_LOG_ERROR, "XCH sync word not found\n");
                return AVERROR_INVALIDDATA;
            }
            break;

        case EXT_AUDIO_X96:
            // The distance between X96 sync word and end of the core frame
            // must be equal to X96 frame size. Minimum X96 frame size is 96
            // bytes.
            for (; sync_pos >= last_pos; sync_pos--) {
                if (AV_RB32(s->gb.buffer + sync_pos * 4) == DCA_SYNCWORD_X96) {
                    s->gb.index = (sync_pos + 1) * 32;
                    size = get_bits(&s->gb, 12) + 1;
                    dist = s->frame_size - sync_pos * 4;
                    if (size >= 96 && size == dist) {
                        s->x96_pos = get_bits_count(&s->gb);
                        break;
                    }
                }
            }

            if (s->avctx->err_recognition & AV_EF_EXPLODE) {
                av_log(s->avctx, AV_LOG_ERROR, "X96 sync word not found\n");
                return AVERROR_INVALIDDATA;
            }
            break;

        case EXT_AUDIO_XXCH:
            if (dca->request_channel_layout)
                break;

            // XXCH frame header CRC must be valid. Minimum XXCH frame header
            // size is 11 bytes.
            for (; sync_pos >= last_pos; sync_pos--) {
                if (AV_RB32(s->gb.buffer + sync_pos * 4) == DCA_SYNCWORD_XXCH) {
                    s->gb.index = (sync_pos + 1) * 32;
                    size = get_bits(&s->gb, 6) + 1;
                    if (size >= 11 &&
                        !ff_dca2_check_crc(&s->gb, (sync_pos + 1) * 32,
                                           sync_pos * 32 + size * 8)) {
                        s->xxch_pos = sync_pos * 32;
                        break;
                    }
                }
            }

            if (s->avctx->err_recognition & AV_EF_EXPLODE) {
                av_log(s->avctx, AV_LOG_ERROR, "XXCH sync word not found\n");
                return AVERROR_INVALIDDATA;
            }
            break;
        }
    }

    return 0;
}

int ff_dca2_core_parse(DCA2CoreDecoder *s, uint8_t *data, int size)
{
    int ret;

    s->ext_audio_mask = 0;
    s->xch_pos = s->xxch_pos = s->x96_pos = 0;

    if ((ret = init_get_bits8(&s->gb, data, size)) < 0)
        return ret;

    skip_bits_long(&s->gb, 32);
    if ((ret = parse_frame_header(s)) < 0)
        return ret;
    if ((ret = alloc_sample_buffer(s)) < 0)
        return ret;
    if ((ret = parse_frame_data(s, HEADER_CORE, 0)) < 0)
        return ret;
    if ((ret = parse_optional_info(s)) < 0)
        return ret;

    // Workaround for DTS in WAV
    if (s->frame_size > size && s->frame_size < size + 4) {
        av_log(s->avctx, AV_LOG_DEBUG, "Working around excessive core frame size (%d > %d)\n", s->frame_size, size);
        s->frame_size = size;
    }

    if (ff_dca2_seek_bits(&s->gb, s->frame_size * 8)) {
        av_log(s->avctx, AV_LOG_ERROR, "Read past end of core frame\n");
        return ret;
    }

    return 0;
}

int ff_dca2_core_parse_exss(DCA2CoreDecoder *s, uint8_t *data, DCA2ExssAsset *asset)
{
    DCA2Context *dca = s->avctx->priv_data;
    GetBitContext temp = s->gb;
    int exss_mask = asset ? asset->extension_mask : 0;
    int ret = 0, ext = 0;

    // Parse (X)XCH unless downmixing
    if (!dca->request_channel_layout) {
        if (exss_mask & DCA2_EXSS_XXCH) {
            if ((ret = init_get_bits8(&s->gb, data + asset->xxch_offset, asset->xxch_size)) < 0)
                return ret;
            ret = parse_xxch_frame(s);
            ext = DCA2_EXSS_XXCH;
        } else if (s->xxch_pos) {
            s->gb.index = s->xxch_pos;
            ret = parse_xxch_frame(s);
            ext = DCA2_CSS_XXCH;
        } else if (s->xch_pos) {
            s->gb.index = s->xch_pos;
            ret = parse_xch_frame(s);
            ext = DCA2_CSS_XCH;
        }

        // Revert to primary channel set in case (X)XCH parsing fails
        if (ret < 0) {
            if (s->avctx->err_recognition & AV_EF_EXPLODE)
                return ret;
            s->nchannels = ff_dca_channels[s->audio_mode];
            s->ch_mask = audio_mode_ch_mask[s->audio_mode];
            if (s->lfe_present)
                s->ch_mask |= DCA2_SPEAKER_MASK_LFE1;
        } else {
            s->ext_audio_mask |= ext;
        }
    }

    // Parse XBR
    if (exss_mask & DCA2_EXSS_XBR) {
        if ((ret = init_get_bits8(&s->gb, data + asset->xbr_offset, asset->xbr_size)) < 0)
            return ret;
        if ((ret = parse_xbr_frame(s)) < 0) {
            if (s->avctx->err_recognition & AV_EF_EXPLODE)
                return ret;
        } else {
            s->ext_audio_mask |= DCA2_EXSS_XBR;
        }
    }

    // Parse X96
    if (exss_mask & DCA2_EXSS_X96) {
        if ((ret = init_get_bits8(&s->gb, data + asset->x96_offset, asset->x96_size)) < 0)
            return ret;
        if ((ret = parse_x96_frame_exss(s)) < 0) {
            if (s->avctx->err_recognition & AV_EF_EXPLODE)
                return ret;
        } else {
            s->ext_audio_mask |= DCA2_EXSS_X96;
        }
    } else if (s->x96_pos) {
        s->gb = temp;
        s->gb.index = s->x96_pos;
        if ((ret = parse_x96_frame(s)) < 0) {
            if (s->avctx->err_recognition & AV_EF_EXPLODE)
                return ret;
        } else {
            s->ext_audio_mask |= DCA2_CSS_X96;
        }
    }

    return 0;
}

static int map_prm_ch_to_spkr(DCA2CoreDecoder *s, int ch)
{
    int pos, spkr;

    // Try to map this channel to core first
    pos = ff_dca_channels[s->audio_mode];
    if (ch < pos) {
        spkr = prm_ch_to_spkr_map[s->audio_mode][ch];
        if (s->ext_audio_mask & (DCA2_CSS_XXCH | DCA2_EXSS_XXCH)) {
            if (s->xxch_core_mask & (1U << spkr))
                return spkr;
            if (spkr == DCA2_SPEAKER_Ls && (s->xxch_core_mask & DCA2_SPEAKER_MASK_Lss))
                return DCA2_SPEAKER_Lss;
            if (spkr == DCA2_SPEAKER_Rs && (s->xxch_core_mask & DCA2_SPEAKER_MASK_Rss))
                return DCA2_SPEAKER_Rss;
            return -1;
        }
        return spkr;
    }

    // Then XCH
    if ((s->ext_audio_mask & DCA2_CSS_XCH) && ch == pos)
        return DCA2_SPEAKER_Cs;

    // Then XXCH
    if (s->ext_audio_mask & (DCA2_CSS_XXCH | DCA2_EXSS_XXCH)) {
        for (spkr = DCA2_SPEAKER_Cs; spkr < s->xxch_mask_nbits; spkr++)
            if (s->xxch_spkr_mask & (1U << spkr))
                if (pos++ == ch)
                    return spkr;
    }

    // No mapping
    return -1;
}

static void erase_dsp_history(DCA2CoreDecoder *s)
{
    memset(s->dcadsp_data, 0, sizeof(s->dcadsp_data));
    s->output_history_lfe_fixed = 0;
    s->output_history_lfe_float = 0.0f;
}

int ff_dca2_core_filter_fixed(DCA2CoreDecoder *s, int x96_synth)
{
    int n, ch, spkr, nsamples, *ptr, x96_nchannels = 0;

    // Externally set x96_synth flag implies that X96 synthesis should be
    // enabled, yet actual X96 subband data should be discarded. This is a
    // special case for lossless residual decoder that ignores X96 data if
    // present.
    if (!x96_synth && (s->ext_audio_mask & (DCA2_CSS_X96 | DCA2_EXSS_X96))) {
        x96_nchannels = s->x96_nchannels;
        x96_synth = 1;
    }

    s->output_rate = s->sample_rate << x96_synth;
    s->npcmsamples = nsamples = (s->npcmblocks * DCA2_PCMBLOCK_SAMPLES) << x96_synth;

    // Reallocate PCM output buffer
    av_fast_malloc(&s->output_buffer, &s->output_size,
                   nsamples * av_popcount(s->ch_mask) * sizeof(int));
    if (!s->output_buffer)
        return AVERROR(ENOMEM);

    ptr = (int *)s->output_buffer;
    for (spkr = 0; spkr < DCA2_SPEAKER_COUNT; spkr++) {
        if (s->ch_mask & (1U << spkr)) {
            s->output_samples[spkr] = ptr;
            ptr += nsamples;
        } else {
            s->output_samples[spkr] = NULL;
        }
    }

    // Handle change of filtering mode
    if (s->filter_mode != (x96_synth | 2)) {
        erase_dsp_history(s);
        s->filter_mode = x96_synth | 2;
    }

    // Filter primary channels
    for (ch = 0; ch < s->nchannels; ch++) {
        // Map this primary channel to speaker
        spkr = map_prm_ch_to_spkr(s, ch);
        if (spkr < 0)
            return AVERROR(EINVAL);

        // Filter bank reconstruction
        s->dcadsp_fixed.sub_qmf[x96_synth](
            s->output_samples[spkr],
            s->subband_samples[ch],
            ch < x96_nchannels ? s->x96_subband_samples[ch] : NULL,
            &s->dcadsp_data[ch],
            s->npcmblocks,
            s->filter_perfect);
    }

    // Filter LFE channel
    if (s->lfe_present) {
        int *samples = s->output_samples[DCA2_SPEAKER_LFE1];
        int nlfesamples = s->npcmblocks >> 1;

        // Check LFF
        if (s->lfe_present == LFE_FLAG_128) {
            av_log(s->avctx, AV_LOG_ERROR, "Fixed point mode doesn't support LFF=1\n");
            return AVERROR(EINVAL);
        }

        // Offset intermediate buffer for X96
        if (x96_synth)
            samples += nsamples / 2;

        // Interpolate LFE channel
        s->dcadsp_fixed.lfe_fir(
            samples, s->lfe_samples + DCA2_LFE_HISTORY, s->npcmblocks);

        if (x96_synth) {
            // Filter 96 kHz oversampled LFE PCM to attenuate high frequency
            // (47.6 - 48.0 kHz) components of interpolation image
            int history = s->output_history_lfe_fixed;
            int *samples_out = s->output_samples[DCA2_SPEAKER_LFE1];
            for (n = 0; n < nsamples / 2; n++) {
                int64_t a = INT64_C(2097471) * samples[n] + INT64_C(6291137) * history;
                int64_t b = INT64_C(6291137) * samples[n] + INT64_C(2097471) * history;
                history = samples[n];
                samples_out[2 * n    ] = clip23(norm23(a));
                samples_out[2 * n + 1] = clip23(norm23(b));
            }

            // Update LFE PCM history
            s->output_history_lfe_fixed = history;
        }

        // Update LFE history
        for (n = DCA2_LFE_HISTORY - 1; n >= 0; n--)
            s->lfe_samples[n] = s->lfe_samples[nlfesamples + n];
    }

    return 0;
}

static int filter_frame_fixed(DCA2CoreDecoder *s, AVFrame *frame)
{
    AVCodecContext *avctx = s->avctx;
    int i, n, ch, ret, spkr, nsamples;

    if ((ret = ff_dca2_core_filter_fixed(s, 0)) < 0)
        return ret;

    avctx->sample_rate = s->output_rate;
    avctx->sample_fmt = AV_SAMPLE_FMT_S32P;
    avctx->bits_per_raw_sample = 24;

    frame->nb_samples = nsamples = s->npcmsamples;
    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    // Undo embedded XCH downmix
    if (s->es_format && (s->ext_audio_mask & DCA2_CSS_XCH)
        && s->audio_mode >= AMODE_2F2R) {
        int *samples_ls = s->output_samples[DCA2_SPEAKER_Ls];
        int *samples_rs = s->output_samples[DCA2_SPEAKER_Rs];
        int *samples_cs = s->output_samples[DCA2_SPEAKER_Cs];
        for (n = 0; n < nsamples; n++) {
            int cs = mul23(samples_cs[n], 5931520);
            samples_ls[n] = clip23(samples_ls[n] - cs);
            samples_rs[n] = clip23(samples_rs[n] - cs);
        }
    }

    // Undo embedded XXCH downmix
    if ((s->ext_audio_mask & (DCA2_CSS_XXCH | DCA2_EXSS_XXCH))
        && s->xxch_dmix_embedded) {
        int scale_inv   = s->xxch_dmix_scale_inv;
        int *coeff_ptr  = s->xxch_dmix_coeff;
        int xch_base    = ff_dca_channels[s->audio_mode];
        av_assert0(s->nchannels - xch_base <= DCA2_XXCH_CHANNELS_MAX);

        // Undo embedded core downmix pre-scaling
        for (spkr = 0; spkr < s->xxch_mask_nbits; spkr++)
            if (s->xxch_core_mask & (1U << spkr))
                vmul16(s->output_samples[spkr], scale_inv, nsamples);

        // Undo downmix
        for (ch = xch_base; ch < s->nchannels; ch++) {
            int src_spkr = map_prm_ch_to_spkr(s, ch);
            if (src_spkr < 0)
                return AVERROR(EINVAL);
            for (spkr = 0; spkr < s->xxch_mask_nbits; spkr++) {
                if (s->xxch_dmix_mask[ch - xch_base] & (1U << spkr)) {
                    int coeff = mul16(*coeff_ptr++, scale_inv);
                    if (coeff)
                        vmul15_sub(s->output_samples[spkr    ],
                                   s->output_samples[src_spkr],
                                   coeff, nsamples);
                }
            }
        }
    }

    // Downmix primary channel set to stereo
    if (s->request_mask != s->ch_mask) {
        ff_dca2_downmix_to_stereo_fixed(s->output_samples,
                                        s->prim_dmix_coeff,
                                        nsamples, s->ch_mask);
    }

    for (i = 0; i < avctx->channels; i++) {
        int32_t *samples = s->output_samples[s->ch_remap[i]];
        int32_t *plane = (int32_t *)frame->extended_data[i];
        for (n = 0; n < nsamples; n++)
            plane[n] = clip23(samples[n]) * (1 << 8);
    }

    return 0;
}

static int filter_frame_float(DCA2CoreDecoder *s, AVFrame *frame)
{
    AVCodecContext *avctx = s->avctx;
    int x96_nchannels = 0, x96_synth = 0;
    int i, n, ch, ret, spkr, nsamples, nchannels;
    float *output_samples[DCA2_SPEAKER_COUNT] = { NULL }, *ptr;

    if (s->ext_audio_mask & (DCA2_CSS_X96 | DCA2_EXSS_X96)) {
        x96_nchannels = s->x96_nchannels;
        x96_synth = 1;
    }

    avctx->sample_rate = s->sample_rate << x96_synth;
    avctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    avctx->bits_per_raw_sample = 0;

    frame->nb_samples = nsamples = (s->npcmblocks * DCA2_PCMBLOCK_SAMPLES) << x96_synth;
    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    // Build reverse speaker to channel mapping
    for (i = 0; i < avctx->channels; i++)
        output_samples[s->ch_remap[i]] = (float *)frame->extended_data[i];

    // Allocate space for extra channels
    nchannels = av_popcount(s->ch_mask) - avctx->channels;
    if (nchannels > 0) {
        av_fast_malloc(&s->output_buffer, &s->output_size,
                       nsamples * nchannels * sizeof(float));
        if (!s->output_buffer)
            return AVERROR(ENOMEM);

        ptr = (float *)s->output_buffer;
        for (spkr = 0; spkr < DCA2_SPEAKER_COUNT; spkr++) {
            if (!(s->ch_mask & (1U << spkr)))
                continue;
            if (output_samples[spkr])
                continue;
            output_samples[spkr] = ptr;
            ptr += nsamples;
        }
    }

    // Handle change of filtering mode
    if (s->filter_mode != x96_synth) {
        erase_dsp_history(s);
        s->filter_mode = x96_synth;
    }

    // Filter primary channels
    for (ch = 0; ch < s->nchannels; ch++) {
        // Map this primary channel to speaker
        spkr = map_prm_ch_to_spkr(s, ch);
        if (spkr < 0)
            return AVERROR(EINVAL);

        // Filter bank reconstruction
        s->dcadsp_float.sub_qmf[x96_synth](
            &s->imdct[x96_synth],
            &s->synth,
            output_samples[spkr],
            s->subband_samples[ch],
            ch < x96_nchannels ? s->x96_subband_samples[ch] : NULL,
            &s->dcadsp_data[ch],
            s->npcmblocks,
            s->filter_perfect);
    }

    // Filter LFE channel
    if (s->lfe_present) {
        int dec_select = (s->lfe_present == LFE_FLAG_128);
        float *samples = output_samples[DCA2_SPEAKER_LFE1];
        int nlfesamples = s->npcmblocks >> (dec_select + 1);

        // Offset intermediate buffer for X96
        if (x96_synth)
            samples += nsamples / 2;

        // Interpolate LFE channel
        s->dcadsp_float.lfe_fir[dec_select](
            samples, s->lfe_samples + DCA2_LFE_HISTORY, s->npcmblocks);

        if (x96_synth) {
            // Filter 96 kHz oversampled LFE PCM to attenuate high frequency
            // (47.6 - 48.0 kHz) components of interpolation image
            float history = s->output_history_lfe_float;
            float *samples_out = output_samples[DCA2_SPEAKER_LFE1];
            for (n = 0; n < nsamples / 2; n++) {
                float a = 0.25f * samples[n] + 0.75f * history;
                float b = 0.75f * samples[n] + 0.25f * history;
                history = samples[n];
                samples_out[2 * n    ] = a;
                samples_out[2 * n + 1] = b;
            }

            // Update LFE PCM history
            s->output_history_lfe_float = history;
        }

        // Update LFE history
        for (n = DCA2_LFE_HISTORY - 1; n >= 0; n--)
            s->lfe_samples[n] = s->lfe_samples[nlfesamples + n];
    }

    // Undo embedded XCH downmix
    if (s->es_format && (s->ext_audio_mask & DCA2_CSS_XCH)
        && s->audio_mode >= AMODE_2F2R) {
        s->fdsp.vector_fmac_scalar(output_samples[DCA2_SPEAKER_Ls],
                                   output_samples[DCA2_SPEAKER_Cs],
                                   -M_SQRT1_2, nsamples);
        s->fdsp.vector_fmac_scalar(output_samples[DCA2_SPEAKER_Rs],
                                   output_samples[DCA2_SPEAKER_Cs],
                                   -M_SQRT1_2, nsamples);
    }

    // Undo embedded XXCH downmix
    if ((s->ext_audio_mask & (DCA2_CSS_XXCH | DCA2_EXSS_XXCH))
        && s->xxch_dmix_embedded) {
        float scale_inv = s->xxch_dmix_scale_inv * (1.0f / (1 << 16));
        int *coeff_ptr  = s->xxch_dmix_coeff;
        int xch_base    = ff_dca_channels[s->audio_mode];
        av_assert0(s->nchannels - xch_base <= DCA2_XXCH_CHANNELS_MAX);

        // Undo downmix
        for (ch = xch_base; ch < s->nchannels; ch++) {
            int src_spkr = map_prm_ch_to_spkr(s, ch);
            if (src_spkr < 0)
                return AVERROR(EINVAL);
            for (spkr = 0; spkr < s->xxch_mask_nbits; spkr++) {
                if (s->xxch_dmix_mask[ch - xch_base] & (1U << spkr)) {
                    int coeff = *coeff_ptr++;
                    if (coeff) {
                        s->fdsp.vector_fmac_scalar(output_samples[    spkr],
                                                   output_samples[src_spkr],
                                                   coeff * (-1.0f / (1 << 15)),
                                                   nsamples);
                    }
                }
            }
        }

        // Undo embedded core downmix pre-scaling
        for (spkr = 0; spkr < s->xxch_mask_nbits; spkr++) {
            if (s->xxch_core_mask & (1U << spkr)) {
                s->fdsp.vector_fmul_scalar(output_samples[spkr],
                                           output_samples[spkr],
                                           scale_inv, nsamples);
            }
        }
    }

    // Downmix primary channel set to stereo
    if (s->request_mask != s->ch_mask) {
        ff_dca2_downmix_to_stereo_float(&s->fdsp, output_samples,
                                        s->prim_dmix_coeff,
                                        nsamples, s->ch_mask);
    }

    return 0;
}

int ff_dca2_core_filter_frame(DCA2CoreDecoder *s, AVFrame *frame)
{
    AVCodecContext *avctx = s->avctx;
    DCA2Context *dca = avctx->priv_data;
    enum AVMatrixEncoding matrix_encoding;
    int ret;

    // Handle downmixing to stereo request
    if (dca->request_channel_layout == DCA2_SPEAKER_LAYOUT_STEREO
        && s->audio_mode > AMODE_MONO && s->prim_dmix_embedded
        && (s->prim_dmix_type == DCA2_DMIX_TYPE_LoRo ||
            s->prim_dmix_type == DCA2_DMIX_TYPE_LtRt))
        s->request_mask = DCA2_SPEAKER_LAYOUT_STEREO;
    else
        s->request_mask = s->ch_mask;
    if (!ff_dca2_set_channel_layout(avctx, s->ch_remap, s->request_mask))
        return AVERROR(EINVAL);

    // Filter the frame
    if (avctx->flags & AV_CODEC_FLAG_BITEXACT)
        ret = filter_frame_fixed(s, frame);
    else
        ret = filter_frame_float(s, frame);
    if (ret < 0)
        return ret;

    // Set profile, bit rate, etc
    if (s->ext_audio_mask & DCA2_EXSS_MASK)
        avctx->profile = FF_PROFILE_DTS_HD_HRA;
    else if (s->ext_audio_mask & (DCA2_CSS_XXCH | DCA2_CSS_XCH))
        avctx->profile = FF_PROFILE_DTS_ES;
    else if (s->ext_audio_mask & DCA2_CSS_X96)
        avctx->profile = FF_PROFILE_DTS_96_24;
    else
        avctx->profile = FF_PROFILE_DTS;

    if (s->bit_rate > 3 && !(s->ext_audio_mask & DCA2_EXSS_MASK))
        avctx->bit_rate = s->bit_rate;
    else
        avctx->bit_rate = 0;

    if (s->audio_mode == AMODE_STEREO_TOTAL || (s->request_mask != s->ch_mask &&
                                                s->prim_dmix_type == DCA2_DMIX_TYPE_LtRt))
        matrix_encoding = AV_MATRIX_ENCODING_DOLBY;
    else
        matrix_encoding = AV_MATRIX_ENCODING_NONE;
    if ((ret = ff_side_data_update_matrix_encoding(frame, matrix_encoding)) < 0)
        return ret;

    return 0;
}

av_cold void ff_dca2_core_flush(DCA2CoreDecoder *s)
{
    if (s->subband_buffer) {
        erase_adpcm_history(s);
        memset(s->lfe_samples, 0, DCA2_LFE_HISTORY * sizeof(int));
    }

    if (s->x96_subband_buffer)
        erase_x96_adpcm_history(s);

    erase_dsp_history(s);
}

av_cold int ff_dca2_core_init(DCA2CoreDecoder *s)
{
    dca2_init_vlcs();

    avpriv_float_dsp_init(&s->fdsp, 0);
    ff_synth_filter_init(&s->synth);
    ff_mdct_init(&s->imdct[0], 6, 1, 1.0);
    ff_mdct_init(&s->imdct[1], 7, 1, 1.0);
    ff_dcadsp2_float_init(&s->dcadsp_float);
    ff_dcadsp2_fixed_init(&s->dcadsp_fixed);

    s->x96_rand = 1;
    return 0;
}

av_cold void ff_dca2_core_close(DCA2CoreDecoder *s)
{
    ff_mdct_end(&s->imdct[0]);
    ff_mdct_end(&s->imdct[1]);

    av_freep(&s->subband_buffer);
    s->subband_size = 0;

    av_freep(&s->x96_subband_buffer);
    s->x96_subband_size = 0;

    av_freep(&s->output_buffer);
    s->output_size = 0;
}
