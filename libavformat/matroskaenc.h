/*
 * Matroska muxing functions
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

#ifndef AVFORMAT_MATROSKAENC_H
#define AVFORMAT_MATROSKAENC_H

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

typedef struct ebml_master {
    int64_t         pos;                ///< absolute offset in the file where the master's elements start
    int             sizebytes;          ///< how many bytes were reserved for the size
} ebml_master;

typedef struct mkv_seekhead_entry {
    unsigned int    elementid;
    uint64_t        segmentpos;
} mkv_seekhead_entry;

typedef struct mkv_seekhead {
    int64_t                 filepos;
    int64_t                 segment_offset;     ///< the file offset to the beginning of the segment
    int                     reserved_size;      ///< -1 if appending to file
    int                     max_entries;
    mkv_seekhead_entry      *entries;
    int                     num_entries;
} mkv_seekhead;

typedef struct mkv_cuepoint {
    uint64_t        pts;
    int             tracknum;
    int64_t         cluster_pos;        ///< file offset of the cluster containing the block
} mkv_cuepoint;

typedef struct mkv_cues {
    int64_t         segment_offset;
    mkv_cuepoint    *entries;
    int             num_entries;
} mkv_cues;

typedef struct mkv_track {
    int             write_dts;
    int64_t         ts_offset;
} mkv_track;


enum {
    MODE_COMPAT   = -1,
    MODE_MATROSKA = 0x01,
    MODE_WEBM     = 0x02,
};

typedef struct MatroskaMuxContext {
    const AVClass  *class;
    int             mode;
    AVIOContext   *dyn_bc;
    ebml_master     segment;
    int64_t         segment_offset;
    ebml_master     cluster;
    int64_t         cluster_pos;        ///< file offset of the current cluster
    int64_t         cluster_pts;
    int64_t         duration_offset;
    int64_t         duration;
    mkv_seekhead    *main_seekhead;
    mkv_cues        *cues;
    mkv_track       *tracks;

    AVPacket        cur_audio_pkt;

    int have_attachments;

    int reserve_cues_space;
    int cluster_size_limit;
    int64_t cues_pos;
    int64_t cluster_time_limit;
    int wrote_chapters;
    int version;
} MatroskaMuxContext;

void ff_put_ebml_id(AVIOContext *pb, unsigned int id);

void ff_put_ebml_num(AVIOContext *pb, uint64_t num, int bytes);

void ff_put_ebml_binary(AVIOContext *pb, unsigned int elementid,
                        const void *buf, int size);

int ff_mkv_write_codecprivate(AVFormatContext *s, AVIOContext *pb,
                              AVCodecContext *codec, int native_id,
                              int qt_id);

void ff_mkv_write_block(AVFormatContext *s, AVIOContext *pb,
                        unsigned int blockid, AVPacket *pkt, int flags);

void ff_get_aac_sample_rates(AVFormatContext *s, AVCodecContext *codec,
                             int *sample_rate, int *output_sample_rate);

#endif /* AVFORMAT_MATROSKAENC_H */
