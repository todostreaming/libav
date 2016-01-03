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

#ifndef AVCODEC_DCA2_H
#define AVCODEC_DCA2_H

#include "libavutil/common.h"
#include "libavutil/avassert.h"
#include "libavutil/channel_layout.h"
#include "libavutil/float_dsp.h"
#include "libavutil/opt.h"

#include "avcodec.h"
#include "internal.h"
#include "dca_syncwords.h"
#include "dcadata.h"
#include "dcadata2.h"
#include "get_bits.h"
#include "unary.h"
#include "fft.h"
#include "synth_filter.h"
#include "profiles.h"

#define DCA2_BUFFER_PADDING_SIZE    1024

#define DCA2_PACKET_CORE        0x01
#define DCA2_PACKET_EXSS        0x02
#define DCA2_PACKET_XLL         0x04
#define DCA2_PACKET_RECOVERY    0x08

enum DCA2Speaker {
    DCA2_SPEAKER_C,    DCA2_SPEAKER_L,    DCA2_SPEAKER_R,    DCA2_SPEAKER_Ls,
    DCA2_SPEAKER_Rs,   DCA2_SPEAKER_LFE1, DCA2_SPEAKER_Cs,   DCA2_SPEAKER_Lsr,
    DCA2_SPEAKER_Rsr,  DCA2_SPEAKER_Lss,  DCA2_SPEAKER_Rss,  DCA2_SPEAKER_Lc,
    DCA2_SPEAKER_Rc,   DCA2_SPEAKER_Lh,   DCA2_SPEAKER_Ch,   DCA2_SPEAKER_Rh,
    DCA2_SPEAKER_LFE2, DCA2_SPEAKER_Lw,   DCA2_SPEAKER_Rw,   DCA2_SPEAKER_Oh,
    DCA2_SPEAKER_Lhs,  DCA2_SPEAKER_Rhs,  DCA2_SPEAKER_Chr,  DCA2_SPEAKER_Lhr,
    DCA2_SPEAKER_Rhr,  DCA2_SPEAKER_Cl,   DCA2_SPEAKER_Ll,   DCA2_SPEAKER_Rl,
    DCA2_SPEAKER_RSV1, DCA2_SPEAKER_RSV2, DCA2_SPEAKER_RSV3, DCA2_SPEAKER_RSV4,

    DCA2_SPEAKER_COUNT
};

enum DCA2SpeakerMask {
    DCA2_SPEAKER_MASK_C     = 0x00000001,
    DCA2_SPEAKER_MASK_L     = 0x00000002,
    DCA2_SPEAKER_MASK_R     = 0x00000004,
    DCA2_SPEAKER_MASK_Ls    = 0x00000008,
    DCA2_SPEAKER_MASK_Rs    = 0x00000010,
    DCA2_SPEAKER_MASK_LFE1  = 0x00000020,
    DCA2_SPEAKER_MASK_Cs    = 0x00000040,
    DCA2_SPEAKER_MASK_Lsr   = 0x00000080,
    DCA2_SPEAKER_MASK_Rsr   = 0x00000100,
    DCA2_SPEAKER_MASK_Lss   = 0x00000200,
    DCA2_SPEAKER_MASK_Rss   = 0x00000400,
    DCA2_SPEAKER_MASK_Lc    = 0x00000800,
    DCA2_SPEAKER_MASK_Rc    = 0x00001000,
    DCA2_SPEAKER_MASK_Lh    = 0x00002000,
    DCA2_SPEAKER_MASK_Ch    = 0x00004000,
    DCA2_SPEAKER_MASK_Rh    = 0x00008000,
    DCA2_SPEAKER_MASK_LFE2  = 0x00010000,
    DCA2_SPEAKER_MASK_Lw    = 0x00020000,
    DCA2_SPEAKER_MASK_Rw    = 0x00040000,
    DCA2_SPEAKER_MASK_Oh    = 0x00080000,
    DCA2_SPEAKER_MASK_Lhs   = 0x00100000,
    DCA2_SPEAKER_MASK_Rhs   = 0x00200000,
    DCA2_SPEAKER_MASK_Chr   = 0x00400000,
    DCA2_SPEAKER_MASK_Lhr   = 0x00800000,
    DCA2_SPEAKER_MASK_Rhr   = 0x01000000,
    DCA2_SPEAKER_MASK_Cl    = 0x02000000,
    DCA2_SPEAKER_MASK_Ll    = 0x04000000,
    DCA2_SPEAKER_MASK_Rl    = 0x08000000,
};

