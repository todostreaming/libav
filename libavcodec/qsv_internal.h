/*
 * Intel MediaSDK QSV internal utility functions
 *
 * Copyright (c) 2013 Luca Barbato
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

#ifndef AVCODEC_QSV_INTERNAL_H
#define AVCODEC_QSV_INTERNAL_H

#include <stdint.h>

#include "qsv.h"

#define QSV_VERSION_MAJOR 1
#define QSV_VERSION_MINOR 1

#define ASYNC_DEPTH_DEFAULT 4 // internal parallelism

int ff_qsv_error(int mfx_err);

int ff_qsv_init(AVCodecContext *s, QSVContext *q);

int ff_qsv_decode(AVCodecContext *s, QSVContext *q,
                  AVFrame *frame, int *got_frame,
                  AVPacket *avpkt);

int ff_qsv_flush(QSVContext *q);

int ff_qsv_close(QSVContext *q);

#endif /* AVCODEC_QSV_INTERNAL_H */
