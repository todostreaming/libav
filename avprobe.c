/*
 * avprobe : Simple Media Prober based on the Libav libraries
 * Copyright (c) 2007-2010 Stefano Sabatini
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

#include "config.h"

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avstring.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/dict.h"
#include "libavutil/libm.h"
#include "libavdevice/avdevice.h"
#include "cmdutils.h"

const char program_name[] = "avprobe";
const int program_birth_year = 2007;

static int do_show_format  = 0;
static AVDictionary *fmt_entries_to_show = NULL;
static int nb_fmt_entries_to_show;
static int do_show_packets = 0;
static int do_show_streams = 0;

static int show_value_unit              = 0;
static int use_value_prefix             = 0;
static int use_byte_value_binary_prefix = 0;
static int use_value_sexagesimal_format = 0;

/* globals */
static const OptionDef *options;

/* AVprobe context */
static const char *input_filename;
static AVInputFormat *iformat = NULL;

static const char *const binary_unit_prefixes [] = { "", "Ki", "Mi", "Gi", "Ti", "Pi" };
static const char *const decimal_unit_prefixes[] = { "", "K" , "M" , "G" , "T" , "P"  };

static const char unit_second_str[]         = "s"    ;
static const char unit_hertz_str[]          = "Hz"   ;
static const char unit_byte_str[]           = "byte" ;
static const char unit_bit_per_second_str[] = "bit/s";

static void exit_program(void)
{
    av_dict_free(&fmt_entries_to_show);
}

/*
 * The output is structured in array and objects that might contain items
 * Array could require the objects within to not be named.
 * Object could require the items within to be named.
 *
 * For flat representation the name of each section is saved on prefix so it
 * can be rendered in order to represent nested structures (e.g. array of
 * objects for the packets list).
 *
 * Within an array each element can need an unique identifier or an index.
 *
 * Nesting level is accounted separately.
 */

typedef enum {
    ARRAY,
    OBJECT
} PrintElementType;

typedef struct {
    const char *name;
    PrintElementType type;
    int64_t index;
    int64_t nb_elems;
} PrintElement;

struct PrintContext;

typedef struct PrintContext {
    AVIOContext *out;
    PrintElement *prefix;
    int level;
    void (*print_header)(struct PrintContext *p);
    void (*print_footer)(struct PrintContext *p);

    void (*print_array_header) (struct PrintContext *p, const char *name);
    void (*print_array_footer) (struct PrintContext *p, const char *name);
    void (*print_object_header)(struct PrintContext *p, const char *name);
    void (*print_object_footer)(struct PrintContext *p, const char *name);

    void (*print_integer) (struct PrintContext *p,
                           const char *key, int64_t value);
    void (*print_string)  (struct PrintContext *p,
                           const char *key, const char *value);
} PrintContext;

static PrintContext octx;

/*
 * Default format, INI
 *
 * - all key and values are utf8
 * - '.' is the subgroup separator
 * - newlines and the following characters are escaped
 * - '\' is the escape character
 * - '#' is the comment
 * - '=' is the key/value separators
 * - ':' is not used but usually parsed as key/value separator
 */

static void ini_print_header(PrintContext *p)
{
    avio_printf(p->out, "# avprobe output\n\n");
}
static void ini_print_footer(PrintContext *p)
{
    avio_w8(p->out, '\n');
}

static void ini_escape_print(AVIOContext *out, const char *s)
{
    int i = 0;
    char c = 0;

    while (c = s[i++]) {
        switch (c) {
        case '\r': avio_printf(out, "%s", "\\r"); break;
        case '\n': avio_printf(out, "%s", "\\n"); break;
        case '\f': avio_printf(out, "%s", "\\f"); break;
        case '\b': avio_printf(out, "%s", "\\b"); break;
        case '\t': avio_printf(out, "%s", "\\t"); break;
        case '\\':
        case '#' :
        case '=' :
        case ':' : avio_w8(out, '\\');
        default:
            if ((unsigned char)c < 32)
                avio_printf(out, "\\x00%02x", c & 0xff);
            else
                avio_w8(out, c);
        break;
        }
    }
}

static void ini_print_array_header(PrintContext *p, const char *name)
{
    if (p->prefix[p->level -1].nb_elems)
        avio_printf(p->out, "\n");
}

