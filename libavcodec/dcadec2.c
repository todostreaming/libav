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

#include "dca.h"    // for ff_dca_convert_bitstream()
#include "dca2.h"
#include "dca2_math.h"

#define MIN_PACKET_SIZE     16
#define MAX_PACKET_SIZE     0x104000

int ff_dca2_set_channel_layout(AVCodecContext *avctx, int *ch_remap, int dca_mask)
{
    static const uint8_t dca2wav_norm[28] = {
         2,  0, 1, 9, 10,  3,  8,  4,  5,  9, 10, 6, 7, 12,
        13, 14, 3, 6,  7, 11, 12, 14, 16, 15, 17, 8, 4,  5,
    };

    static const uint8_t dca2wav_wide[28] = {
         2,  0, 1, 4,  5,  3,  8,  4,  5,  9, 10, 6, 7, 12,
        13, 14, 3, 9, 10, 11, 12, 14, 16, 15, 17, 8, 4,  5,
    };

    int dca_ch, wav_ch, nchannels = 0;

    if (avctx->request_channel_layout & AV_CH_LAYOUT_NATIVE) {
        for (dca_ch = 0; dca_ch < DCA2_SPEAKER_COUNT; dca_ch++)
            if (dca_mask & (1U << dca_ch))
                ch_remap[nchannels++] = dca_ch;
        avctx->channel_layout = dca_mask;
    } else {
        int wav_mask = 0;
        int wav_map[18];
        const uint8_t *dca2wav;
        if (dca_mask == DCA2_SPEAKER_LAYOUT_7POINT0_WIDE ||
            dca_mask == DCA2_SPEAKER_LAYOUT_7POINT1_WIDE)
            dca2wav = dca2wav_wide;
        else
            dca2wav = dca2wav_norm;
        for (dca_ch = 0; dca_ch < 28; dca_ch++) {
            if (dca_mask & (1 << dca_ch)) {
                wav_ch = dca2wav[dca_ch];
                if (!(wav_mask & (1 << wav_ch))) {
                    wav_map[wav_ch] = dca_ch;
                    wav_mask |= 1 << wav_ch;
                }
            }
        }
        for (wav_ch = 0; wav_ch < 18; wav_ch++)
            if (wav_mask & (1 << wav_ch))
                ch_remap[nchannels++] = wav_map[wav_ch];
        avctx->channel_layout = wav_mask;
    }

    avctx->channels = nchannels;
    return nchannels;
}

static uint16_t crc16(const uint8_t *data, int size)
{
    static const uint16_t crctab[16] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    };

    uint16_t res = 0xffff;
    for (int i = 0; i < size; i++) {
        res = (res << 4) ^ crctab[(data[i] >> 4) ^ (res >> 12)];
        res = (res << 4) ^ crctab[(data[i] & 15) ^ (res >> 12)];
    }

    return res;
}

int ff_dca2_check_crc(GetBitContext *s, int p1, int p2)
{
    if (((p1 | p2) & 7) || p1 < 0 || p2 > s->size_in_bits || p2 - p1 < 16)
        return -1;
    if (crc16(s->buffer + p1 / 8, (p2 - p1) / 8))
        return -1;
    return 0;
}

void ff_dca2_downmix_to_stereo_fixed(int **samples, int *coeff_l, int nsamples, int ch_mask)
{
    int pos, spkr, max_spkr = av_log2(ch_mask);
    int *coeff_r = coeff_l + av_popcount(ch_mask);

    av_assert0((ch_mask & DCA2_SPEAKER_LAYOUT_STEREO) == DCA2_SPEAKER_LAYOUT_STEREO);

    // Scale left and right channels
    pos = (ch_mask & DCA2_SPEAKER_MASK_C);
    vmul15(samples[DCA2_SPEAKER_L], coeff_l[pos    ], nsamples);
    vmul15(samples[DCA2_SPEAKER_R], coeff_r[pos + 1], nsamples);

    // Downmix remaining channels
    for (spkr = 0; spkr <= max_spkr; spkr++) {
        if (!(ch_mask & (1U << spkr)))
            continue;

        if (*coeff_l && spkr != DCA2_SPEAKER_L)
            vmul15_add(samples[DCA2_SPEAKER_L], samples[spkr], *coeff_l, nsamples);

        if (*coeff_r && spkr != DCA2_SPEAKER_R)
            vmul15_add(samples[DCA2_SPEAKER_R], samples[spkr], *coeff_r, nsamples);

        coeff_l++;
        coeff_r++;
    }
}