#define DCA2_SPEAKER_LAYOUT_MONO         (DCA2_SPEAKER_MASK_C)
#define DCA2_SPEAKER_LAYOUT_STEREO       (DCA2_SPEAKER_MASK_L | DCA2_SPEAKER_MASK_R)
#define DCA2_SPEAKER_LAYOUT_2POINT1      (DCA2_SPEAKER_LAYOUT_STEREO | DCA2_SPEAKER_MASK_LFE1)
#define DCA2_SPEAKER_LAYOUT_3_0          (DCA2_SPEAKER_LAYOUT_STEREO | DCA2_SPEAKER_MASK_C)
#define DCA2_SPEAKER_LAYOUT_2_1          (DCA2_SPEAKER_LAYOUT_STEREO | DCA2_SPEAKER_MASK_Cs)
#define DCA2_SPEAKER_LAYOUT_3_1          (DCA2_SPEAKER_LAYOUT_3_0 | DCA2_SPEAKER_MASK_Cs)
#define DCA2_SPEAKER_LAYOUT_2_2          (DCA2_SPEAKER_LAYOUT_STEREO | DCA2_SPEAKER_MASK_Ls | DCA2_SPEAKER_MASK_Rs)
#define DCA2_SPEAKER_LAYOUT_5POINT0      (DCA2_SPEAKER_LAYOUT_3_0 | DCA2_SPEAKER_MASK_Ls | DCA2_SPEAKER_MASK_Rs)
#define DCA2_SPEAKER_LAYOUT_5POINT1      (DCA2_SPEAKER_LAYOUT_5POINT0 | DCA2_SPEAKER_MASK_LFE1)
#define DCA2_SPEAKER_LAYOUT_7POINT0_WIDE (DCA2_SPEAKER_LAYOUT_5POINT0 | DCA2_SPEAKER_MASK_Lw | DCA2_SPEAKER_MASK_Rw)
#define DCA2_SPEAKER_LAYOUT_7POINT1_WIDE (DCA2_SPEAKER_LAYOUT_7POINT0_WIDE | DCA2_SPEAKER_MASK_LFE1)

enum DCA2RepresentationType {
    DCA2_REPR_TYPE_LtRt = 2,
    DCA2_REPR_TYPE_LhRh = 3
};

enum DCA2ExtensionMask {
    DCA2_CSS_CORE   = 0x001,
    DCA2_CSS_XXCH   = 0x002,
    DCA2_CSS_X96    = 0x004,
    DCA2_CSS_XCH    = 0x008,
    DCA2_CSS_MASK   = 0x00f,
    DCA2_EXSS_CORE  = 0x010,
    DCA2_EXSS_XBR   = 0x020,
    DCA2_EXSS_XXCH  = 0x040,
    DCA2_EXSS_X96   = 0x080,
    DCA2_EXSS_LBR   = 0x100,
    DCA2_EXSS_XLL   = 0x200,
    DCA2_EXSS_RSV1  = 0x400,
    DCA2_EXSS_RSV2  = 0x800,
    DCA2_EXSS_MASK  = 0xff0,
};

enum DCA2DownMixType {
    DCA2_DMIX_TYPE_1_0,
    DCA2_DMIX_TYPE_LoRo,
    DCA2_DMIX_TYPE_LtRt,
    DCA2_DMIX_TYPE_3_0,
    DCA2_DMIX_TYPE_2_1,
    DCA2_DMIX_TYPE_2_2,
    DCA2_DMIX_TYPE_3_1,

    DCA2_DMIX_TYPE_COUNT
};

// ============================================================================

