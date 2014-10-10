/*
 * Mpeg video formats-related picture management functions
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

#include <stdint.h>

#include "libavutil/avassert.h"

#include "avcodec.h"
#include "mpegpicture.h"
#include "mpegutils.h"

/**
 * Deallocate a picture.
 */
void ff_mpeg_unref_picture(AVCodecContext *avctx, Picture *pic)
{
    pic->tf.f = pic->f;
    /* WM Image / Screen codecs allocate internal buffers with different
     * dimensions / colorspaces; ignore user-defined callbacks for these. */
    if (avctx->codec_id != AV_CODEC_ID_WMV3IMAGE &&
        avctx->codec_id != AV_CODEC_ID_VC1IMAGE  &&
        avctx->codec_id != AV_CODEC_ID_MSS2)
        ff_thread_release_buffer(avctx, &pic->tf);
    else if (pic->f)
        av_frame_unref(pic->f);

    av_buffer_unref(&pic->hwaccel_priv_buf);

    if (pic->needs_realloc)
        ff_free_picture_tables(pic);
}

int ff_update_picture_tables(Picture *dst, Picture *src)
{
     int i;

#define UPDATE_TABLE(table)                                                   \
do {                                                                          \
    if (src->table &&                                                         \
        (!dst->table || dst->table->buffer != src->table->buffer)) {          \
        av_buffer_unref(&dst->table);                                         \
        dst->table = av_buffer_ref(src->table);                               \
        if (!dst->table) {                                                    \
            ff_free_picture_tables(dst);                                      \
            return AVERROR(ENOMEM);                                           \
        }                                                                     \
    }                                                                         \
} while (0)

    UPDATE_TABLE(mb_var_buf);
    UPDATE_TABLE(mc_mb_var_buf);
    UPDATE_TABLE(mb_mean_buf);
    UPDATE_TABLE(mbskip_table_buf);
    UPDATE_TABLE(qscale_table_buf);
    UPDATE_TABLE(mb_type_buf);
    for (i = 0; i < 2; i++) {
        UPDATE_TABLE(motion_val_buf[i]);
        UPDATE_TABLE(ref_index_buf[i]);
    }

    dst->mb_var        = src->mb_var;
    dst->mc_mb_var     = src->mc_mb_var;
    dst->mb_mean       = src->mb_mean;
    dst->mbskip_table  = src->mbskip_table;
    dst->qscale_table  = src->qscale_table;
    dst->mb_type       = src->mb_type;
    for (i = 0; i < 2; i++) {
        dst->motion_val[i] = src->motion_val[i];
        dst->ref_index[i]  = src->ref_index[i];
    }

    return 0;
}

int ff_mpeg_ref_picture(AVCodecContext *avctx, Picture *dst, Picture *src)
{
    int ret;

    av_assert0(!dst->f->buf[0]);
    av_assert0(src->f->buf[0]);

    src->tf.f = src->f;
    dst->tf.f = dst->f;
    ret = ff_thread_ref_frame(&dst->tf, &src->tf);
    if (ret < 0)
        goto fail;

    ret = ff_update_picture_tables(dst, src);
    if (ret < 0)
        goto fail;

    if (src->hwaccel_picture_private) {
        dst->hwaccel_priv_buf = av_buffer_ref(src->hwaccel_priv_buf);
        if (!dst->hwaccel_priv_buf)
            goto fail;
        dst->hwaccel_picture_private = dst->hwaccel_priv_buf->data;
    }

    dst->field_picture           = src->field_picture;
    dst->mb_var_sum              = src->mb_var_sum;
    dst->mc_mb_var_sum           = src->mc_mb_var_sum;
    dst->b_frame_score           = src->b_frame_score;
    dst->needs_realloc           = src->needs_realloc;
    dst->reference               = src->reference;
    dst->shared                  = src->shared;

    return 0;
fail:
    ff_mpeg_unref_picture(avctx, dst);
    return ret;
}

static inline int pic_is_unused(Picture *pic)
{
    if (!pic->f->buf[0])
        return 1;
    if (pic->needs_realloc && !(pic->reference & DELAYED_PIC_REF))
        return 1;
    return 0;
}

static int find_unused_picture(Picture *picture, int shared)
{
    int i;

    if (shared) {
        for (i = 0; i < MAX_PICTURE_COUNT; i++) {
            if (!picture[i].f->buf[0])
                return i;
        }
    } else {
        for (i = 0; i < MAX_PICTURE_COUNT; i++) {
            if (pic_is_unused(&picture[i]))
                return i;
        }
    }

    return AVERROR_INVALIDDATA;
}

int ff_find_unused_picture(AVCodecContext *avctx, Picture *picture, int shared)
{
    int ret = find_unused_picture(picture, shared);

    if (ret >= 0 && ret < MAX_PICTURE_COUNT) {
        if (picture[ret].needs_realloc) {
            picture[ret].needs_realloc = 0;
            ff_free_picture_tables(&picture[ret]);
            ff_mpeg_unref_picture(avctx, &picture[ret]);
        }
    }
    return ret;
}

void ff_free_picture_tables(Picture *pic)
{
    int i;

    av_buffer_unref(&pic->mb_var_buf);
    av_buffer_unref(&pic->mc_mb_var_buf);
    av_buffer_unref(&pic->mb_mean_buf);
    av_buffer_unref(&pic->mbskip_table_buf);
    av_buffer_unref(&pic->qscale_table_buf);
    av_buffer_unref(&pic->mb_type_buf);

    for (i = 0; i < 2; i++) {
        av_buffer_unref(&pic->motion_val_buf[i]);
        av_buffer_unref(&pic->ref_index_buf[i]);
    }
}
