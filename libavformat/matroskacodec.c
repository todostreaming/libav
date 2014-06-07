/*
 * Matroska codec-specific muxer functions
 * Copyright (c) 2007 David Conrad
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

#include "avc.h"
#include "avformat.h"
#include "flacenc.h"
#include "hevc.h"
#include "isom.h"
#include "matroska.h"
#include "matroskaenc.h"
#include "riff.h"
#include "vorbiscomment.h"
#include "wv.h"

#include "libavcodec/mpeg4audio.h"
#include "libavcodec/xiph.h"

#include "libavutil/intreadwrite.h"

void ff_get_aac_sample_rates(AVFormatContext *s, AVCodecContext *codec,
                             int *sample_rate, int *output_sample_rate)
{
    MPEG4AudioConfig mp4ac;

    if (avpriv_mpeg4audio_get_config(&mp4ac, codec->extradata,
                                     codec->extradata_size * 8, 1) < 0) {
        av_log(s, AV_LOG_WARNING,
               "Error parsing AAC extradata, unable to determine samplerate.\n");
        return;
    }

    *sample_rate        = mp4ac.sample_rate;
    *output_sample_rate = mp4ac.ext_sample_rate;
}

static int mkv_strip_wavpack(const uint8_t *src, uint8_t **pdst, int *size)
{
    uint8_t *dst;
    int srclen = *size;
    int offset = 0;
    int ret;

    dst = av_malloc(srclen);
    if (!dst)
        return AVERROR(ENOMEM);

    while (srclen >= WV_HEADER_SIZE) {
        WvHeader header;

        ret = ff_wv_parse_header(&header, src);
        if (ret < 0)
            goto fail;
        src    += WV_HEADER_SIZE;
        srclen -= WV_HEADER_SIZE;

        if (srclen < header.blocksize) {
            ret = AVERROR_INVALIDDATA;
            goto fail;
        }

        if (header.initial) {
            AV_WL32(dst + offset, header.samples);
            offset += 4;
        }
        AV_WL32(dst + offset,     header.flags);
        AV_WL32(dst + offset + 4, header.crc);
        offset += 8;

        if (!(header.initial && header.final)) {
            AV_WL32(dst + offset, header.blocksize);
            offset += 4;
        }

        memcpy(dst + offset, src, header.blocksize);
        src    += header.blocksize;
        srclen -= header.blocksize;
        offset += header.blocksize;
    }

    *pdst = dst;
    *size = offset;

    return 0;
fail:
    av_freep(&dst);
    return ret;
}

void ff_mkv_write_block(AVFormatContext *s, AVIOContext *pb,
                        unsigned int blockid, AVPacket *pkt, int flags)
{
    MatroskaMuxContext *mkv = s->priv_data;
    AVCodecContext *codec = s->streams[pkt->stream_index]->codec;
    uint8_t *data = NULL;
    int offset = 0, size = pkt->size;
    int64_t ts = mkv->tracks[pkt->stream_index].write_dts ? pkt->dts : pkt->pts;

    av_log(s, AV_LOG_DEBUG, "Writing block at offset %" PRIu64 ", size %d, "
           "pts %" PRId64 ", dts %" PRId64 ", duration %d, flags %d\n",
           avio_tell(pb), pkt->size, pkt->pts, pkt->dts, pkt->duration, flags);
    if (codec->codec_id == AV_CODEC_ID_H264 && codec->extradata_size > 0 &&
        (AV_RB24(codec->extradata) == 1 || AV_RB32(codec->extradata) == 1))
        ff_avc_parse_nal_units_buf(pkt->data, &data, &size);
    else if (codec->codec_id == AV_CODEC_ID_HEVC && codec->extradata_size > 6 &&
             (AV_RB24(codec->extradata) == 1 || AV_RB32(codec->extradata) == 1))
        /* extradata is Annex B, assume the bitstream is too and convert it */
        ff_hevc_annexb2mp4_buf(pkt->data, &data, &size, 0, NULL);
    else if (codec->codec_id == AV_CODEC_ID_WAVPACK) {
        int ret = mkv_strip_wavpack(pkt->data, &data, &size);
        if (ret < 0) {
            av_log(s, AV_LOG_ERROR, "Error stripping a WavPack packet.\n");
            return;
        }
    } else
        data = pkt->data;

    if (codec->codec_id == AV_CODEC_ID_PRORES) {
        /* Matroska specification requires to remove the first QuickTime atom
         */
        size  -= 8;
        offset = 8;
    }

    ff_put_ebml_id(pb, blockid);
    ff_put_ebml_num(pb, size + 4, 0);
    // this assumes stream_index is less than 126
    avio_w8(pb, 0x80 | (pkt->stream_index + 1));
    avio_wb16(pb, ts - mkv->cluster_pts);
    avio_w8(pb, flags);
    avio_write(pb, data + offset, size);
    if (data != pkt->data)
        av_free(data);
}

static void put_xiph_size(AVIOContext *pb, int size)
{
    int i;
    for (i = 0; i < size / 255; i++)
        avio_w8(pb, 255);
    avio_w8(pb, size % 255);
}

static int put_xiph_codecpriv(AVFormatContext *s, AVIOContext *pb, AVCodecContext *codec)
{
    uint8_t *header_start[3];
    int header_len[3];
    int first_header_size;
    int j;

    if (codec->codec_id == AV_CODEC_ID_VORBIS)
        first_header_size = 30;
    else
        first_header_size = 42;

    if (avpriv_split_xiph_headers(codec->extradata, codec->extradata_size,
                              first_header_size, header_start, header_len) < 0) {
        av_log(s, AV_LOG_ERROR, "Extradata corrupt.\n");
        return -1;
    }

    avio_w8(pb, 2);                    // number packets - 1
    for (j = 0; j < 2; j++) {
        put_xiph_size(pb, header_len[j]);
    }
    for (j = 0; j < 3; j++)
        avio_write(pb, header_start[j], header_len[j]);

    return 0;
}