typedef struct DCA2ExssAsset {
    int     asset_offset;   ///< Offset to asset data from start of substream
    int     asset_size;     ///< Size of encoded asset data
    int     asset_index;    ///< Audio asset identifier

    int     pcm_bit_res;                ///< PCM bit resolution
    int     max_sample_rate;            ///< Maximum sample rate
    int     nchannels_total;            ///< Total number of channels
    int     one_to_one_map_ch_to_spkr;  ///< One to one channel to speaker mapping flag
    int     embedded_stereo;            ///< Embedded stereo flag
    int     embedded_6ch;               ///< Embedded 6 channels flag
    int     spkr_mask_enabled;          ///< Speaker mask enabled flag
    int     spkr_mask;                  ///< Loudspeaker activity mask
    int     representation_type;        ///< Representation type

    int     coding_mode;        ///< Coding mode for the asset
    int     extension_mask;     ///< Coding components used in asset

    int     core_offset;    ///< Offset to core component from start of substream
    int     core_size;      ///< Size of core component in extension substream

    int     xbr_offset;     ///< Offset to XBR extension from start of substream
    int     xbr_size;       ///< Size of XBR extension in extension substream

    int     xxch_offset;    ///< Offset to XXCH extension from start of substream
    int     xxch_size;      ///< Size of XXCH extension in extension substream

    int     x96_offset;     ///< Offset to X96 extension from start of substream
    int     x96_size;       ///< Size of X96 extension in extension substream

    int     lbr_offset;     ///< Offset to LBR component from start of substream
    int     lbr_size;       ///< Size of LBR component in extension substream

    int     xll_offset;         ///< Offset to XLL data from start of substream
    int     xll_size;           ///< Size of XLL data in extension substream
    int     xll_sync_present;   ///< XLL sync word present flag
    int     xll_delay_nframes;  ///< Initial XLL decoding delay in frames
    int     xll_sync_offset;    ///< Number of bytes offset to XLL sync

    int     hd_stream_id;   ///< DTS-HD stream ID
} DCA2ExssAsset;

typedef struct DCA2ExssParser {
    AVCodecContext  *avctx;
    GetBitContext   gb;

    int     exss_index;         ///< Extension substream index
    int     exss_size_nbits;    ///< Number of bits for extension substream size
    int     exss_size;          ///< Number of bytes of extension substream

    int     static_fields_present;  ///< Per stream static fields presence flag
    int     npresents;  ///< Number of defined audio presentations
    int     nassets;    ///< Number of audio assets in extension substream

    int     mix_metadata_enabled;   ///< Mixing metadata enable flag
    int     nmixoutconfigs;         ///< Number of mixing configurations
    int     nmixoutchs[4];          ///< Speaker layout mask for mixer output channels

    DCA2ExssAsset   assets[1];    ///< Audio asset descriptors
} DCA2ExssParser;

int ff_dca2_exss_parse(DCA2ExssParser *s, uint8_t *data, int size);

// ============================================================================

#define DCA2_CHANNELS           7
#define DCA2_SUBBANDS           32
#define DCA2_SUBBANDS_X96       64
#define DCA2_SUBFRAMES          16
#define DCA2_SUBBAND_SAMPLES    8
#define DCA2_PCMBLOCK_SAMPLES   32
#define DCA2_ADPCM_COEFFS       4
#define DCA2_LFE_HISTORY        8
#define DCA2_CODE_BOOKS         10

#define DCA2_CORE_CHANNELS_MAX      6
#define DCA2_DMIX_CHANNELS_MAX      4
#define DCA2_XXCH_CHANNELS_MAX      2
#define DCA2_EXSS_CHANNELS_MAX      8
#define DCA2_EXSS_CHSETS_MAX        4

typedef struct DCA2DspData {
    union {
        struct {
            DECLARE_ALIGNED(32, float, hist1)[512];
            DECLARE_ALIGNED(32, float, hist2)[32];
        } flt32;
        struct {
            DECLARE_ALIGNED(32, int, hist1)[512];
            DECLARE_ALIGNED(32, int, hist2)[32];
        } fix32;
        struct {
            DECLARE_ALIGNED(32, float, hist1)[1024];
            DECLARE_ALIGNED(32, float, hist2)[64];
        } flt64;
        struct {
            DECLARE_ALIGNED(32, int, hist1)[1024];
            DECLARE_ALIGNED(32, int, hist2)[64];
        } fix64;
    } u;
    int offset;
} DCA2DspData;

typedef struct DCA2FloatDspContext {
    void (*lfe_fir[2])(float *pcm_samples, int *lfe_samples, int npcmblocks);
    void (*sub_qmf[2])(FFTContext *imdct,
                       SynthFilterContext *synth,
                       float *pcm_samples,
                       int **subband_samples_lo,
                       int **subband_samples_hi,
                       DCA2DspData *dsp,
                       int nsamples, int perfect);
} DCA2FloatDspContext;