static void ini_print_object_header(PrintContext *p, const char *name)
{
    int i;
    AVIOContext *out = p->out;
    PrintElement *el = p->prefix + p->level -1;

    if (el->nb_elems)
        avio_printf(out, "\n");

    avio_printf(out, "[");

    for (i = 1; i < p->level; i++) {
        el = p->prefix + i;
        avio_printf(out, "%s.", el->name);
        if (el->index >= 0)
            avio_printf(out, "%"PRId64".", el->index);
    }

    avio_printf(out, "%s", name);
    if (el && el->type == ARRAY)
        avio_printf(out, ".%"PRId64"", el->nb_elems);
    avio_printf(out, "]\n");
}

static void ini_print_integer(PrintContext *p, const char *key, int64_t value)
{
    ini_escape_print(p->out, key);
    avio_printf(p->out, "=%"PRId64"\n", value);
}


static void ini_print_string(PrintContext *p,
                             const char *key, const char *value)
{
    AVIOContext *out = p->out;

    ini_escape_print(out, key);
    avio_printf(out, "=");
    ini_escape_print(out, value);
    avio_w8(out, '\n');
}

/*
 * Alternate format, JSON
 */
#define AVP_INDENT() avio_printf(out, "%*c", p->level * 2, ' ')

static void json_print_header(PrintContext *p)
{
    avio_printf(p->out, "{");
}
static void json_print_footer(PrintContext *p)
{
    avio_printf(p->out, "}\n");
}

static void json_print_array_header(PrintContext *p, const char *name)
{
    AVIOContext *out = p->out;

    if (p->prefix[p->level -1].nb_elems)
        avio_printf(out, ",\n");
    AVP_INDENT();
    avio_printf(out, "\"%s\" : ", name);
    avio_printf(out, "[\n");
}

static void json_print_array_footer(PrintContext *p, const char *name)
{
    AVIOContext *out = p->out;

    avio_printf(out, "\n");
    AVP_INDENT();
    avio_printf(out, "]");
}

static void json_print_object_header(PrintContext *p, const char *name)
{
    AVIOContext *out = p->out;

    if (p->prefix[p->level -1].nb_elems)
        avio_printf(out, ",\n");
    AVP_INDENT();
    if (p->prefix[p->level -1].type == OBJECT)
        avio_printf(out, "\"%s\" : ", name);
    avio_printf(out, "{\n");
}

static void json_print_object_footer(PrintContext *p, const char *name)
{
    AVIOContext *out = p->out;

    avio_printf(out, "\n");
    AVP_INDENT();
    avio_printf(out, "}");
}

static void json_print_integer(PrintContext *p, const char *key, int64_t value)
{
    AVIOContext *out = p->out;

    if (p->prefix[p->level -1].nb_elems)
        avio_printf(out, ",\n");
    AVP_INDENT();
    avio_printf(out, "\"%s\" : %"PRId64"", key, value);
}

static void json_escape_print(AVIOContext *out, const char *s)
{
    int i = 0;
    char c = 0;

    while (c = s[i++]) {
        switch (c) {
        case '\r': avio_printf(out, "%s", "\\r"); break;
        case '\n': avio_printf(out, "%s", "\\n"); break;
        case '\f': avio_printf(out, "%s", "\\f"); break;
        case '\b': avio_printf(out, "%s", "\\b"); break;
        case '\t': avio_printf(out, "%s", "\\t"); break;
        case '\\':
        case '"' : avio_w8(out, '\\');
        default:
            if ((unsigned char)c < 32)
                avio_printf(out, "\\u00%02x", c & 0xff);
            else
                avio_w8(out, c);
        break;
        }
    }
}

static void json_print_string(PrintContext *p,
                              const char *key, const char *value)
{
    AVIOContext *out = p->out;

    if (p->prefix[p->level -1].nb_elems)
        avio_printf(out, ",\n");
    AVP_INDENT();
    avio_w8(out, '\"');
    json_escape_print(out, key);
    avio_printf(out, "\" : \"");
    json_escape_print(out, value);
    avio_w8(out, '\"');
}

/*
 * old-style pseudo-INI
 */
static void old_print_object_header(PrintContext *p, const char *name)
{
    char *str, *q;

    if (!strcmp(name, "tags"))
        return;

    str = q = av_strdup(name);
    while (*q) {
        *q = av_toupper(*q);
        q++;
    }

    avio_printf(p->out, "[%s]\n", str);
    av_freep(&str);
}

