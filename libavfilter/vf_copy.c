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
 * copy video filter
 */

#include "avfilter.h"
#include "internal.h"
#include "video.h"

static AVFilterPad tmp__0[] = {{ "default",
                                    AVMEDIA_TYPE_VIDEO,
                                    0, ~0 ,
                                    ff_null_start_frame,
                                    ff_null_get_video_buffer,
                                    0, ff_null_end_frame},
                                  { NULL}};
static AVFilterPad tmp__1[] = {{ "default",
                                    AVMEDIA_TYPE_VIDEO, },
                                  { NULL}};
AVFilter avfilter_vf_copy = {
    "copy",
    NULL_IF_CONFIG_SMALL("Copy the input video unchanged to the output."),

    tmp__0,
    tmp__1,
};