typedef struct DCA2FixedDspContext {
    void (*lfe_fir)(int *pcm_samples, int *lfe_samples, int npcmblocks);
    void (*sub_qmf[2])(int *pcm_samples,
                       int **subband_samples_lo,
                       int **subband_samples_hi,
                       DCA2DspData *dsp,
                       int nsamples, int perfect);
} DCA2FixedDspContext;

typedef struct DCA2CoreDecoder {
    AVCodecContext  *avctx;
    GetBitContext   gb;

    // Bit stream header
    int     crc_present;        ///< CRC present flag
    int     npcmblocks;         ///< Number of PCM sample blocks
    int     frame_size;         ///< Primary frame byte size
    int     audio_mode;         ///< Audio channel arrangement
    int     sample_rate;        ///< Core audio sampling frequency
    int     bit_rate;           ///< Transmission bit rate
    int     drc_present;        ///< Embedded dynamic range flag
    int     ts_present;         ///< Embedded time stamp flag
    int     aux_present;        ///< Auxiliary data flag
    int     ext_audio_type;     ///< Extension audio descriptor flag
    int     ext_audio_present;  ///< Extended coding flag
    int     sync_ssf;           ///< Audio sync word insertion flag
    int     lfe_present;        ///< Low frequency effects flag
    int     predictor_history;  ///< Predictor history flag switch
    int     filter_perfect;     ///< Multirate interpolator switch
    int     source_pcm_res;     ///< Source PCM resolution
    int     es_format;          ///< Extended surround (ES) mastering flag
    int     sumdiff_front;      ///< Front sum/difference flag
    int     sumdiff_surround;   ///< Surround sum/difference flag

    // Primary audio coding header
    int         nsubframes;     ///< Number of subframes
    int         nchannels;      ///< Number of primary audio channels (incl. extension channels)
    int         ch_mask;        ///< Speaker layout mask (incl. LFE and extension channels)
    int8_t      nsubbands[DCA2_CHANNELS];                ///< Subband activity count
    int8_t      subband_vq_start[DCA2_CHANNELS];         ///< High frequency VQ start subband
    int8_t      joint_intensity_index[DCA2_CHANNELS];    ///< Joint intensity coding index
    int8_t      transition_mode_sel[DCA2_CHANNELS];      ///< Transient mode code book
    int8_t      scale_factor_sel[DCA2_CHANNELS];         ///< Scale factor code book
    int8_t      bit_allocation_sel[DCA2_CHANNELS];       ///< Bit allocation quantizer select
    int8_t      quant_index_sel[DCA2_CHANNELS][DCA2_CODE_BOOKS];  ///< Quantization index codebook select
    int32_t     scale_factor_adj[DCA2_CHANNELS][DCA2_CODE_BOOKS]; ///< Scale factor adjustment

    // Primary audio coding side information
    int8_t      nsubsubframes[DCA2_SUBFRAMES];   ///< Subsubframe count for each subframe
    int8_t      prediction_mode[DCA2_CHANNELS][DCA2_SUBBANDS_X96];             ///< Prediction mode
    int16_t     prediction_vq_index[DCA2_CHANNELS][DCA2_SUBBANDS_X96];         ///< Prediction coefficients VQ address
    int8_t      bit_allocation[DCA2_CHANNELS][DCA2_SUBBANDS_X96];              ///< Bit allocation index
    int8_t      transition_mode[DCA2_SUBFRAMES][DCA2_CHANNELS][DCA2_SUBBANDS];  ///< Transition mode
    int32_t     scale_factors[DCA2_CHANNELS][DCA2_SUBBANDS][2];                ///< Scale factors (2x for transients and X96)
    int8_t      joint_scale_sel[DCA2_CHANNELS];                                ///< Joint subband codebook select
    int32_t     joint_scale_factors[DCA2_CHANNELS][DCA2_SUBBANDS_X96];         ///< Scale factors for joint subband coding

    // Auxiliary data
    int     prim_dmix_embedded; ///< Auxiliary dynamic downmix flag
    int     prim_dmix_type;     ///< Auxiliary primary channel downmix type
    int     prim_dmix_coeff[DCA2_DMIX_CHANNELS_MAX * DCA2_CORE_CHANNELS_MAX]; ///< Dynamic downmix code coefficients

    // Core extensions
    int     ext_audio_mask;     ///< Bit mask of fully decoded core extensions

    // XCH extension data
    int     xch_pos;    ///< Bit position of XCH frame in core substream

    // XXCH extension data
    int     xxch_crc_present;       ///< CRC presence flag for XXCH channel set header
    int     xxch_mask_nbits;        ///< Number of bits for loudspeaker mask
    int     xxch_core_mask;         ///< Core loudspeaker activity mask
    int     xxch_spkr_mask;         ///< Loudspeaker layout mask
    int     xxch_dmix_embedded;     ///< Downmix already performed by encoder
    int     xxch_dmix_scale_inv;    ///< Downmix scale factor
    int     xxch_dmix_mask[DCA2_XXCH_CHANNELS_MAX];  ///< Downmix channel mapping mask
    int     xxch_dmix_coeff[DCA2_XXCH_CHANNELS_MAX * DCA2_CORE_CHANNELS_MAX];     ///< Downmix coefficients
    int     xxch_pos;   ///< Bit position of XXCH frame in core substream

    // X96 extension data
    int     x96_rev_no;         ///< X96 revision number
    int     x96_crc_present;    ///< CRC presence flag for X96 channel set header
    int     x96_nchannels;      ///< Number of primary channels in X96 extension
    int     x96_high_res;       ///< X96 high resolution flag
    int     x96_subband_start;  ///< First encoded subband in X96 extension
    int     x96_rand;           ///< Random seed for generating samples for unallocated X96 subbands
    int     x96_pos;            ///< Bit position of X96 frame in core substream

    // Sample buffers
    unsigned int    x96_subband_size;
    int             *x96_subband_buffer;    ///< X96 subband sample buffer base
    int             *x96_subband_samples[DCA2_CHANNELS][DCA2_SUBBANDS_X96];   ///< X96 subband samples

    unsigned int    subband_size;
    int             *subband_buffer;    ///< Subband sample buffer base
    int             *subband_samples[DCA2_CHANNELS][DCA2_SUBBANDS];   ///< Subband samples
    int             *lfe_samples;    ///< Decimated LFE samples

    // DSP contexts
    DCA2DspData             dcadsp_data[DCA2_CHANNELS];    ///< FIR history buffers
    DCA2FloatDspContext     dcadsp_float;
    DCA2FixedDspContext     dcadsp_fixed;
    FFTContext              imdct[2];
    SynthFilterContext      synth;
    AVFloatDSPContext       fdsp;

    // PCM output data
    unsigned int    output_size;
    void            *output_buffer;                         ///< PCM output buffer base
    int             *output_samples[DCA2_SPEAKER_COUNT];    ///< PCM output speaker map
    int             output_history_lfe_fixed;               ///< LFE PCM history for X96 filter
    float           output_history_lfe_float;               ///< LFE PCM history for X96 filter

    int     ch_remap[DCA2_SPEAKER_COUNT];
    int     request_mask;

    int     npcmsamples;    ///< Number of PCM samples per channel
    int     output_rate;    ///< Output sample rate (1x or 2x header rate)

    int     filter_mode;   ///< Previous filtering mode for detecting changes
} DCA2CoreDecoder;