static void old_print_object_footer(PrintContext *p, const char *name)
{
    char *str, *q;

    if (!strcmp(name, "tags"))
        return;

    str = q = av_strdup(name);
    while (*q) {
        *q = av_toupper(*q);
        q++;
    }

    avio_printf(p->out, "[/%s]\n", str);
    av_freep(&str);
}

static void old_print_string(PrintContext *p,
                             const char *key, const char *value)
{
    if (!strcmp(p->prefix[p->level - 1].name, "tags"))
        avio_printf(p->out, "TAG:");
    ini_print_string(p, key, value);
}

/*
 * Simple Formatter for single entries.
 */

static void show_format_entry_integer(PrintContext *p,
                                      const char *key, int64_t value)
{
    if (key && av_dict_get(fmt_entries_to_show, key, NULL, 0)) {
        if (nb_fmt_entries_to_show > 1)
            avio_printf(p->out, "%s=", key);
        avio_printf(p->out, "%"PRId64"\n", value);
    }
}

static void show_format_entry_string(PrintContext *p,
                                     const char *key, const char *value)
{
    if (key && av_dict_get(fmt_entries_to_show, key, NULL, 0)) {
        if (nb_fmt_entries_to_show > 1)
            avio_printf(p->out, "%s=", key);
        avio_printf(p->out, "%s\n", value);
    }
}

/*
 * print external api
 */

static void print_group_enter(PrintContext *p, const char *name, int type)
{
    int64_t count = -1;

    p->prefix =
        av_realloc(p->prefix, sizeof(PrintElement) * (p->level + 1));

    if (!p->prefix || !name) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    if (p->level) {
        PrintElement *parent = p->prefix + p->level -1;
        if (parent->type == ARRAY)
            count = parent->nb_elems;
        parent->nb_elems++;
    }

    p->prefix[p->level++] = (PrintElement){name, type, count, 0};
}

static void print_group_leave(PrintContext *p)
{
    --p->level;
}

static void print_header(PrintContext *p)
{
    if (p->print_header)
        p->print_header(p);
    print_group_enter(p, "root", OBJECT);
}

static void print_footer(PrintContext *p)
{
    if (p->print_footer)
        p->print_footer(p);
    print_group_leave(p);
}


static void print_array_header(PrintContext *p, const char *name)
{
    if (p->print_array_header)
        p->print_array_header(p, name);

    print_group_enter(p, name, ARRAY);
}

static void print_array_footer(PrintContext *p, const char *name)
{
    print_group_leave(p);
    if (p->print_array_footer)
        p->print_array_footer(p, name);
}

static void print_object_header(PrintContext *p, const char *name)
{
    if (p->print_object_header)
        p->print_object_header(p, name);

    print_group_enter(p, name, OBJECT);
}

static void print_object_footer(PrintContext *p, const char *name)
{
    print_group_leave(p);
    if (p->print_object_footer)
        p->print_object_footer(p, name);
}

static void print_int(PrintContext *p, const char *key, int64_t value)
{
    p->print_integer(p, key, value);
    p->prefix[p->level -1].nb_elems++;
}

static void print_str(PrintContext *p, const char *key, const char *value)
{
    p->print_string(p, key, value);
    p->prefix[p->level -1].nb_elems++;
}

static void print_dict(PrintContext *p, AVDictionary *dict, const char *name)
{
    AVDictionaryEntry *entry = NULL;
    if (!dict)
        return;
    print_object_header(p, name);
    while ((entry = av_dict_get(dict, "", entry, AV_DICT_IGNORE_SUFFIX))) {
        print_str(p, entry->key, entry->value);
    }
    print_object_footer(p, name);
}

static char *value_string(char *buf, int buf_size, double val, const char *unit)
{
    if (unit == unit_second_str && use_value_sexagesimal_format) {
        double secs;
        int hours, mins;
        secs  = val;
        mins  = (int)secs / 60;
        secs  = secs - mins * 60;
        hours = mins / 60;
        mins %= 60;
        snprintf(buf, buf_size, "%d:%02d:%09.6f", hours, mins, secs);
    } else if (use_value_prefix) {
        const char *prefix_string;
        int index;

        if (unit == unit_byte_str && use_byte_value_binary_prefix) {
            index = (int) log2(val) / 10;
            index = av_clip(index, 0, FF_ARRAY_ELEMS(binary_unit_prefixes) - 1);
            val  /= pow(2, index * 10);
            prefix_string = binary_unit_prefixes[index];
        } else {
            index = (int) (log10(val)) / 3;
            index = av_clip(index, 0, FF_ARRAY_ELEMS(decimal_unit_prefixes) - 1);
            val  /= pow(10, index * 3);
            prefix_string = decimal_unit_prefixes[index];
        }
        snprintf(buf, buf_size, "%.*f%s%s",
                 index ? 3 : 0, val,
                 prefix_string,
                 show_value_unit ? unit : "");
    } else {
        snprintf(buf, buf_size, "%f%s", val, show_value_unit ? unit : "");
    }

    return buf;
}

