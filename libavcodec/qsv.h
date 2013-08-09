/*
 * Intel MediaSDK QSV utility functions
 *
 * copyright (c) 2013 Luca Barbato
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

#ifndef AVCODEC_QSV_H
#define AVCODEC_QSV_H

#include <stdint.h>
#include <sys/types.h>
#include <mfx/mfxvideo.h>

#include "libavutil/avutil.h"

#include "libavcodec/avcodec.h"

typedef struct QSVContext {
    AVClass *class;
    mfxSession session;
    mfxVideoParam param;
    mfxFrameSurface1 *surfaces;
    int64_t *dts;
    int64_t *pts;
    int nb_surfaces;
    mfxSyncPoint sync;
    mfxBitstream bs;
    int last_ret;
    int async_depth;
    int reinit;
    AVPacketList *pending, *pending_end;
} QSVContext;

int av_qsv_default_init(AVCodecContext *s);

int av_qsv_default_free(AVCodecContext *s);

#endif /* AVCODEC_QSV_H */