static inline int ff_dca2_core_map_spkr(DCA2CoreDecoder *core, int spkr)
{
    if (core->ch_mask & (1U << spkr))
        return spkr;
    if (spkr == DCA2_SPEAKER_Lss && (core->ch_mask & DCA2_SPEAKER_MASK_Ls))
        return DCA2_SPEAKER_Ls;
    if (spkr == DCA2_SPEAKER_Rss && (core->ch_mask & DCA2_SPEAKER_MASK_Rs))
        return DCA2_SPEAKER_Rs;
    return -1;
}

int ff_dca2_core_parse(DCA2CoreDecoder *s, uint8_t *data, int size);
int ff_dca2_core_parse_exss(DCA2CoreDecoder *s, uint8_t *data, DCA2ExssAsset *asset);
int ff_dca2_core_filter_fixed(DCA2CoreDecoder *s, int x96_synth);
int ff_dca2_core_filter_frame(DCA2CoreDecoder *s, AVFrame *frame);
av_cold void ff_dca2_core_flush(DCA2CoreDecoder *s);
av_cold int ff_dca2_core_init(DCA2CoreDecoder *s);
av_cold void ff_dca2_core_close(DCA2CoreDecoder *s);

av_cold void ff_dcadsp2_float_init(DCA2FloatDspContext *s);
av_cold void ff_dcadsp2_fixed_init(DCA2FixedDspContext *s);