static char *time_value_string(char *buf, int buf_size, int64_t val,
                               const AVRational *time_base)
{
    if (val == AV_NOPTS_VALUE) {
        snprintf(buf, buf_size, "N/A");
    } else {
        value_string(buf, buf_size, val * av_q2d(*time_base), unit_second_str);
    }

    return buf;
}

static char *ts_value_string(char *buf, int buf_size, int64_t ts)
{
    if (ts == AV_NOPTS_VALUE) {
        snprintf(buf, buf_size, "N/A");
    } else {
        snprintf(buf, buf_size, "%"PRId64, ts);
    }

    return buf;
}

static char *rational_string(char *buf, int buf_size, const char *sep,
                             const AVRational *rat)
{
    snprintf(buf, buf_size, "%d%s%d", rat->num, sep, rat->den);
    return buf;
}

static char *tag_string(char *buf, int buf_size, int tag)
{
    snprintf(buf, buf_size, "0x%04x", tag);
    return buf;
}

static void show_packet(PrintContext *p,
                        AVFormatContext *fmt_ctx, AVPacket *pkt)
{
    char val_str[128];
    AVStream *st = fmt_ctx->streams[pkt->stream_index];

    print_object_header(p, "packet");
    print_str(p, "codec_type", media_type_string(st->codec->codec_type));
    print_int(p, "stream_index", pkt->stream_index);
    print_str(p, "pts", ts_value_string(val_str, sizeof(val_str), pkt->pts));
    print_str(p, "pts_time", time_value_string(val_str, sizeof(val_str),
                                               pkt->pts, &st->time_base));
    print_str(p, "dts", ts_value_string(val_str, sizeof(val_str), pkt->dts));
    print_str(p, "dts_time", time_value_string(val_str, sizeof(val_str),
                                               pkt->dts, &st->time_base));
    print_str(p, "duration", ts_value_string(val_str, sizeof(val_str),
                                             pkt->duration));
    print_str(p, "duration_time", time_value_string(val_str, sizeof(val_str),
                                                    pkt->duration,
                                                    &st->time_base));
    print_str(p, "size", value_string(val_str, sizeof(val_str),
                                      pkt->size, unit_byte_str));
    print_int(p, "pos", pkt->pos);
    print_str(p, "flags", pkt->flags & AV_PKT_FLAG_KEY ? "K" : "_");
    print_object_footer(p, "packet");
}

static void show_packets(PrintContext *p, AVFormatContext *fmt_ctx)
{
    AVPacket pkt;

    av_init_packet(&pkt);
    print_array_header(p, "packets");
    while (!av_read_frame(fmt_ctx, &pkt))
        show_packet(p, fmt_ctx, &pkt);
    print_array_footer(p, "packets");
}

