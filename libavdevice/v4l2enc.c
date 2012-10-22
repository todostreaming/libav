/*
 * Video4Linux2 grab interface
 * Copyright (c) 2000,2001 Fabrice Bellard
 * Copyright (c) 2006 Luca Abeni
 *
 * Part of this file is based on the V4L2 video capture example
 * (http://v4l2spec.bytesex.org/v4l2spec/capture.c)
 *
 * Thanks to Michael Niedermayer for providing the mapping between
 * V4L2_PIX_FMT_* and AV_PIX_FMT_*
 *
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

#undef __STRICT_ANSI__ //workaround due to broken kernel headers
#include "config.h"
#include "libavformat/avformat.h"
#include "libavformat/internal.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <poll.h>
#if HAVE_SYS_VIDEOIO_H
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif
#include "libavutil/imgutils.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/parseutils.h"
#include "libavutil/pixdesc.h"
#include "libavutil/avstring.h"
#include "libavutil/mathematics.h"

static const int desired_video_buffers = 256;

#define V4L_ALLFORMATS  3
#define V4L_RAWFORMATS  1
#define V4L_COMPFORMATS 2

typedef struct V4L2IndevContext {
    AVClass *class;
    int fd;
    int list_format;    /**< Set by a private option. */
} V4L2IndevContext;

typedef struct V4L2AVPixelFormat {
    enum AVPixelFormat ff_fmt;
    enum AVCodecID codec_id;
    uint32_t v4l2_fmt;
} V4L2AVPixelFormat;

static V4L2AVPixelFormat fmt_conversion_table[] = {
    //ff_fmt           codec_id           v4l2_fmt
    { AV_PIX_FMT_YUV420P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV420  },
    { AV_PIX_FMT_YUV422P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV422P },
    { AV_PIX_FMT_YUYV422, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUYV    },
    { AV_PIX_FMT_UYVY422, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_UYVY    },
    { AV_PIX_FMT_YUV411P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV411P },
    { AV_PIX_FMT_YUV410P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV410  },
    { AV_PIX_FMT_RGB555,  AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB555  },
    { AV_PIX_FMT_RGB565,  AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB565  },
    { AV_PIX_FMT_BGR24,   AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_BGR24   },
    { AV_PIX_FMT_RGB24,   AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB24   },
    { AV_PIX_FMT_BGRA,    AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_BGR32   },
    { AV_PIX_FMT_GRAY8,   AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_GREY    },
    { AV_PIX_FMT_NV12,    AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_NV12    },
    { AV_PIX_FMT_NONE,    AV_CODEC_ID_MJPEG,    V4L2_PIX_FMT_MJPEG   },
    { AV_PIX_FMT_NONE,    AV_CODEC_ID_MJPEG,    V4L2_PIX_FMT_JPEG    },
};

static int device_open(AVFormatContext *ctx)
{
    struct v4l2_capability cap;
    int fd;
    int res, err;
    int flags = O_RDWR;

    if (ctx->flags & AVFMT_FLAG_NONBLOCK) {
        flags |= O_NONBLOCK;
    }

    fd = open(ctx->filename, flags, 0);
    if (fd < 0) {
        err = errno;

        av_log(ctx, AV_LOG_ERROR, "Cannot open video device %s : %s\n",
               ctx->filename, strerror(err));

        return AVERROR(err);
    }

    res = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (res < 0) {
        err = errno;
        av_log(ctx, AV_LOG_ERROR, "ioctl(VIDIOC_QUERYCAP): %s\n",
               strerror(err));

        goto fail;
    }

    av_log(ctx, AV_LOG_VERBOSE, "[%d]Capabilities: %x\n",
           fd, cap.capabilities);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        av_log(ctx, AV_LOG_ERROR, "Not a video output device.\n");
        err = ENODEV;

        goto fail;
    }


    if (!(cap.capabilities & V4L2_CAP_READWRITE /* V4L2_CAP_STREAMING */)) {
        av_log(ctx, AV_LOG_ERROR,
               "The device does not support the write I/O method.\n");
        err = ENOSYS;

        goto fail;
    }

    return fd;