// ============================================================================

#define DCA2_XLL_CHSETS_MAX             3
#define DCA2_XLL_CHANNELS_MAX           8
#define DCA2_XLL_BANDS_MAX              2
#define DCA2_XLL_ADAPT_PRED_ORDER_MAX   16
#define DCA2_XLL_DECI_HISTORY_MAX       8
#define DCA2_XLL_DMIX_SCALES_MAX        ((DCA2_XLL_CHSETS_MAX - 1) * DCA2_XLL_CHANNELS_MAX)
#define DCA2_XLL_DMIX_COEFFS_MAX        (DCA2_XLL_DMIX_SCALES_MAX * DCA2_XLL_CHANNELS_MAX)
#define DCA2_XLL_PBR_BUFFER_MAX         (240 << 10)
#define DCA2_XLL_SAMPLE_BUFFERS_MAX     3

typedef struct DCA2XllBand {
    int     decor_enabled;                      ///< Pairwise channel decorrelation flag
    int     orig_order[DCA2_XLL_CHANNELS_MAX];       ///< Original channel order
    int     decor_coeff[DCA2_XLL_CHANNELS_MAX / 2];  ///< Pairwise channel coefficients

    int     adapt_pred_order[DCA2_XLL_CHANNELS_MAX]; ///< Adaptive predictor order
    int     highest_pred_order;                 ///< Highest adaptive predictor order
    int     fixed_pred_order[DCA2_XLL_CHANNELS_MAX]; ///< Fixed predictor order
    int     adapt_refl_coeff[DCA2_XLL_CHANNELS_MAX][DCA2_XLL_ADAPT_PRED_ORDER_MAX];   ///< Adaptive predictor reflection coefficients

    int     dmix_embedded;  ///< Downmix performed by encoder in frequency band

    int     lsb_section_size;                   ///< Size of LSB section in any segment
    int     nscalablelsbs[DCA2_XLL_CHANNELS_MAX];    ///< Number of bits to represent the samples in LSB part
    int     bit_width_adjust[DCA2_XLL_CHANNELS_MAX]; ///< Number of bits discarded by authoring

    int     *msb_sample_buffer[DCA2_XLL_CHANNELS_MAX];   ///< MSB sample buffer pointers
    int     *lsb_sample_buffer[DCA2_XLL_CHANNELS_MAX];   ///< LSB sample buffer pointers or NULL
} DCA2XllBand;

typedef struct DCA2XllChSet {
    // Channel set header
    int     nchannels;          ///< Number of channels in the channel set (N)
    int     residual_encode;    ///< Residual encoding mask (0 - residual, 1 - full channel)
    int     pcm_bit_res;        ///< PCM bit resolution (variable)
    int     storage_bit_res;    ///< Storage bit resolution (16 or 24)
    int     freq;               ///< Original sampling frequency (max. 96000 Hz)

    int     primary_chset;          ///< Primary channel set flag
    int     dmix_coeffs_present;    ///< Downmix coefficients present in stream
    int     dmix_embedded;          ///< Downmix already performed by encoder
    int     dmix_type;              ///< Primary channel set downmix type
    int     hier_chset;             ///< Whether the channel set is part of a hierarchy
    int     hier_ofs;               ///< Number of preceding channels in a hierarchy (M)
    int     dmix_coeff[DCA2_XLL_DMIX_COEFFS_MAX];       ///< Downmixing coefficients
    int     dmix_scale[DCA2_XLL_DMIX_SCALES_MAX];       ///< Downmixing scales
    int     dmix_scale_inv[DCA2_XLL_DMIX_SCALES_MAX];   ///< Inverse downmixing scales
    int     ch_mask;                ///< Channel mask for set
    int     ch_remap[DCA2_XLL_CHANNELS_MAX];

    int     nfreqbands; ///< Number of frequency bands (1 or 2)
    int     nabits;     ///< Number of bits to read bit allocation coding parameter

    DCA2XllBand     bands[DCA2_XLL_BANDS_MAX];   ///< Frequency bands

    // Frequency band coding parameters
    int     seg_common;                                     ///< Segment type
    int     rice_code_flag[DCA2_XLL_CHANNELS_MAX];          ///< Rice coding flag
    int     bitalloc_hybrid_linear[DCA2_XLL_CHANNELS_MAX];  ///< Binary code length for isolated samples
    int     bitalloc_part_a[DCA2_XLL_CHANNELS_MAX];         ///< Coding parameter for part A of segment
    int     bitalloc_part_b[DCA2_XLL_CHANNELS_MAX];         ///< Coding parameter for part B of segment
    int     nsamples_part_a[DCA2_XLL_CHANNELS_MAX];         ///< Number of samples in part A of segment

    // Decimator history
    int     deci_history[DCA2_XLL_CHANNELS_MAX][DCA2_XLL_DECI_HISTORY_MAX];   ///< Decimator history for frequency band 1

    // Sample buffers
    unsigned int    sample_size[DCA2_XLL_SAMPLE_BUFFERS_MAX];
    int             *sample_buffer[DCA2_XLL_SAMPLE_BUFFERS_MAX];
} DCA2XllChSet;