void ff_dca2_downmix_to_stereo_float(AVFloatDSPContext *fdsp, float **samples,
                                     int *coeff_l, int nsamples, int ch_mask)
{
    int pos, spkr, max_spkr = av_log2(ch_mask);
    int *coeff_r = coeff_l + av_popcount(ch_mask);

    av_assert0((ch_mask & DCA2_SPEAKER_LAYOUT_STEREO) == DCA2_SPEAKER_LAYOUT_STEREO);

    // Scale left and right channels
    pos = (ch_mask & DCA2_SPEAKER_MASK_C);
    fdsp->vector_fmul_scalar(samples[DCA2_SPEAKER_L],
                             samples[DCA2_SPEAKER_L],
                             coeff_l[pos    ] * (1.0f / (1 << 15)),
                             nsamples);
    fdsp->vector_fmul_scalar(samples[DCA2_SPEAKER_R],
                             samples[DCA2_SPEAKER_R],
                             coeff_r[pos + 1] * (1.0f / (1 << 15)),
                             nsamples);

    // Downmix remaining channels
    for (spkr = 0; spkr <= max_spkr; spkr++) {
        if (!(ch_mask & (1U << spkr)))
            continue;

        if (*coeff_l && spkr != DCA2_SPEAKER_L)
            fdsp->vector_fmac_scalar(samples[DCA2_SPEAKER_L],
                                     samples[spkr],
                                     *coeff_l * (1.0f / (1 << 15)),
                                     nsamples);

        if (*coeff_r && spkr != DCA2_SPEAKER_R)
            fdsp->vector_fmac_scalar(samples[DCA2_SPEAKER_R],
                                     samples[spkr],
                                     *coeff_r * (1.0f / (1 << 15)),
                                     nsamples);

        coeff_l++;
        coeff_r++;
    }
}

static int filter_core_frame(DCA2Context *s, AVFrame *frame)
{
    int ret = ff_dca2_core_filter_frame(&s->core, frame);

    if (ret < 0) {
        s->core_residual_valid = 0;
        return ret;
    }

    s->core_residual_valid = !!(s->avctx->flags & AV_CODEC_FLAG_BITEXACT);
    return 0;
}

