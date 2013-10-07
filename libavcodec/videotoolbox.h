/*
 * VideoToolbox hardware acceleration
 *
 * copyright (c) 2012 Sebastien Zwickert
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_VIDEOTOOLBOX_H
#define AVCODEC_VIDEOTOOLBOX_H

/**
 * @file
 * @ingroup lavc_codec_hwaccel_videotooblox
 * Public libavcodec VideoToolbox header.
 */

#include <stdint.h>

#define Picture QuickdrawPicture
#include <VideoToolbox/VideoToolbox.h>
#undef Picture

#include "libavcodec/avcodec.h"

/**
 * This structure is used to provide the necessary configuration and data
 * to the VideoToolbox Libav HWAccel implementation.
 *
 * The application must make it available as AVCodecContext.hwaccel_context.
 */
typedef struct VideotoolboxContext {
    /**
     * VideoToolbox decompression session.
     *
     * - encoding: unused.
     * - decoding: Set/Unset by libavcodec.
     */
    VTDecompressionSessionRef   session;

    /**
     * The type of video compression.
     *
     * - encoding: unused.
     * - decoding: Set/Unset by user.
     */
    int                 cm_codec_type;

    /**
     * The pixel format for output image buffers.
     *
     * - encoding: unused.
     * - decoding: Set/Unset by user.
     */
    OSType              cv_pix_fmt;

    /**
     * The video format description.
     *
     * encoding: unused.
     * decoding: Set by user. Unset by libavcodec.
     */
    CMVideoFormatDescriptionRef cm_fmt_desc;

    /**
     * The Core Video pixel buffer that contains the current image data.
     *
     * encoding: unused.
     * decoding: Set by libavcodec. Unset by user.
     */
    CVImageBufferRef	cv_buffer;

    /**
     * The current bitstream buffer.
     */
    uint8_t             *priv_bitstream;

    /**
     * The current size of the bitstream.
     */
    int                 priv_bitstream_size;

    /**
     * The allocated size used for fast reallocation.
     */
    int                 priv_allocated_size;
} VideotoolboxContext;

/**
 * Create a decompression session.
 *
 * @param avctx         the codec context
 * @param pix_fmt       the pixel format of output image
 * @param width         the width of encoded video
 * @param height        the height of encoded video
 * @return status
 */
int avhwaccel_videotoolbox_session_create(AVCodecContext *avctx,
                                         OSType pix_fmt,
                                         int width,
                                         int height);

/**
 * Destroy a decompression session.
 *
 * @param avctx         the codec context
 */
void avhwaccel_videotoolbox_session_invalidate(AVCodecContext *avctx);

/**
 * @}
 */

#endif /* AVCODEC_VIDEOTOOLBOX_H */