typedef struct DCA2XllDecoder {
    AVCodecContext  *avctx;
    GetBitContext   gb;

    int     frame_size;             ///< Number of bytes in a lossless frame
    int     nchsets;                ///< Number of channels sets per frame
    int     nframesegs;             ///< Number of segments per frame
    int     nsegsamples_log2;       ///< log2(nsegsamples)
    int     nsegsamples;            ///< Samples in segment per one frequency band
    int     nframesamples_log2;     ///< log2(nframesamples)
    int     nframesamples;          ///< Samples in frame per one frequency band
    int     seg_size_nbits;         ///< Number of bits used to read segment size
    int     band_crc_present;       ///< Presence of CRC16 within each frequency band
    int     scalable_lsbs;          ///< MSB/LSB split flag
    int     ch_mask_nbits;          ///< Number of bits used to read channel mask
    int     fixed_lsb_width;        ///< Fixed LSB width

    DCA2XllChSet    chset[DCA2_XLL_CHSETS_MAX]; ///< Channel sets

    int             *navi;          ///< NAVI table
    unsigned int    navi_size;

    int     nfreqbands;     ///< Highest number of frequency bands
    int     nchannels;      ///< Total number of channels in a hierarchy
    int     nactivechsets;  ///< Number of active channel sets to decode

    int     hd_stream_id;   ///< Previous DTS-HD stream ID for detecting changes

    uint8_t     *pbr_buffer;        ///< Peak bit rate (PBR) smoothing buffer
    int         pbr_length;         ///< Length in bytes of data currently buffered
    int         pbr_delay;          ///< Delay in frames before decoding buffered data

    int     output_mask;
    int     *output_samples[DCA2_SPEAKER_COUNT];
} DCA2XllDecoder;

int ff_dca2_xll_parse(DCA2XllDecoder *s, uint8_t *data, DCA2ExssAsset *asset);
int ff_dca2_xll_filter(DCA2XllDecoder *s);
av_cold void ff_dca2_xll_flush(DCA2XllDecoder *s);
av_cold void ff_dca2_xll_close(DCA2XllDecoder *s);

// ============================================================================

typedef struct DCA2Context {
    const AVClass   *class;       ///< class for AVOptions
    AVCodecContext  *avctx;

    DCA2CoreDecoder core;  ///< Core decoder context
    DCA2ExssParser  exss;  ///< EXSS parser context
    DCA2XllDecoder  xll;   ///< XLL decoder context

    uint8_t         *buffer;
    unsigned int    buffer_size;

    int     packet; ///< Packet flags

    int     has_residual_encoded;   ///< XLL residual encoded channels present
    int     core_residual_valid;    ///< Core valid for residual decoding

    int     request_channel_layout;
    int     core_only;
} DCA2Context;

int ff_dca2_set_channel_layout(AVCodecContext *avctx, int *ch_remap, int dca_mask);

int ff_dca2_check_crc(GetBitContext *s, int p1, int p2);

void ff_dca2_downmix_to_stereo_fixed(int **samples, int *coeff_l, int nsamples, int ch_mask);
void ff_dca2_downmix_to_stereo_float(AVFloatDSPContext *fdsp, float **samples,
                                     int *coeff_l, int nsamples, int ch_mask);

static inline int ff_dca2_seek_bits(GetBitContext *s, int p)
{
    if (p < s->index || p > s->size_in_bits)
        return -1;
    s->index = p;
    return 0;
}

#endif