static int filter_hd_ma_frame(DCA2Context *s, AVFrame *frame)
{
    AVCodecContext *avctx = s->avctx;
    DCA2XllChSet *p = &s->xll.chset[0];
    enum AVMatrixEncoding matrix_encoding = AV_MATRIX_ENCODING_NONE;
    int i, k, ret, shift, nsamples, request_mask;
    int ch_remap[DCA2_SPEAKER_COUNT];

    if (s->packet & DCA2_PACKET_CORE) {
        int x96_synth = 0;

        if (p->freq == 96000 && s->core.sample_rate == 48000)
            x96_synth = 1;

        if ((ret = ff_dca2_core_filter_fixed(&s->core, x96_synth)) < 0) {
            s->core_residual_valid = 0;
            return ret;
        }

        if (!s->core_residual_valid) {
            if (s->has_residual_encoded && s->xll.nchsets > 1)
                s->packet |= DCA2_PACKET_RECOVERY;
            s->core_residual_valid = 1;
        }
    }

    if ((ret = ff_dca2_xll_filter(&s->xll)) < 0)
        return ret;

    // Handle downmixing to stereo request
    if (s->request_channel_layout == DCA2_SPEAKER_LAYOUT_STEREO
        && (s->xll.output_mask & DCA2_SPEAKER_LAYOUT_STEREO) == DCA2_SPEAKER_LAYOUT_STEREO
        && p->dmix_embedded && (p->dmix_type == DCA2_DMIX_TYPE_LoRo ||
                                p->dmix_type == DCA2_DMIX_TYPE_LtRt))
        request_mask = DCA2_SPEAKER_LAYOUT_STEREO;
    else
        request_mask = s->xll.output_mask;
    if (!ff_dca2_set_channel_layout(avctx, ch_remap, request_mask))
        return AVERROR(EINVAL);

    avctx->sample_rate = p->freq << (s->xll.nfreqbands - 1);

    switch (p->storage_bit_res) {
    case 16:
        avctx->sample_fmt = AV_SAMPLE_FMT_S16P;
        break;
    case 24:
        avctx->sample_fmt = AV_SAMPLE_FMT_S32P;
        break;
    default:
        return AVERROR(EINVAL);
    }

    avctx->bits_per_raw_sample = p->storage_bit_res;
    avctx->profile = FF_PROFILE_DTS_HD_MA;
    avctx->bit_rate = 0;

    frame->nb_samples = nsamples = s->xll.nframesamples << (s->xll.nfreqbands - 1);
    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    // Downmix primary channel set to stereo
    if (request_mask != s->xll.output_mask) {
        ff_dca2_downmix_to_stereo_fixed(s->xll.output_samples, p->dmix_coeff,
                                        nsamples, s->xll.output_mask);
    }

    shift = p->storage_bit_res - p->pcm_bit_res;
    for (i = 0; i < avctx->channels; i++) {
        int32_t *samples = s->xll.output_samples[ch_remap[i]];
        if (frame->format == AV_SAMPLE_FMT_S16P) {
            int16_t *plane = (int16_t *)frame->extended_data[i];
            for (k = 0; k < nsamples; k++)
                plane[k] = av_clip_int16(samples[k] * (1 << shift));
        } else {
            int32_t *plane = (int32_t *)frame->extended_data[i];
            for (k = 0; k < nsamples; k++)
                plane[k] = clip23(samples[k] * (1 << shift)) * (1 << 8);
        }
    }

    if (!s->exss.assets[0].one_to_one_map_ch_to_spkr) {
        if (s->exss.assets[0].representation_type == DCA2_REPR_TYPE_LtRt)
            matrix_encoding = AV_MATRIX_ENCODING_DOLBY;
        else if (s->exss.assets[0].representation_type == DCA2_REPR_TYPE_LhRh)
            matrix_encoding = AV_MATRIX_ENCODING_DOLBYHEADPHONE;
    } else if (request_mask != s->xll.output_mask && p->dmix_type == DCA2_DMIX_TYPE_LtRt) {
        matrix_encoding = AV_MATRIX_ENCODING_DOLBY;
    }
    if ((ret = ff_side_data_update_matrix_encoding(frame, matrix_encoding)) < 0)
        return ret;

    return 0;
}

// Verify that core is compatible if there are residual encoded channel sets
static int validate_hd_ma_frame(DCA2Context *s)
{
    DCA2XllChSet *p = &s->xll.chset[0], *c;
    int i, ch, ch_mask;

    s->has_residual_encoded = 0;
    for (i = 0, c = s->xll.chset, ch_mask = 0; i < s->xll.nactivechsets; i++, c++) {
        if (ch_mask & c->ch_mask) {
            av_log(s->avctx, AV_LOG_WARNING, "Channel masks overlap between XLL channel sets\n");
            return AVERROR_INVALIDDATA;
        }

        if (c->residual_encode != (1 << c->nchannels) - 1) {
            if (!(s->packet & DCA2_PACKET_CORE)) {
                av_log(s->avctx, AV_LOG_WARNING, "Residual encoded channels are present without core\n");
                return AVERROR_INVALIDDATA;
            }

            for (ch = 0; ch < c->nchannels; ch++) {
                if (ff_dca2_core_map_spkr(&s->core, c->ch_remap[ch]) < 0) {
                    av_log(s->avctx, AV_LOG_WARNING, "Residual encoded channel (%d) references unavailable core channel\n", c->ch_remap[ch]);
                    return AVERROR_INVALIDDATA;
                }
            }

            s->has_residual_encoded = 1;
        }

        ch_mask |= c->ch_mask;
    }

    if (s->has_residual_encoded) {
        int rate = s->core.sample_rate;
        int nsamples = s->core.npcmblocks * DCA2_PCMBLOCK_SAMPLES;

        // Double sampling frequency if needed
        if (p->freq == 96000 && rate == 48000) {
            rate *= 2;
            nsamples *= 2;
        }

        if (p->freq != rate) {
            av_log(s->avctx, AV_LOG_WARNING, "Sample rate mismatch between core (%d) and XLL (%d)\n", rate, p->freq);
            return AVERROR_INVALIDDATA;
        }

        if (s->xll.nframesamples != nsamples) {
            av_log(s->avctx, AV_LOG_WARNING, "Number of samples per frame mismatch between core (%d) and XLL (%d)\n", nsamples, s->xll.nframesamples);
            return AVERROR_INVALIDDATA;
        }
    }

    return 0;
}

