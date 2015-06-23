/*
 * H.26L/H.264/AVC/JVT/14496-10/... sei decoding
 * Copyright (c) 2003 Michael Niedermayer <michaelni@gmx.at>
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

/**
 * @file
 * H.264 / AVC / MPEG4 part10 sei decoding.
 * @author Michael Niedermayer <michaelni@gmx.at>
 */

#include "avcodec.h"
#include "golomb.h"
#include "h264.h"
#include "internal.h"

static const uint8_t sei_num_clock_ts_table[9] = {
    1, 1, 1, 2, 2, 3, 3, 2, 3
};

void ff_h264_reset_sei(H264Context *h)
{
    h->sei_recovery_frame_cnt       = -1;
    h->sei_dpb_output_delay         =  0;
    h->sei_cpb_removal_delay        = -1;
    h->sei_buffering_period_present =  0;
    h->sei_frame_packing_present    =  0;
    h->sei_display_orientation_present = 0;
    h->sei_reguserdata_afd_present  =  0;

    h->a53_caption_size = 0;
    av_freep(&h->a53_caption);
}

static int decode_picture_timing(H264Context *h)
{
    if (h->sps.nal_hrd_parameters_present_flag ||
        h->sps.vcl_hrd_parameters_present_flag) {
        h->sei_cpb_removal_delay = get_bits(&h->gb,
                                            h->sps.cpb_removal_delay_length);
        h->sei_dpb_output_delay  = get_bits(&h->gb,
                                            h->sps.dpb_output_delay_length);
    }
    if (h->sps.pic_struct_present_flag) {
        unsigned int i, num_clock_ts;

        h->sei_pic_struct = get_bits(&h->gb, 4);
        h->sei_ct_type    = 0;

        if (h->sei_pic_struct > SEI_PIC_STRUCT_FRAME_TRIPLING)
            return AVERROR_INVALIDDATA;

        num_clock_ts = sei_num_clock_ts_table[h->sei_pic_struct];

        for (i = 0; i < num_clock_ts; i++) {
            if (get_bits(&h->gb, 1)) {                /* clock_timestamp_flag */
                unsigned int full_timestamp_flag;

                h->sei_ct_type |= 1 << get_bits(&h->gb, 2);
                skip_bits(&h->gb, 1);                 /* nuit_field_based_flag */
                skip_bits(&h->gb, 5);                 /* counting_type */
                full_timestamp_flag = get_bits(&h->gb, 1);
                skip_bits(&h->gb, 1);                 /* discontinuity_flag */
                skip_bits(&h->gb, 1);                 /* cnt_dropped_flag */
                skip_bits(&h->gb, 8);                 /* n_frames */
                if (full_timestamp_flag) {
                    skip_bits(&h->gb, 6);             /* seconds_value 0..59 */
                    skip_bits(&h->gb, 6);             /* minutes_value 0..59 */
                    skip_bits(&h->gb, 5);             /* hours_value 0..23 */
                } else {
                    if (get_bits(&h->gb, 1)) {        /* seconds_flag */
                        skip_bits(&h->gb, 6);         /* seconds_value range 0..59 */
                        if (get_bits(&h->gb, 1)) {    /* minutes_flag */
                            skip_bits(&h->gb, 6);     /* minutes_value 0..59 */
                            if (get_bits(&h->gb, 1))  /* hours_flag */
                                skip_bits(&h->gb, 5); /* hours_value 0..23 */
                        }
                    }
                }
                if (h->sps.time_offset_length > 0)
                    skip_bits(&h->gb,
                              h->sps.time_offset_length); /* time_offset */
            }
        }

        if (h->avctx->debug & FF_DEBUG_PICT_INFO)
            av_log(h->avctx, AV_LOG_DEBUG, "ct_type:%X pic_struct:%d\n",
                   h->sei_ct_type, h->sei_pic_struct);
    }
    return 0;
}

