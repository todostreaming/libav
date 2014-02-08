/*
 * MVC-related functions
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

#include "h264.h"
#include "golomb.h"

static int mvc_decode_sps_extension(H264Context *h, SPS *sps)
{
    int i, j, k;
    int view_id, num_ops, num_targets;

    sps->num_views = get_ue_golomb(&h->gb) + 1;
    if (sps->num_views > FFMIN(MAX_VIEW_COUNT, 1024)) {
        av_log(h, AV_LOG_ERROR, "Maximum number of layers reached.\n");
        return AVERROR_INVALIDDATA;
    }

    for (i = 0; i < sps->num_views; i++) {
        view_id = get_ue_golomb(&h->gb);
        sps->view_id[i] = view_id;
        //h->voidx_list[view_id] = i;
    }
    for (i = 1; i < sps->num_views; i++) {
        sps->num_anchor_refs[0][i] = get_ue_golomb(&h->gb);
        for (j = 0; j < sps->num_anchor_refs[0][i]; j++)
            sps->anchor_ref[0][i][j] = get_ue_golomb(&h->gb);

        sps->num_anchor_refs[1][i] = get_ue_golomb(&h->gb);
        for (j = 0; j < sps->num_anchor_refs[1][i]; j++)
            sps->anchor_ref[1][i][j] = get_ue_golomb(&h->gb);
    }
    for (i = 1; i < sps->num_views; i++) {
        sps->num_non_anchor_refs_lX[0][i] = get_ue_golomb(&h->gb);
        for (j = 0; j < sps->num_non_anchor_refs_lX[0][i]; j++)
            sps->non_anchor_ref_lX[0][i][j] = get_ue_golomb(&h->gb);

        sps->num_non_anchor_refs_lX[1][i] = get_ue_golomb(&h->gb);
        for (j = 0; j < sps->num_non_anchor_refs_lX[1][i]; j++)
            sps->non_anchor_ref_lX[1][i][j] = get_ue_golomb(&h->gb);
    }

    // skip the rest
    sps->num_level_values_signalled = get_ue_golomb(&h->gb) + 1;
    for (i = 0; i < sps->num_level_values_signalled; i++) {
        skip_bits(&h->gb, 8);           // level_idc[i]

        num_ops = get_ue_golomb(&h->gb) + 1;
        for (j = 0; j < num_ops; j++) {
            skip_bits(&h->gb, 3);       // applicable_op_temporal_id[i][j]

            num_targets = get_ue_golomb(&h->gb) + 1;
            for (k = 0; k < num_targets; k++)
                get_ue_golomb(&h->gb);  // applicable_op_target_view_id[i][j][k]

            get_ue_golomb(&h->gb);      // applicable_op_num_views_minus1[i][j]
        }
    }

    return 0;
}

static int mvc_decode_vui_parameters(H264Context *h, SPS *sps)
{
    int i, j, ret;
    int vui_mvc_num_ops;
    int vui_mvc_num_target_output_views;
    int vui_mvc_timing_info_present_flag;
    int vui_mvc_nal_hrd_parameters_present_flag;
    int vui_mvc_vcl_hrd_parameters_present_flag;

    sps->inter_layer_deblocking_filter_control_present_flag = get_bits1(&h->gb);
    vui_mvc_num_ops = get_ue_golomb(&h->gb) + 1;
    for (i = 0; i < vui_mvc_num_ops; i++) {
        skip_bits(&h->gb, 3);           // vui_mvc_temporal_id[i]
        vui_mvc_num_target_output_views = get_ue_golomb(&h->gb) + 1;
        for (j = 0; j < vui_mvc_num_target_output_views; j++)
            get_ue_golomb(&h->gb);      // vui_mvc_view_id[i][j]

        vui_mvc_timing_info_present_flag = get_bits1(&h->gb);
        if (vui_mvc_timing_info_present_flag)
            // skip sps->vui_mvc_num_units_in_tick[i] vui_mvc_time_scale[i]
            // and vui_mvc_fixed_frame_rate_flag[i]
            skip_bits(&h->gb, 32 + 32 + 1);

        vui_mvc_nal_hrd_parameters_present_flag = get_bits1(&h->gb);
        if (vui_mvc_nal_hrd_parameters_present_flag) {
            ret = ff_decode_hrd_parameters(h, sps);
            if (ret < 0)
                return ret;
        }

        vui_mvc_vcl_hrd_parameters_present_flag = get_bits1(&h->gb);
        if (vui_mvc_vcl_hrd_parameters_present_flag) {
            ret = ff_decode_hrd_parameters(h, sps);
            if (ret < 0)
                return ret;
        }

        if (vui_mvc_nal_hrd_parameters_present_flag ||
            vui_mvc_vcl_hrd_parameters_present_flag) {
            skip_bits1(&h->gb);         // vui_mvc_low_delay_hrd_flag[i]
        }
        skip_bits1(&h->gb);             // vui_mvc_pic_struct_present_flag[i]
    }

    return 0;
}

int ff_mvc_decode_subset_sequence_parameter_set(H264Context *h)
{
    int ret;

    ret = ff_h264_decode_seq_parameter_set(h);
    if (ret < 0)
        return ret;

    if (h->sps.profile_idc != FF_PROFILE_MVC_MULTIVIEW_HIGH &&
        h->sps.profile_idc != FF_PROFILE_MVC_STEREO_HIGH) {
        avpriv_request_sample(h->avctx, "Profile IDC %d",
                              h->sps.profile_idc);
        return AVERROR_PATCHWELCOME;
    }

    skip_bits1(&h->gb);         /* bit_equal_to_one */
    ret = mvc_decode_sps_extension(h, &h->sps);
    if (ret < 0)
        return ret;

    if (get_bits1(&h->gb)) {    /* mvc_vui_parameters_present_flag */
        ret = mvc_decode_vui_parameters(h, &h->sps);
        if (ret < 0)
            return ret;
    }

    // ignore remaining bits for future extensions
    if (get_bits1(&h->gb))      /* additional_extension2_flag */
        skip_bits(&h->gb, get_bits_left(&h->gb));

    return 0;
}

int ff_mvc_decode_nal_header(H264Context *h)
{
    GetBitContext *gb = &h->gb;

    if (get_bits1(gb)) { /* svc_extension_flag */
        avpriv_request_sample(h, "svc_extension_flag");
        return AVERROR_PATCHWELCOME;
    }

    h->non_idr_flag    = get_bits1(gb);
    h->priority_id     = get_bits(gb, 6);
    h->view_id         = get_bits(gb, 10);
    h->temporal_id     = get_bits(gb, 3);
    h->anchor_pic_flag = get_bits1(gb);
    h->inter_view_flag = get_bits1(gb);
    h->is_mvc          = 1;

    //if (h->nal_unit_type == NAL_PREFIX)
     //   h->base_view_id = h->view_id;

    av_log(h->avctx, AV_LOG_VERBOSE, "NALU: %d %d %d %d %d %d %d\n",
           h->nal_unit_type,
           h->non_idr_flag, h->priority_id, h->view_id, h->temporal_id,
           h->anchor_pic_flag, h->inter_view_flag);

    return 0;
}
