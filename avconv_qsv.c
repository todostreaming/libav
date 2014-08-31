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

#include "libavcodec/avcodec.h"
#include "libavcodec/qsv.h"

#include "libavutil/imgutils.h"

#include "avconv.h"

static void qsv_uninit(AVCodecContext *s)
{
    InputStream *ist = s->opaque;

    ist->hwaccel_uninit        = NULL;
    ist->hwaccel_retrieve_data = NULL;

    av_qsv_default_free(s);
    av_freep(&ist->hwaccel_ctx);
}

int qsv_init(AVCodecContext *s)
{
    InputStream *ist = s->opaque;
    int loglevel = (ist->hwaccel_id == HWACCEL_AUTO) ? AV_LOG_VERBOSE : AV_LOG_ERROR;
    int ret;

    ist->hwaccel_uninit = qsv_uninit;

    ret = av_qsv_default_init(s);
    if (ret < 0) {
        av_log(NULL, loglevel, "Error creating QSV decoder.\n");
        qsv_uninit(s);
    }

    return ret;
}