static void show_stream(PrintContext *p,
                        AVFormatContext *fmt_ctx, int stream_idx)
{
    AVStream *stream = fmt_ctx->streams[stream_idx];
    AVCodecContext *dec_ctx;
    const AVCodec *dec;
    const char *profile;
    char val_str[128];
    AVRational display_aspect_ratio, *sar = NULL;
    const AVPixFmtDescriptor *desc;

    print_object_header(p, "stream");

    print_int(p, "index", stream->index);

    if ((dec_ctx = stream->codec)) {
        if ((dec = dec_ctx->codec)) {
            print_str(p, "codec_name", dec->name);
            print_str(p, "codec_long_name", dec->long_name);
        } else {
            print_str(p, "codec_name", "unknown");
        }

        print_str(p, "codec_type", media_type_string(dec_ctx->codec_type));
        print_str(p, "codec_time_base",
                  rational_string(val_str, sizeof(val_str),
                                  "/", &dec_ctx->time_base));

        /* print AVI/FourCC tag */
        av_get_codec_tag_string(val_str, sizeof(val_str), dec_ctx->codec_tag);
        print_str(p, "codec_tag_string", val_str);
        print_str(p, "codec_tag", tag_string(val_str, sizeof(val_str),
                                             dec_ctx->codec_tag));

        /* print profile, if there is one */
        if (dec && (profile = av_get_profile_name(dec, dec_ctx->profile)))
            print_str(p, "profile", profile);

        switch (dec_ctx->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            print_int(p, "width", dec_ctx->width);
            print_int(p, "height", dec_ctx->height);
            print_int(p, "has_b_frames", dec_ctx->has_b_frames);
            if (dec_ctx->sample_aspect_ratio.num)
                sar = &dec_ctx->sample_aspect_ratio;
            else if (stream->sample_aspect_ratio.num)
                sar = &stream->sample_aspect_ratio;

            if (sar) {
                print_str(p, "sample_aspect_ratio",
                          rational_string(val_str, sizeof(val_str), ":", sar));
                av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den,
                          dec_ctx->width  * sar->num, dec_ctx->height * sar->den,
                          1024*1024);
                print_str(p, "display_aspect_ratio",
                          rational_string(val_str, sizeof(val_str), ":",
                          &display_aspect_ratio));
            }
            desc = av_pix_fmt_desc_get(dec_ctx->pix_fmt);
            print_str(p, "pix_fmt", desc ? desc->name : "unknown");
            print_int(p, "level", dec_ctx->level);
            break;

        case AVMEDIA_TYPE_AUDIO:
            print_str(p, "sample_rate",
                      value_string(val_str, sizeof(val_str),
                                   dec_ctx->sample_rate,
                                   unit_hertz_str));
            print_int(p, "channels", dec_ctx->channels);
            print_int(p, "bits_per_sample",
                      av_get_bits_per_sample(dec_ctx->codec_id));
            break;
        }
    } else {
        print_str(p, "codec_type", "unknown");
    }

    if (fmt_ctx->iformat->flags & AVFMT_SHOW_IDS)
        print_int(p, "id", stream->id);
    print_str(p, "avg_frame_rate",
              rational_string(val_str, sizeof(val_str), "/",
              &stream->avg_frame_rate));
    if (dec_ctx->bit_rate)
        print_str(p, "bit_rate",
                  value_string(val_str, sizeof(val_str),
                               dec_ctx->bit_rate, unit_bit_per_second_str));
    print_str(p, "time_base",
              rational_string(val_str, sizeof(val_str), "/",
              &stream->time_base));
    print_str(p, "start_time",
              time_value_string(val_str, sizeof(val_str),
                                stream->start_time, &stream->time_base));
    print_str(p, "duration",
              time_value_string(val_str, sizeof(val_str),
                                stream->duration, &stream->time_base));
    if (stream->nb_frames)
        print_int(p, "nb_frames", stream->nb_frames);

    print_dict(p, stream->metadata, "tags");

    print_object_footer(p, "stream");
}

static void show_format(PrintContext *p, AVFormatContext *fmt_ctx)
{
    char val_str[128];
    int64_t size = fmt_ctx->pb ? avio_size(fmt_ctx->pb) : -1;

    print_object_header(p, "format");
    print_str(p, "filename",         fmt_ctx->filename);
    print_int(p, "nb_streams",       fmt_ctx->nb_streams);
    print_str(p, "format_name",      fmt_ctx->iformat->name);
    print_str(p, "format_long_name", fmt_ctx->iformat->long_name);
    print_str(p, "start_time",
                       time_value_string(val_str, sizeof(val_str),
                                         fmt_ctx->start_time, &AV_TIME_BASE_Q));
    print_str(p, "duration",
                       time_value_string(val_str, sizeof(val_str),
                                         fmt_ctx->duration, &AV_TIME_BASE_Q));
    print_str(p, "size",
                       size >= 0 ? value_string(val_str, sizeof(val_str),
                                                size, unit_byte_str)
                                  : "unknown");
    print_str(p, "bit_rate",
                       value_string(val_str, sizeof(val_str),
                                    fmt_ctx->bit_rate, unit_bit_per_second_str));

    print_dict(p, fmt_ctx->metadata, "tags");

    print_object_footer(p, "format");
}