static int decode_registered_user_data_afd(H264Context *h, int size)
{
    int flag;

    if (size-- < 1)
        return AVERROR_INVALIDDATA;
    skip_bits(&h->gb, 1);               // 0
    flag = get_bits(&h->gb, 1);         // active_format_flag
    skip_bits(&h->gb, 6);               // reserved

    if (flag) {
        if (size-- < 1)
            return AVERROR_INVALIDDATA;
        skip_bits(&h->gb, 4);           // reserved
        h->active_format_description   = get_bits(&h->gb, 4);
        h->sei_reguserdata_afd_present = 1;
    }

    return 0;
}

static int decode_registered_user_data_closed_caption(H264Context *h, int size)
{
    int flag;
    int user_data_type_code;
    int cc_count;

    if (size < 3)
        return AVERROR(EINVAL);

    user_data_type_code = get_bits(&h->gb, 8);
    if (user_data_type_code == 0x3) {
        skip_bits(&h->gb, 1);           // reserved

        flag = get_bits(&h->gb, 1);     // process_cc_data_flag
        if (flag) {
            skip_bits(&h->gb, 1);       // zero bit
            cc_count = get_bits(&h->gb, 5);
            skip_bits(&h->gb, 8);       // reserved
            size -= 2;

            if (cc_count && size >= cc_count * 3) {
                const uint64_t new_size = (h->a53_caption_size + cc_count
                                           * UINT64_C(3));
                int i, ret;

                if (new_size > INT_MAX)
                    return AVERROR(EINVAL);

                /* Allow merging of the cc data from two fields. */
                ret = av_reallocp(&h->a53_caption, new_size);
                if (ret < 0)
                    return ret;

                for (i = 0; i < cc_count; i++) {
                    h->a53_caption[h->a53_caption_size++] = get_bits(&h->gb, 8);
                    h->a53_caption[h->a53_caption_size++] = get_bits(&h->gb, 8);
                    h->a53_caption[h->a53_caption_size++] = get_bits(&h->gb, 8);
                }

                skip_bits(&h->gb, 8);   // marker_bits
            }
        }
    } else {
        int i;
        avpriv_request_sample(h->avctx, "Subtitles with data type 0x%02x",
                              user_data_type_code);
        for (i = 0; i < size - 1; i++)
            skip_bits(&h->gb, 8);
    }

    return 0;
}

static int decode_registered_user_data(H264Context *h, int size)
{
    uint32_t country_code;
    uint32_t user_identifier;

    if (size < 7)
        return AVERROR_INVALIDDATA;
    size -= 7;

    country_code = get_bits(&h->gb, 8); // itu_t_t35_country_code
    if (country_code == 0xFF) {
        skip_bits(&h->gb, 8);           // itu_t_t35_country_code_extension_byte
        size--;
    }

    /* itu_t_t35_payload_byte follows */
    skip_bits(&h->gb, 8);              // terminal provider code
    skip_bits(&h->gb, 8);              // terminal provider oriented code
    user_identifier = get_bits_long(&h->gb, 32);

    switch (user_identifier) {
        case MKBETAG('D', 'T', 'G', '1'):       // afd_data
            return decode_registered_user_data_afd(h, size);
        case MKBETAG('G', 'A', '9', '4'):       // closed captions
            return decode_registered_user_data_closed_caption(h, size);
        default:
            skip_bits(&h->gb, size * 8);
            break;
    }

    return 0;
}

static const uint8_t x264_version_uuid[] = {
    0xdc, 0x45, 0xe9, 0xbd, 0xe6, 0xd9, 0x48, 0xb7,
    0x96, 0x2c, 0xd8, 0x20, 0xd9, 0x23, 0xee, 0xef
};

static int decode_x264_version(H264Context *h, int size)
{
    int e, build, i;
    uint8_t x264_string[256];


    for (i = 0; i < size && i < sizeof(x264_string) - 1; i++)
        x264_string[i] = get_bits(&h->gb, 8) & 0x7f;

    x264_string[i] = 0;

    e = sscanf(x264_string, "x264 - core %d", &build);
    if (e == 1 && build > 0)
         h->x264_build = build;

    if (h->avctx->debug & FF_DEBUG_BUGS)
        av_log(h->avctx, AV_LOG_DEBUG, "x264 version string:\"%s\"\n", x264_string + 16);

    for (; i < size; i++)
        skip_bits(&h->gb, 8);

    return 0;
}