fail:
    close(fd);
    return AVERROR(err);
}

static uint32_t fmt_ff2v4l(enum AVPixelFormat pix_fmt, enum AVCodecID codec_id)
{
    int i;

    for (i = 0; i < FF_ARRAY_ELEMS(fmt_conversion_table); i++) {
        if ((codec_id == AV_CODEC_ID_NONE ||
             fmt_conversion_table[i].codec_id == codec_id) &&
            (pix_fmt == AV_PIX_FMT_NONE ||
             fmt_conversion_table[i].ff_fmt == pix_fmt)) {
            return fmt_conversion_table[i].v4l2_fmt;
        }
    }

    return 0;
}

static enum AVPixelFormat fmt_v4l2ff(uint32_t v4l2_fmt, enum AVCodecID codec_id)
{
    int i;

    for (i = 0; i < FF_ARRAY_ELEMS(fmt_conversion_table); i++) {
        if (fmt_conversion_table[i].v4l2_fmt == v4l2_fmt &&
            fmt_conversion_table[i].codec_id == codec_id) {
            return fmt_conversion_table[i].ff_fmt;
        }
    }

    return AV_PIX_FMT_NONE;
}

static enum AVCodecID fmt_v4l2codec(uint32_t v4l2_fmt)
{
    int i;

    for (i = 0; i < FF_ARRAY_ELEMS(fmt_conversion_table); i++) {
        if (fmt_conversion_table[i].v4l2_fmt == v4l2_fmt) {
            return fmt_conversion_table[i].codec_id;
        }
    }

    return AV_CODEC_ID_NONE;
}

#if HAVE_STRUCT_V4L2_FRMIVALENUM_DISCRETE
static void list_framesizes(AVFormatContext *ctx, int fd, uint32_t pixelformat)
{
    struct v4l2_frmsizeenum vfse = { .pixel_format = pixelformat };

    while(!ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &vfse)) {
        switch (vfse.type) {
        case V4L2_FRMSIZE_TYPE_DISCRETE:
            av_log(ctx, AV_LOG_INFO, " %ux%u",
                   vfse.discrete.width, vfse.discrete.height);
        break;
        case V4L2_FRMSIZE_TYPE_CONTINUOUS:
        case V4L2_FRMSIZE_TYPE_STEPWISE:
            av_log(ctx, AV_LOG_INFO, " {%u-%u, %u}x{%u-%u, %u}",
                   vfse.stepwise.min_width,
                   vfse.stepwise.max_width,
                   vfse.stepwise.step_width,
                   vfse.stepwise.min_height,
                   vfse.stepwise.max_height,
                   vfse.stepwise.step_height);
        }
        vfse.index++;
    }
}
#endif

static void list_formats(AVFormatContext *ctx, int fd, int type)
{
    struct v4l2_fmtdesc vfd = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };

    while(!ioctl(fd, VIDIOC_ENUM_FMT, &vfd)) {
        enum AVCodecID codec_id = fmt_v4l2codec(vfd.pixelformat);
        enum AVPixelFormat pix_fmt = fmt_v4l2ff(vfd.pixelformat, codec_id);

        vfd.index++;

        if (!(vfd.flags & V4L2_FMT_FLAG_COMPRESSED) &&
            type & V4L_RAWFORMATS) {
            const char *fmt_name = av_get_pix_fmt_name(pix_fmt);
            av_log(ctx, AV_LOG_INFO, "R : %9s : %20s :",
                   fmt_name ? fmt_name : "Unsupported",
                   vfd.description);
        } else if (vfd.flags & V4L2_FMT_FLAG_COMPRESSED &&
                   type & V4L_COMPFORMATS) {
            AVCodec *codec = avcodec_find_encoder(codec_id);
            av_log(ctx, AV_LOG_INFO, "C : %9s : %20s :",
                   codec ? codec->name : "Unsupported",
                   vfd.description);
        } else {
            continue;
        }

#ifdef V4L2_FMT_FLAG_EMULATED
        if (vfd.flags & V4L2_FMT_FLAG_EMULATED) {
            av_log(ctx, AV_LOG_WARNING, "%s", "Emulated");
            continue;
        }
#endif
#if HAVE_STRUCT_V4L2_FRMIVALENUM_DISCRETE
        list_framesizes(ctx, fd, vfd.pixelformat);
#endif
        av_log(ctx, AV_LOG_INFO, "\n");
    }
}