static int open_input_file(AVFormatContext **fmt_ctx_ptr, const char *filename)
{
    int err, i;
    AVFormatContext *fmt_ctx = NULL;
    AVDictionaryEntry *t;

    if ((err = avformat_open_input(&fmt_ctx, filename,
                                   iformat, &format_opts)) < 0) {
        print_error(filename, err);
        return err;
    }
    if ((t = av_dict_get(format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        return AVERROR_OPTION_NOT_FOUND;
    }


    /* fill the streams in the format context */
    if ((err = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        print_error(filename, err);
        return err;
    }

    av_dump_format(fmt_ctx, 0, filename, 0);

    /* bind a decoder to each input stream */
    for (i = 0; i < fmt_ctx->nb_streams; i++) {
        AVStream *stream = fmt_ctx->streams[i];
        AVCodec *codec;

        if (stream->codec->codec_id == AV_CODEC_ID_PROBE) {
            fprintf(stderr, "Failed to probe codec for input stream %d\n",
                    stream->index);
        } else if (!(codec = avcodec_find_decoder(stream->codec->codec_id))) {
            fprintf(stderr,
                    "Unsupported codec with id %d for input stream %d\n",
                    stream->codec->codec_id, stream->index);
        } else if (avcodec_open2(stream->codec, codec, NULL) < 0) {
            fprintf(stderr, "Error while opening codec for input stream %d\n",
                    stream->index);
        }
    }

    *fmt_ctx_ptr = fmt_ctx;
    return 0;
}

static void close_input_file(AVFormatContext **ctx_ptr)
{
    int i;
    AVFormatContext *fmt_ctx = *ctx_ptr;

    /* close decoder for each stream */
    for (i = 0; i < fmt_ctx->nb_streams; i++) {
        AVStream *stream = fmt_ctx->streams[i];

        avcodec_close(stream->codec);
    }
    avformat_close_input(ctx_ptr);
}

static int probe_file(PrintContext *p, const char *filename)
{
    AVFormatContext *fmt_ctx;
    int ret, i;

    if ((ret = open_input_file(&fmt_ctx, filename)))
        return ret;

    if (do_show_format)
        show_format(p, fmt_ctx);

    if (do_show_streams) {
        print_array_header(p, "streams");
        for (i = 0; i < fmt_ctx->nb_streams; i++)
            show_stream(p, fmt_ctx, i);
        print_array_footer(p, "streams");
    }

    if (do_show_packets)
        show_packets(p, fmt_ctx);

    close_input_file(&fmt_ctx);
    return 0;
}

static void show_usage(void)
{
    printf("Simple multimedia streams analyzer\n");
    printf("usage: %s [OPTIONS] [INPUT_FILE]\n", program_name);
    printf("\n");
}

static int opt_format(void *optctx, const char *opt, const char *arg)
{
    iformat = av_find_input_format(arg);
    if (!iformat) {
        fprintf(stderr, "Unknown input format: %s\n", arg);
        return AVERROR(EINVAL);
    }
    return 0;
}
static int print_set_formatter(PrintContext *p, const char *formatter)
{
    if (!formatter)
        return AVERROR(EINVAL);

    if (!strcmp(formatter, "json")) {
        p->print_header        = json_print_header;
        p->print_footer        = json_print_footer;
        p->print_array_header  = json_print_array_header;
        p->print_array_footer  = json_print_array_footer;
        p->print_object_header = json_print_object_header;
        p->print_object_footer = json_print_object_footer;

        p->print_integer = json_print_integer;
        p->print_string  = json_print_string;
    } else if (!strcmp(formatter, "ini")) {
        p->print_header        = ini_print_header;
        p->print_footer        = ini_print_footer;
        p->print_array_header  = ini_print_array_header;
        p->print_object_header = ini_print_object_header;

        p->print_integer = ini_print_integer;
        p->print_string  = ini_print_string;
    } else if (!strcmp(formatter, "old")) {
        p->print_header        = NULL;
        p->print_object_header = old_print_object_header;
        p->print_object_footer = old_print_object_footer;

        p->print_integer       = ini_print_integer;
        p->print_string        = old_print_string;
    } else {
        av_log(NULL, AV_LOG_ERROR, "Unsupported formatter %s\n", formatter);
        return AVERROR(EINVAL);
    }

    return 0;
}

static int opt_output_format(void *optctx, const char *opt, const char *arg)
{
    return print_set_formatter(&octx, arg);
}

static int opt_show_format_entry(void *optctx, const char *opt, const char *arg)
{
    do_show_format = 1;
    nb_fmt_entries_to_show++;
    octx.print_header        = NULL;
    octx.print_footer        = NULL;
    octx.print_array_header  = NULL;
    octx.print_array_footer  = NULL;
    octx.print_object_header = NULL;
    octx.print_object_footer = NULL;

    octx.print_integer = show_format_entry_integer;
    octx.print_string  = show_format_entry_string;
    av_dict_set(&fmt_entries_to_show, arg, "", 0);
    return 0;
}

static void opt_input_file(void *optctx, const char *arg)
{
    if (input_filename) {
        fprintf(stderr,
                "Argument '%s' provided as input filename, but '%s' was already specified.\n",
                arg, input_filename);
        exit(1);
    }
    if (!strcmp(arg, "-"))
        arg = "pipe:";
    input_filename = arg;
}

void show_help_default(const char *opt, const char *arg)
{
    av_log_set_callback(log_callback_help);
    show_usage();
    show_help_options(options, "Main options:", 0, 0, 0);
    printf("\n");
    show_help_children(avformat_get_class(), AV_OPT_FLAG_DECODING_PARAM);
}

static int opt_pretty(void *optctx, const char *opt, const char *arg)
{
    show_value_unit              = 1;
    use_value_prefix             = 1;
    use_byte_value_binary_prefix = 1;
    use_value_sexagesimal_format = 1;
    return 0;
}

static const OptionDef real_options[] = {
#include "cmdutils_common_opts.h"
    { "f", HAS_ARG, {.func_arg = opt_format}, "force format", "format" },
    { "of", HAS_ARG, {.func_arg = opt_output_format}, "output the document either as ini or json", "output_format" },
    { "unit", OPT_BOOL, {&show_value_unit},
      "show unit of the displayed values" },
    { "prefix", OPT_BOOL, {&use_value_prefix},
      "use SI prefixes for the displayed values" },
    { "byte_binary_prefix", OPT_BOOL, {&use_byte_value_binary_prefix},
      "use binary prefixes for byte units" },
    { "sexagesimal", OPT_BOOL,  {&use_value_sexagesimal_format},
      "use sexagesimal format HOURS:MM:SS.MICROSECONDS for time units" },
    { "pretty", 0, {.func_arg = opt_pretty},
      "prettify the format of displayed values, make it more human readable" },
    { "show_format",  OPT_BOOL, {&do_show_format} , "show format/container info" },
    { "show_format_entry", HAS_ARG, {.func_arg = opt_show_format_entry},
      "show a particular entry from the format/container info", "entry" },
    { "show_packets", OPT_BOOL, {&do_show_packets}, "show packets info" },
    { "show_streams", OPT_BOOL, {&do_show_streams}, "show streams info" },
    { "default", HAS_ARG | OPT_AUDIO | OPT_VIDEO | OPT_EXPERT, {.func_arg = opt_default},
      "generic catch all option", "" },
    { NULL, },
};

static int probe_buf_write(void *opaque, uint8_t *buf, int buf_size)
{
    printf("%.*s", buf_size, buf);
    return 0;
}

#define AVP_BUFFSIZE 4096

int main(int argc, char **argv)
{
    int ret;
    uint8_t *buffer = av_malloc(AVP_BUFFSIZE);

    if (!buffer)
        exit(1);

    atexit(exit_program);

    options = real_options;
    parse_loglevel(argc, argv, options);
    av_register_all();
    avformat_network_init();
    init_opts();
#if CONFIG_AVDEVICE
    avdevice_register_all();
#endif

    show_banner();

    print_set_formatter(&octx, "ini");

    parse_options(NULL, argc, argv, options, opt_input_file);

    if (!input_filename) {
        show_usage();
        fprintf(stderr, "You have to specify one input file.\n");
        fprintf(stderr,
                "Use -h to get full help or, even better, run 'man %s'.\n",
                program_name);
        exit(1);
    }

    octx.out = avio_alloc_context(buffer, AVP_BUFFSIZE, 1, NULL, NULL,
                                 probe_buf_write, NULL);
    if (!octx.out)
        exit(1);

    print_header(&octx);
    ret = probe_file(&octx, input_filename);
    print_footer(&octx);
    avio_flush(octx.out);
    avio_close(octx.out);

    avformat_network_deinit();

    return ret;
}