static int put_wv_codecpriv(AVIOContext *pb, AVCodecContext *codec)
{
    if (codec->extradata && codec->extradata_size == 2)
        avio_write(pb, codec->extradata, 2);
    else
        avio_wl16(pb, 0x403); // fallback to the version mentioned in matroska specs
    return 0;
}

static int put_flac_codecpriv(AVFormatContext *s,
                              AVIOContext *pb, AVCodecContext *codec)
{
    int write_comment = (codec->channel_layout &&
                         !(codec->channel_layout & ~0x3ffffULL) &&
                         !ff_flac_is_native_layout(codec->channel_layout));
    int ret = ff_flac_write_header(pb, codec->extradata, codec->extradata_size,
                                   !write_comment);

    if (ret < 0)
        return ret;

    if (write_comment) {
        const char *vendor = (s->flags & AVFMT_FLAG_BITEXACT) ?
                             "Libav" : LIBAVFORMAT_IDENT;
        AVDictionary *dict = NULL;
        uint8_t buf[32], *data, *p;
        int len;

        snprintf(buf, sizeof(buf), "0x%"PRIx64, codec->channel_layout);
        av_dict_set(&dict, "WAVEFORMATEXTENSIBLE_CHANNEL_MASK", buf, 0);

        len = ff_vorbiscomment_length(dict, vendor);
        data = av_malloc(len + 4);
        if (!data) {
            av_dict_free(&dict);
            return AVERROR(ENOMEM);
        }

        data[0] = 0x84;
        AV_WB24(data + 1, len);

        p = data + 4;
        ff_vorbiscomment_write(&p, &dict, vendor);

        avio_write(pb, data, len + 4);

        av_freep(&data);
        av_dict_free(&dict);
    }

    return 0;
}

static int mkv_write_native_codecprivate(AVFormatContext *s,
                                         AVCodecContext *codec,
                                         AVIOContext *dyn_cp)
{
    switch (codec->codec_id) {
    case AV_CODEC_ID_VORBIS:
    case AV_CODEC_ID_THEORA:
        return put_xiph_codecpriv(s, dyn_cp, codec);
    case AV_CODEC_ID_FLAC:
        return put_flac_codecpriv(s, dyn_cp, codec);
    case AV_CODEC_ID_WAVPACK:
        return put_wv_codecpriv(dyn_cp, codec);
    case AV_CODEC_ID_H264:
        return ff_isom_write_avcc(dyn_cp, codec->extradata,
                                  codec->extradata_size);
    case AV_CODEC_ID_HEVC:
        return ff_isom_write_hvcc(dyn_cp, codec->extradata,
                                  codec->extradata_size, 0);
    case AV_CODEC_ID_ALAC:
        if (codec->extradata_size < 36) {
            av_log(s, AV_LOG_ERROR,
                   "Invalid extradata found, ALAC expects a 36-byte "
                   "QuickTime atom.");
            return AVERROR_INVALIDDATA;
        } else
            avio_write(dyn_cp, codec->extradata + 12,
                       codec->extradata_size - 12);
        break;
    default:
        if (codec->extradata_size)
        avio_write(dyn_cp, codec->extradata, codec->extradata_size);
    }

    return 0;
}

int ff_mkv_write_codecprivate(AVFormatContext *s, AVIOContext *pb,
                              AVCodecContext *codec, int native_id, int qt_id)
{
    AVIOContext *dyn_cp;
    uint8_t *codecpriv;
    int ret, codecpriv_size;

    ret = avio_open_dyn_buf(&dyn_cp);
    if (ret < 0)
        return ret;

    if (native_id) {
        ret = mkv_write_native_codecprivate(s, codec, dyn_cp);
    } else if (codec->codec_type == AVMEDIA_TYPE_VIDEO) {
        if (qt_id) {
            if (!codec->codec_tag)
                codec->codec_tag = ff_codec_get_tag(ff_codec_movvideo_tags,
                                                    codec->codec_id);
            if (codec->extradata_size)
                avio_write(dyn_cp, codec->extradata, codec->extradata_size);
        } else {
            if (!codec->codec_tag)
                codec->codec_tag = ff_codec_get_tag(ff_codec_bmp_tags,
                                                    codec->codec_id);
            if (!codec->codec_tag) {
                av_log(s, AV_LOG_ERROR, "No bmp codec ID found.\n");
                ret = -1;
            }

            ff_put_bmp_header(dyn_cp, codec, ff_codec_bmp_tags, 0);
        }
    } else if (codec->codec_type == AVMEDIA_TYPE_AUDIO) {
        unsigned int tag;
        tag = ff_codec_get_tag(ff_codec_wav_tags, codec->codec_id);
        if (!tag) {
            av_log(s, AV_LOG_ERROR, "No wav codec ID found.\n");
            ret = -1;
        }
        if (!codec->codec_tag)
            codec->codec_tag = tag;

        ff_put_wav_header(dyn_cp, codec);
    }

    codecpriv_size = avio_close_dyn_buf(dyn_cp, &codecpriv);
    if (codecpriv_size)
        ff_put_ebml_binary(pb, MATROSKA_ID_CODECPRIVATE, codecpriv,
                           codecpriv_size);
    av_free(codecpriv);
    return ret;
}