static int dcadec_decode_frame(AVCodecContext *avctx, void *data,
                               int *got_frame_ptr, AVPacket *avpkt)
{
    DCA2Context *s = avctx->priv_data;
    AVFrame *frame = data;
    uint8_t *input = avpkt->data;
    int input_size = avpkt->size;
    int i, ret, prev_packet = s->packet;

    if (input_size < MIN_PACKET_SIZE || input_size > MAX_PACKET_SIZE) {
        av_log(avctx, AV_LOG_ERROR, "Invalid packet size\n");
        return AVERROR_INVALIDDATA;
    }

    av_fast_malloc(&s->buffer, &s->buffer_size,
                   FFALIGN(input_size, 4096) + DCA2_BUFFER_PADDING_SIZE);
    if (!s->buffer)
        return AVERROR(ENOMEM);

    for (i = 0, ret = AVERROR_INVALIDDATA; i < input_size - MIN_PACKET_SIZE + 1 && ret < 0; i++)
        ret = ff_dca_convert_bitstream(input + i, input_size - i, s->buffer, s->buffer_size);

    if (ret < 0)
        return ret;

    input      = s->buffer;
    input_size = ret;

    s->packet = 0;

    // Parse backward compatible core sub-stream
    if (AV_RB32(input) == DCA_SYNCWORD_CORE_BE) {
        int frame_size;

        if ((ret = ff_dca2_core_parse(&s->core, input, input_size)) < 0) {
            s->core_residual_valid = 0;
            return ret;
        }

        s->packet |= DCA2_PACKET_CORE;

        // EXXS data must be aligned on 4-byte boundary
        frame_size = FFALIGN(s->core.frame_size, 4);
        if (input_size - 4 > frame_size) {
            input      += frame_size;
            input_size -= frame_size;
        }
    }

    if (!s->core_only) {
        DCA2ExssAsset *asset = NULL;

        // Parse extension sub-stream (EXSS)
        if (AV_RB32(input) == DCA_SYNCWORD_SUBSTREAM) {
            if ((ret = ff_dca2_exss_parse(&s->exss, input, input_size)) < 0) {
                if (avctx->err_recognition & AV_EF_EXPLODE)
                    return ret;
            } else {
                asset = &s->exss.assets[0];
            }
        }

        // Parse XLL component in EXSS
        if (asset && (asset->extension_mask & DCA2_EXSS_XLL)) {
            if ((ret = ff_dca2_xll_parse(&s->xll, input, asset)) < 0) {
                // Conceal XLL synchronization error
                if (ret == AVERROR(EAGAIN) &&
                    (prev_packet & DCA2_PACKET_XLL) &&
                    (s->packet & DCA2_PACKET_CORE)) {
                    s->packet |= DCA2_PACKET_XLL | DCA2_PACKET_RECOVERY;
                } else {
                    if (avctx->err_recognition & AV_EF_EXPLODE)
                        return ret;
                }
            } else {
                s->packet |= DCA2_PACKET_XLL;
            }
        }

        // Parse core extensions in EXSS or backward compatible core sub-stream
        if ((s->packet & DCA2_PACKET_CORE)
            && (ret = ff_dca2_core_parse_exss(&s->core, input, asset)) < 0)
            return ret;
    }

    // Filter
    if (s->packet & DCA2_PACKET_XLL) {
        if ((ret = validate_hd_ma_frame(s)) < 0) {
            if (avctx->err_recognition & AV_EF_EXPLODE)
                return ret;
            if (!(s->packet & DCA2_PACKET_CORE))
                return ret;
            if ((ret = filter_core_frame(s, frame)) < 0)
                return ret;
        } else {
            if ((ret = filter_hd_ma_frame(s, frame)) < 0)
                return ret;
        }
    } else if (s->packet & DCA2_PACKET_CORE) {
        if ((ret = filter_core_frame(s, frame)) < 0)
            return ret;
    } else {
        return AVERROR(EINVAL);
    }

    *got_frame_ptr = 1;

    return avpkt->size;
}