static const uint8_t vanc_uuid[] = {
    0x9c, 0x3c, 0x26, 0x8c, 0x76, 0x32, 0x95, 0x07,
    0x0d, 0x7a, 0xd4, 0x5a, 0x95, 0x7e, 0xd3, 0xb8
};

static int decode_vanc(H264Context *h, int size)
{
    int i, ret;
    uint8_t *p;

    ret = av_reallocp(&h->sei_vanc, size + 1);
    if (ret < 0)
        return ret;

    p = h->sei_vanc;

    for (i = 0; i < size; i++)
        p[i] = get_bits(&h->gb, 8);

    h->sei_vanc_size = size;

    return 0;
}

static const uint8_t wall_uuid[] = { 0x8c, 0x2c, 0x26, 0x8c,
                                     0x76, 0x32, 0x95, 0x07,
                                     0x0d, 0x7a, 0xd2, 0x5a,
                                     0x95, 0x7e, 0xd3, 0xb7 };

static int decode_wall(H264Context *h, int size)
{
    int i, ret;
    uint8_t *p;

    ret = av_reallocp(&h->sei_wall, size + 1);
    if (ret < 0)
        return ret;

    p = h->sei_wall;

    for (i = 0; i < size; i++)
        p[i] = get_bits(&h->gb, 8);

    h->sei_wall_size = size;

    return 0;
}


static int decode_unregistered_user_data(H264Context *h, int size)
{
    int i;
    uint8_t uuid[16];

    if (size < 16)
        return AVERROR_INVALIDDATA;

    for (i = 0; i < 16; i++)
        uuid[i] = get_bits(&h->gb, 8);

    av_log(h->avctx, AV_LOG_DEBUG, "uuid 0x%02x 0x%02x 0x%02x 0x%02x "
                                   "0x%02x 0x%02x 0x%02x 0x%02x "
                                   "0x%02x 0x%02x 0x%02x 0x%02x "
                                   "0x%02x 0x%02x 0x%02x 0x%02x\n",
                                   uuid[0], uuid[1], uuid[2], uuid[3],
                                   uuid[4], uuid[5], uuid[6], uuid[7],
                                   uuid[8], uuid[9], uuid[10], uuid[11],
                                   uuid[12], uuid[13], uuid[14], uuid[15]);

    if (!memcmp(uuid, x264_version_uuid, 16))
        return decode_x264_version(h, size - 16);

    if (!memcmp(uuid, vanc_uuid, 16))
        return decode_vanc(h, size - 16);

    if (!memcmp(uuid, wall_uuid, 16))
        return decode_wall(h, size - 16);

    for (; i < size; i++)
        skip_bits(&h->gb, 8);

    return 0;
}

static int decode_recovery_point(H264Context *h)
{
    h->sei_recovery_frame_cnt = get_ue_golomb(&h->gb);

    /* 1b exact_match_flag,
     * 1b broken_link_flag,
     * 2b changing_slice_group_idc */
    skip_bits(&h->gb, 4);

    return 0;
}

static int decode_buffering_period(H264Context *h)
{
    unsigned int sps_id;
    int sched_sel_idx;
    SPS *sps;

    sps_id = get_ue_golomb_31(&h->gb);
    if (sps_id > 31 || !h->sps_buffers[sps_id]) {
        av_log(h->avctx, AV_LOG_ERROR,
               "non-existing SPS %d referenced in buffering period\n", sps_id);
        return AVERROR_INVALIDDATA;
    }
    sps = h->sps_buffers[sps_id];

    // NOTE: This is really so duplicated in the standard... See H.264, D.1.1
    if (sps->nal_hrd_parameters_present_flag) {
        for (sched_sel_idx = 0; sched_sel_idx < sps->cpb_cnt; sched_sel_idx++) {
            h->initial_cpb_removal_delay[sched_sel_idx] =
                get_bits(&h->gb, sps->initial_cpb_removal_delay_length);
            // initial_cpb_removal_delay_offset
            skip_bits(&h->gb, sps->initial_cpb_removal_delay_length);
        }
    }
    if (sps->vcl_hrd_parameters_present_flag) {
        for (sched_sel_idx = 0; sched_sel_idx < sps->cpb_cnt; sched_sel_idx++) {
            h->initial_cpb_removal_delay[sched_sel_idx] =
                get_bits(&h->gb, sps->initial_cpb_removal_delay_length);
            // initial_cpb_removal_delay_offset
            skip_bits(&h->gb, sps->initial_cpb_removal_delay_length);
        }
    }

    h->sei_buffering_period_present = 1;
    return 0;
}

