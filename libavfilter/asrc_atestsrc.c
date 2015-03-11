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

/**
 * @file
 * test audio source
 */

#include <inttypes.h>
#include <stdio.h>

#include "libavutil/channel_layout.h"
#include "libavutil/internal.h"

#include "audio.h"
#include "avfilter.h"
#include "formats.h"
#include "internal.h"

static int atest_request_frame(AVFilterLink *link)
{
    AVFrame *frame = ff_default_get_audio_buffer(link, link->request_samples);

    if (!frame)
        return AVERROR(ENOMEM);

    return ff_filter_frame(link, frame);
}

static int config_props(AVFilterLink *link)
{
    link->time_base       = (AVRational){1, link->sample_rate};
    link->request_samples = 1024;

    return 0;
}

static int query_formats(AVFilterContext *ctx)
{
    AVFilterFormats *formats = NULL;
    ff_add_format(&formats, AV_SAMPLE_FMT_FLT);
    ff_add_format(&formats, AV_SAMPLE_FMT_FLTP);
    ff_set_common_formats(ctx, formats);
    ff_set_common_channel_layouts(ctx, ff_all_channel_layouts());
    ff_set_common_samplerates(ctx, ff_all_samplerates());
    return 0;
}

static const AVFilterPad avfilter_asrc_atestsrc_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_AUDIO,
        .request_frame = atest_request_frame,
        .config_props  = config_props,
    },
    { NULL }
};

AVFilter ff_asrc_atestsrc = {
    .name          = "atestsrc",
    .description   = NULL_IF_CONFIG_SMALL("Null audio source, never return audio frames."),

    .query_formats = query_formats,

    .inputs        = NULL,
    .outputs       = avfilter_asrc_atestsrc_outputs,
};