static int device_init(AVFormatContext *ctx)
{
    V4L2IndevContext *s         = ctx->priv_data;
    AVCodecContext *c           = ctx->streams[0]->codec;
    struct v4l2_format fmt      = { .type = V4L2_BUF_TYPE_VIDEO_OUTPUT };
    struct v4l2_pix_format *pix = &fmt.fmt.pix;
    int res, fd = s->fd;

    pix->width       = c->width;
    pix->height      = c->height;
    pix->field       = V4L2_FIELD_ANY;
    pix->sizeimage   = avpicture_get_size(c->pix_fmt, c->width, c->height);
    pix->pixelformat = fmt_ff2v4l(c->pix_fmt, ctx->video_codec_id);

    res = ioctl(fd, VIDIOC_S_FMT, &fmt);

    return res;
}

static int v4l2_write_header(AVFormatContext *ctx)
{
    V4L2IndevContext *s = ctx->priv_data;
    int res = 0;

    s->fd = device_open(ctx);
    if (s->fd < 0) {
        res = s->fd;
        goto out;
    }

    if (s->list_format) {
        list_formats(ctx, s->fd, s->list_format);
        res = AVERROR_EXIT;
        goto out;
    }

    res = device_init(ctx);

out:
    return res;
}

static int v4l2_write_packet(AVFormatContext *ctx, AVPacket *pkt)
{
    V4L2IndevContext *s = ctx->priv_data;

    return write(s->fd, pkt->data, pkt->size);
}

static int v4l2_write_close(AVFormatContext *ctx)
{
    V4L2IndevContext *s = ctx->priv_data;

    close(s->fd);
    return 0;
}

#define OFFSET(x) offsetof(V4L2IndevContext, x)
#define ENC AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    { "list_formats", "List available formats and exit",                           OFFSET(list_format),  AV_OPT_TYPE_INT,    {.i64 = 0 },  0, INT_MAX, ENC, "list_formats" },
    { "all",          "Show all available formats",                                OFFSET(list_format),  AV_OPT_TYPE_CONST,  {.i64 = V4L_ALLFORMATS  },    0, INT_MAX, ENC, "list_formats" },
    { "raw",          "Show only non-compressed formats",                          OFFSET(list_format),  AV_OPT_TYPE_CONST,  {.i64 = V4L_RAWFORMATS  },    0, INT_MAX, ENC, "list_formats" },
    { "compressed",   "Show only compressed formats",                              OFFSET(list_format),  AV_OPT_TYPE_CONST,  {.i64 = V4L_COMPFORMATS },    0, INT_MAX, ENC, "list_formats" },
    { NULL },
};

static const AVClass v4l2_class = {
    .class_name = "V4L2 indev",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVInputFormat ff_v4l2_demuxer = {
    .name           = "video4linux2",
    .long_name      = NULL_IF_CONFIG_SMALL("Video4Linux2 device grab"),
    .priv_data_size = sizeof(V4L2IndevContext),
    .read_header    = v4l2_write_header,
    .read_packet    = v4l2_write_packet,
    .read_close     = v4l2_write_close,
    .flags          = AVFMT_NOFILE,
    .priv_class     = &v4l2_class,
};