static int decode_frame_packing_arrangement(H264Context *h)
{
    get_ue_golomb(&h->gb);              // frame_packing_arrangement_id
    h->sei_frame_packing_present = !get_bits1(&h->gb);

    if (h->sei_frame_packing_present) {
        h->frame_packing_arrangement_type = get_bits(&h->gb, 7);
        h->quincunx_subsampling           = get_bits1(&h->gb);
        h->content_interpretation_type    = get_bits(&h->gb, 6);

        // the following skips: spatial_flipping_flag, frame0_flipped_flag,
        // field_views_flag, current_frame_is_frame0_flag,
        // frame0_self_contained_flag, frame1_self_contained_flag
        skip_bits(&h->gb, 6);

        if (!h->quincunx_subsampling && h->frame_packing_arrangement_type != 5)
            skip_bits(&h->gb, 16);      // frame[01]_grid_position_[xy]
        skip_bits(&h->gb, 8);           // frame_packing_arrangement_reserved_byte
        get_ue_golomb(&h->gb);          // frame_packing_arrangement_repetition_period
    }
    skip_bits1(&h->gb);                 // frame_packing_arrangement_extension_flag

    return 0;
}

static int decode_display_orientation(H264Context *h)
{
    h->sei_display_orientation_present = !get_bits1(&h->gb);

    if (h->sei_display_orientation_present) {
        h->sei_hflip = get_bits1(&h->gb);     // hor_flip
        h->sei_vflip = get_bits1(&h->gb);     // ver_flip

        h->sei_anticlockwise_rotation = get_bits(&h->gb, 16);
        get_ue_golomb(&h->gb);  // display_orientation_repetition_period
        skip_bits1(&h->gb);     // display_orientation_extension_flag
    }

    return 0;
}

int ff_h264_decode_sei(H264Context *h)
{
    while (get_bits_left(&h->gb) > 16) {
        int size = 0;
        int type = 0;
        int ret  = 0;
        int last = 0;

        while (get_bits_left(&h->gb) >= 8 &&
               (last = get_bits(&h->gb, 8)) == 255) {
            type += 255;
        }
        type += last;

        last = 0;
        while (get_bits_left(&h->gb) >= 8 &&
               (last = get_bits(&h->gb, 8)) == 255) {
            size += 255;
        }
        size += last;

        if (size > get_bits_left(&h->gb) / 8) {
            av_log(h->avctx, AV_LOG_ERROR, "SEI type %d truncated at %d\n",
                   type, get_bits_left(&h->gb));
            return AVERROR_INVALIDDATA;
        }

        switch (type) {
        case SEI_TYPE_PIC_TIMING: // Picture timing SEI
            ret = decode_picture_timing(h);
            break;
        case SEI_TYPE_USER_DATA_REGISTERED:
            ret = decode_registered_user_data(h, size);
            break;
        case SEI_TYPE_USER_DATA_UNREGISTERED:
            ret = decode_unregistered_user_data(h, size);
            break;
        case SEI_TYPE_RECOVERY_POINT:
            ret = decode_recovery_point(h);
            break;
        case SEI_TYPE_BUFFERING_PERIOD:
            ret = decode_buffering_period(h);
            break;
        case SEI_TYPE_FRAME_PACKING:
            ret = decode_frame_packing_arrangement(h);
            break;
        case SEI_TYPE_DISPLAY_ORIENTATION:
            ret = decode_display_orientation(h);
            break;
        default:
            av_log(h->avctx, AV_LOG_DEBUG, "unknown SEI type %d\n", type);
            skip_bits(&h->gb, 8 * size);
        }
        if (ret < 0)
            return ret;

        // FIXME check bits here
        align_get_bits(&h->gb);
    }

    return 0;
}