static av_cold void dcadec_flush(AVCodecContext *avctx)
{
    DCA2Context *s = avctx->priv_data;

    ff_dca2_core_flush(&s->core);
    ff_dca2_xll_flush(&s->xll);

    s->core_residual_valid = 0;
}

static av_cold int dcadec_close(AVCodecContext *avctx)
{
    DCA2Context *s = avctx->priv_data;

    ff_dca2_core_close(&s->core);
    ff_dca2_xll_close(&s->xll);

    av_freep(&s->buffer);
    s->buffer_size = 0;

    return 0;
}

static av_cold int dcadec_init(AVCodecContext *avctx)
{
    DCA2Context *s = avctx->priv_data;
    int ret;

    s->avctx = avctx;
    s->core.avctx = avctx;
    s->exss.avctx = avctx;
    s->xll.avctx = avctx;

    if ((ret = ff_dca2_core_init(&s->core)) < 0)
        return ret;

    switch (avctx->request_channel_layout & ~AV_CH_LAYOUT_NATIVE) {
    case 0:
        s->request_channel_layout = 0;
        break;
    case AV_CH_LAYOUT_STEREO:
    case AV_CH_LAYOUT_STEREO_DOWNMIX:
        s->request_channel_layout = DCA2_SPEAKER_LAYOUT_STEREO;
        break;
    case AV_CH_LAYOUT_5POINT0:
        s->request_channel_layout = DCA2_SPEAKER_LAYOUT_5POINT0;
        break;
    case AV_CH_LAYOUT_5POINT1:
        s->request_channel_layout = DCA2_SPEAKER_LAYOUT_5POINT1;
        break;
    default:
        av_log(avctx, AV_LOG_WARNING, "Invalid request_channel_layout\n");
        break;
    }

    avctx->sample_fmt = AV_SAMPLE_FMT_S32P;
    avctx->bits_per_raw_sample = 24;

    return 0;
}

#define OFFSET(x) offsetof(DCA2Context, x)
#define PARAM AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_DECODING_PARAM

static const AVOption dcadec_options[] = {
    { "core_only",  "Decode core only without extensions", OFFSET(core_only), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 1, PARAM },
    { NULL }
};

static const AVClass dcadec_class = {
    .class_name = "DCA-2 decoder",
    .item_name  = av_default_item_name,
    .option     = dcadec_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_dca2_decoder = {
    .name           = "dca2",
    .long_name      = NULL_IF_CONFIG_SMALL("DCA-2 (DTS Coherent Acoustics)"),
    .type           = AVMEDIA_TYPE_AUDIO,
    .id             = AV_CODEC_ID_DTS,
    .priv_data_size = sizeof(DCA2Context),
    .init           = dcadec_init,
    .decode         = dcadec_decode_frame,
    .close          = dcadec_close,
    .flush          = dcadec_flush,
    .capabilities   = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_CHANNEL_CONF,
    .sample_fmts    = (const enum AVSampleFormat[]) { AV_SAMPLE_FMT_S16P, AV_SAMPLE_FMT_S32P,
                                                      AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE },
    .priv_class     = &dcadec_class,
    .profiles       = NULL_IF_CONFIG_SMALL(ff_dca_profiles),
};
