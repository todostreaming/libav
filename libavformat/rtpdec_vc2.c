/*
 * RTP parser for VC2 payload format (draft version 0)
 * Copyright (c) 2016 Luca Barbato <lu_zero@gentoo.org>
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

#include "libavutil/avstring.h"
#include "libavutil/base64.h"
#include "libavutil/intreadwrite.h"

#include "avio.h"
#include "avio_internal.h"
#include "avformat.h"
#include "rtpdec.h"
#include "rtpdec_formats.h"

#define VC2_PAYLOAD_HEADER_SIZE  (2 + 1 + 1)
#define VC2_FRAGMENT_HEADER_SIZE (4 + 2 + 2)

#define VC2_SEQUENCE_HEADER  0x00
#define VC2_END_OF_SEQUENCE  0x01
#define VC2_PICTURE_FRAGMENT 0xEC
#define VC2_HQ_PICTURE       0xE8


struct PayloadContext {
    AVIOContext *buf;
    uint32_t     timestamp;
    uint32_t     last_offset;
    uint32_t     picture_number;
    uint32_t     size;
};

static uint8_t startcode[4] = { 0x42, 0x42, 0x43, 0x44 };

// TODO: expose it as extradata as well?
static int vc2_parse_sequence_header(AVFormatContext *s, PayloadContext *vc2,
                                     const uint8_t *buf, int len)
{
    int ret;
    uint32_t offset = len + 13 - 3;

    if (!vc2->buf) {
        ret = avio_open_dyn_buf(&vc2->buf);
        if (ret < 0)
            return ret;
    }

    avio_write(vc2->buf, startcode, sizeof(startcode));
    avio_w8(vc2->buf, VC2_SEQUENCE_HEADER);
    avio_wb32(vc2->buf, offset);
    avio_wb32(vc2->buf, vc2->last_offset);

    avio_write(vc2->buf, buf, len);

    vc2->last_offset = offset;

    return AVERROR(EAGAIN);
}

static int vc2_parse_end_of_sequence(AVFormatContext *s, PayloadContext *vc2)
{
    int ret;

    if (!vc2->buf) {
        ret = avio_open_dyn_buf(&vc2->buf);
        if (ret < 0)
            return ret;
    }

    avio_write(vc2->buf, startcode, sizeof(startcode));
    avio_w8(vc2->buf, VC2_END_OF_SEQUENCE);
    avio_wb32(vc2->buf, 0);
    avio_wb32(vc2->buf, vc2->last_offset);

    return AVERROR(EAGAIN);
}

static int vc2_parse_picture_fragment(AVFormatContext *s, PayloadContext *vc2,
                                      const uint8_t *buf, int len, int last,
                                      AVPacket *pkt, int index)
{
    int picture_number   = AV_RB32(buf);
    int fragment_length  = AV_RB16(buf + 4);
    int number_of_slices = AV_RB16(buf + 6);
    int ret;

    buf += VC2_FRAGMENT_HEADER_SIZE;
    len -= VC2_FRAGMENT_HEADER_SIZE;

    // TODO more sanity checks)
    if (fragment_length > len)
        return AVERROR_INVALIDDATA;

    if (!number_of_slices) {
        if (!vc2->buf) {
            ret = avio_open_dyn_buf(&vc2->buf);
            if (ret < 0)
                return ret;
        }
        avio_write(vc2->buf, startcode, sizeof(startcode));
        avio_w8(vc2->buf, VC2_HQ_PICTURE);
        avio_wb32(vc2->buf, 0);
        avio_wb32(vc2->buf, vc2->last_offset);
        avio_wb32(vc2->buf, picture_number);

        vc2->size = 0;
    }

    avio_write(vc2->buf, buf, fragment_length);

    vc2->size += fragment_length;

    if (last) {
        int64_t pos = avio_tell(vc2->buf);
        // patch up the values
        avio_seek(vc2->buf, 5, SEEK_SET);
        avio_wb32(vc2->buf, vc2->size);
        // seek back to the right position to correctly pad the packet on close
        avio_seek(vc2->buf, pos, SEEK_SET);

        ret = ff_rtp_finalize_packet(pkt, &vc2->buf, index);
    } else {
        ret = AVERROR(EAGAIN);
    }

    return ret;
}

static int vc2_handle_packet(AVFormatContext *s, PayloadContext *vc2,
                              AVStream *st, AVPacket *pkt, uint32_t *timestamp,
                              const uint8_t *buf, int len, uint16_t seq,
                              int flags)
{
    int last;
    int extended_seq, first_field, second_field, parse_code;
    int ret = 0;

    /* sanity check for size of input packet: 1 byte payload at least */
    if (len < VC2_PAYLOAD_HEADER_SIZE) {
        av_log(s, AV_LOG_ERROR, "Too short RTP/VC2 packet, got %d bytes\n", len);
        return AVERROR_INVALIDDATA;
    }

    /*
     * decode the VC2 payload header according to section 4 of draft version 0:
     *
     *   0                   1                   2                   3
     *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |   Extended Sequence Number    |  Reserved |I|F|  Parse Code   |
     *  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
     *  |                       Picture Number                          |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
     *  |       Fragment Length         |         No. of Slices         |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
     *
     *  Extended Sequence Number:
     *      Most significant 16bit of a 32bit sequence, the least significant
     *      16bit are stored in the standard Sequence Number field.
     *  Reserved/I:
     *      Set if the packet contains coded picture parameters or slice data
     *      from a field in an interlaced frame, unset otherwise.
     *  Reserved/F:
     *      Set if the packet contains coded picture parameters or slice data
     *      from the second field in an interlaced frame, unset otherwise.
     *  Parse Code:
     *      8 bits  Contains a Parse Code which MUST be the value indicated
     *      for the type of data in the packet.
     *  Picture Number:
     *      Same a Section 12.1 of the VC-2 specification.
     *  Frame Length:
     *      Number of bytes of data
     *  No. of Slices:
     *      The number of coded slices contained in this packet, which MUST
     *      be 0 for a packet containing transform parameters.
     */

    extended_seq = AV_RB16(buf);
    first_field  = !!(buf[1] & 1);
    second_field = !!(buf[1] & 2);
    parse_code   = buf[3];
    last         = !!(flags & RTP_FLAG_MARKER);

    av_log(s, AV_LOG_INFO, "seq %d, %d/%d, parse code 0x%x\n",
           extended_seq << 16 | seq, first_field, second_field, parse_code);

    buf += VC2_PAYLOAD_HEADER_SIZE;
    len -= VC2_PAYLOAD_HEADER_SIZE;

    switch (parse_code) {
    case VC2_SEQUENCE_HEADER:
        ret = vc2_parse_sequence_header(s, vc2, buf, len);
        if (ret < 0)
            return ret;
        return AVERROR(EAGAIN);
    case VC2_END_OF_SEQUENCE:
        ret = vc2_parse_end_of_sequence(s, vc2);
        if (ret < 0)
            return ret;
        break;
    case VC2_PICTURE_FRAGMENT:
        ret = vc2_parse_picture_fragment(s, vc2, buf, len, last, pkt, st->index);
        if (ret < 0)
            return ret;
        break;
    default:
        av_log(s, AV_LOG_ERROR, "Unsupported Parse Code (%d)\n", parse_code);
        return AVERROR_INVALIDDATA;
    }

    return ret;
}

static void vc2_close_context(PayloadContext *vc2)
{
    ffio_free_dyn_buf(&vc2->buf);
}

RTPDynamicProtocolHandler ff_vc2_dynamic_handler = {
    .enc_name         = "VC2",
    .codec_type       = AVMEDIA_TYPE_VIDEO,
    .codec_id         = AV_CODEC_ID_DIRAC,
    .need_parsing     = AVSTREAM_PARSE_FULL,
    .priv_data_size   = sizeof(PayloadContext),
    .close            = vc2_close_context,
    .parse_packet     = vc2_handle_packet,
};
