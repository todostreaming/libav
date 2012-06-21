/*
 * Terribly standard fake license header.
 *
 * The purpose of this preprocessor is to handle some typical C99'
 * isms used in the Libav/FFmpeg projects, and convert those to
 * typical C89-style code such that the resulting preprocessed
 * source code can be compiled under a C89 compiler such as MSVC.
 *
 * This doesn't handle all C99'isms and that is not its goal.
 *
 * You'll notice that this application itself actually is C99. I
 * guess that's kind of funny if you think about it, because it
 * causes a catch-22 when you're on Windows with MSVC and all you
 * want is to compile FFmpeg/Libav on MSVC. I'm sure you'll figure
 * out a way to get that to work. Reason for this is that it allows
 * me to use this applications's source code itself as test input
 * for the preprocessor.
 *
 * Examples:
 *
 *   function((const int[2]) { 1, 2 });
 * becomes
 *   static const int tempvar[2] = { 1, 2 };
 *   function(tempvar);
 *
 *   struct { int x, y; } var;
 *   var = { 0, 1 };
 * becomes
 *   var.x = 0;
 *   var.y = 1;
 *
 * struct x {
 *   int a, b;
 * };
 * enum {
 *   VAL_0 = 0,
 *   VAL_1 = 1,
 *   VAL_2 = 2,
 * };
 * static const struct x var[2] = {
 *   [VAL_2] = {
 *     .b = 2,
 *   },
 *   [VAL_0] = {
 *     .b = 3,
 *     .a = 1,
 *   },
 * };
 * becomes
 * static const struct x var[2] = {
 *   { 1, 3 },
 *   {      },
 *   { 0, 2 },
 * };
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#define LINELEN 256
#define N_LINES 11000 /* for twinvq_data.h; was 1024 */
#define S_ENTRIES 150 /* for id3v1 array */
#define TVALLEN (LINELEN * LINELEN)
#define DEBUG if (1)
#define isspace(x) ((x) == '\t' || (x) == ' ')

struct s_contents {
    char *tag, *struct_type_name /* NULL if none */;
    int array_count /* 0 if none */;
};

struct s_layout {
    char *name; // if anonymous, the name of the parent plus ":" plus tag in parent is used
    int n_contents;
    const struct s_contents contents[S_ENTRIES];
};

struct enum_mapping {
    char *tag;
    int val;
};

// FIXME autogenerate
enum {
    STRUCT_S_CONTENTS,
    STRUCT_DUMMY, // cheat to test named array initializer conversion
    STRUCT_S_LAYOUT,
    STRUCT_LNUM_POS_PAIR,
    STRUCT_TAG_VAL_PAIR,
    STRUCT_STATE_NIC,
    STRUCT_STATE_NIC_NVP,
    STRUCT_STATE,
    STRUCT_ENUM_MAPPING,
    STRUCT_AVRATIONAL,
    STRUCT_SAMPLEFMTINFO,
    STRUCT_AVPIXFMTDESCRIPTOR,
    STRUCT_AVCOMPONENTDESCRIPTOR,
    STRUCT_AVCODEC,
    STRUCT_AVCLASS,
    STRUCT_AVFILTER,
    STRUCT_AVFILTERPAD,
    STRUCT_AVOPTION,
    STRUCT_AVOPTION_DEFAULTVAL,
    STRUCT_AVINPUTFORMAT,
    STRUCT_AVOUTPUTFORMAT,
    STRUCT_AVPROBEDATA,
    STRUCT_URLPROTOCOL,
    STRUCT_EBMLSYNTAX,
    STRUCT_EBMLSYNTAX_DEFAULTVAL,
    STRUCT_MOVATOM,
    STRUCT_AVPACKET,
    STRUCT_POLLFD,
    STRUCT_SYNCPOINT,
    STRUCT_OGG_CODEC,
    STRUCT_RTPDYNAMICPROTOCOLHANDLER,
    STRUCT_AVCODECPARSER,
    STRUCT_ELEMTOCHANNEL,
    STRUCT_PSYMODEL,
    STRUCT_DVPROFILE,
    STRUCT_AVHWACCEL,
    STRUCT_MACROBLOCK,
    STRUCT_PIXFMTINFO,
    STRUCT_AVBITSTREAMFILTER,
    STRUCT_PRORESPROFILE,
    STRUCT_MOTIONVECT,
    STRUCT_SIPRMODEPARAMS,
    STRUCT_BLOCKNODE,
    STRUCT_TWINVQDATA,
    STRUCT_VP56MV,
    STRUCT_FORMATENTRY,
	STRUCT_OPTIONDEF,
	STRUCT_OPTIONDEF_U,
    N_STRUCTS,
};

// FIXME autogenerate
static const struct s_layout known_structs[N_STRUCTS] = {
    [STRUCT_AVCODEC] = {
        .name = "AVCodec",
        .n_contents = 25,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "long_name",
            }, {
                .tag = "type",
            }, {
                .tag = "id",
            }, {
                .tag = "capabilities",
            }, {
                .tag = "supported_framerates",
            }, {
                .tag = "pix_fmts",
            }, {
                .tag = "supported_samplerates",
            }, {
                .tag = "sample_fmts",
            }, {
                .tag = "channel_layouts",
            }, {
                .tag = "max_lowres",
            }, {
                .tag = "priv_class",
            }, {
                .tag = "profiles",
            }, {
                .tag = "priv_data_size",
            }, {
                .tag = "next",
            }, {
                .tag = "init_thread_copy",
            }, {
                .tag = "update_thread_context",
            }, {
                .tag = "defaults",
            }, {
                .tag = "init_static_data",
            }, {
                .tag = "init",
            }, {
                .tag = "encode",
            }, {
                .tag = "encode2",
            }, {
                .tag = "decode",
            }, {
                .tag = "close",
            }, {
                .tag = "flush",
            },
        },
    }, [STRUCT_AVRATIONAL] = {
        .name = "AVRational",
        .n_contents = 2,
        .contents = {
            {
                .tag = "num",
            }, {
                .tag = "den",
            },
        },
    }, [STRUCT_SAMPLEFMTINFO] = {
        .name = "SampleFmtInfo",
        .n_contents = 4,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "bits",
            }, {
                .tag = "planar",
            }, {
				.tag = "altform",
			},
        },
    }, [STRUCT_AVPIXFMTDESCRIPTOR] = {
        .name = "AVPixFmtDescriptor",
        .n_contents = 6,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "nb_components",
            }, {
                .tag = "log2_chroma_w",
            }, {
                .tag = "log2_chroma_h",
            }, {
                .tag = "flags",
            }, {
                .tag = "comp",
                .array_count = 4,
                .struct_type_name = "AVComponentDescriptor",
            },
        },
    }, [STRUCT_AVCOMPONENTDESCRIPTOR] = {
        .name = "AVComponentDescriptor",
        .n_contents = 5,
        .contents = {
            {
                .tag = "plane",
            }, {
                .tag = "step_minus1",
            }, {
                .tag = "offset_plus1",
            }, {
                .tag = "shift",
            }, {
                .tag = "depth_minus1",
            },
        },
    }, [STRUCT_AVCLASS] = {
        .name = "AVClass",
        .n_contents = 8,
        .contents = {
            {
                .tag = "class_name",
            }, {
                .tag = "item_name",
            }, {
                .tag = "option",
            }, {
                .tag = "version",
            }, {
                .tag = "log_level_offset_offset",
            }, {
                .tag = "parent_log_context_offset",
            }, {
                .tag = "child_next",
            }, {
                .tag = "child_class_next",
            },
        },
    }, [STRUCT_AVFILTER] = {
        .name = "AVFilter",
        .n_contents = 8,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "priv_size",
            }, {
                .tag = "init",
            }, {
                .tag = "uninit",
            }, {
                .tag = "query_formats",
            }, {
                .tag = "inputs",
                .struct_type_name = "AVFilterPad",
            }, {
                .tag = "outputs",
                .struct_type_name = "AVFilterPad",
            }, {
                .tag = "description",
            },
        },
    }, [STRUCT_AVFILTERPAD] = {
        .name = "AVFilterPad",
        .n_contents = 13,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "type",
            }, {
                .tag = "min_perms",
            }, {
                .tag = "rej_perms",
            }, {
                .tag = "start_frame",
            }, {
                .tag = "get_video_buffer",
            }, {
                .tag = "get_audio_buffer",
            }, {
                .tag = "end_frame",
            }, {
                .tag = "draw_slice",
            }, {
                .tag = "filter_samples",
            }, {
                .tag = "poll_frame",
            }, {
                .tag = "request_frame",
            }, {
                .tag = "config_props",
            },
        },
    }, [STRUCT_AVOPTION] = {
        .name = "AVOption",
        .n_contents = 9,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "help",
            }, {
                .tag = "offset",
            }, {
                .tag = "type",
            }, {
                .tag = "default_val",
                .struct_type_name = "AVOption:default_val",
            }, {
                .tag = "min",
            }, {
                .tag = "max",
            }, {
                .tag = "flags",
            }, {
                .tag = "unit",
            },
        },
        // FIXME I'm not sure this is a good idea for a union
    }, [STRUCT_AVOPTION_DEFAULTVAL] = {
        .name = "AVOption:default_val",
        .n_contents = 4,
        .contents = {
            {
                .tag = "dbl",
            }, {
                .tag = "str",
            }, {
                .tag = "i64",
            }, {
                .tag = "q",
                .struct_type_name = "AVRational",
            },
        },
    }, [STRUCT_AVINPUTFORMAT] = {
        .name = "AVInputFormat",
        .n_contents = 18,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "long_name",
            }, {
                .tag = "flags",
            }, {
                .tag = "extensions",
            }, {
                .tag = "codec_tag",
            }, {
                .tag = "priv_class",
            }, {
                .tag = "next",
            }, {
                .tag = "raw_codec_id",
            }, {
                .tag = "priv_data_size",
            }, {
                .tag = "read_probe",
            }, {
                .tag = "read_header",
            }, {
                .tag = "read_packet",
            }, {
                .tag = "read_close",
            }, {
                .tag = "read_seek",
            }, {
                .tag = "read_timestamp",
            }, {
                .tag = "read_play",
            }, {
                .tag = "read_pause",
            }, {
                .tag = "read_seek2",
            },
        },
    }, [STRUCT_AVOUTPUTFORMAT] = {
        .name = "AVOutputFormat",
        .n_contents = 17,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "long_name",
            }, {
                .tag = "mime_type",
            }, {
                .tag = "extensions",
            }, {
                .tag = "audio_codec",
            }, {
                .tag = "video_codec",
            }, {
                .tag = "subtitle_codec",
            }, {
                .tag = "flags",
            }, {
                .tag = "codec_tag",
            }, {
                .tag = "priv_class",
            }, {
                .tag = "next",
            }, {
                .tag = "priv_data_size",
            }, {
                .tag = "write_header",
            }, {
                .tag = "write_packet",
            }, {
                .tag = "write_trailer",
            }, {
                .tag = "interleave_packet",
            }, {
                .tag = "query_codec",
            },
        },
    }, [STRUCT_AVPROBEDATA] = {
        .name = "AVProbeData",
        .n_contents = 3,
        .contents = {
            {
                .tag = "filename",
            }, {
                .tag = "buf",
            }, {
                .tag = "buf_size",
            },
        },
    }, [STRUCT_URLPROTOCOL] = {
        .name = "URLProtocol",
        .n_contents = 15,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "url_open",
            }, {
                .tag = "url_open2",
            }, {
                .tag = "url_read",
            }, {
                .tag = "url_write",
            }, {
                .tag = "url_seek",
            }, {
                .tag = "url_close",
            }, {
                .tag = "next",
            }, {
                .tag = "url_read_pause",
            }, {
                .tag = "url_read_seek",
            }, {
                .tag = "url_get_file_handle",
            }, {
                .tag = "priv_data_size",
            }, {
                .tag = "priv_data_class",
            }, {
                .tag = "flags",
            }, {
                .tag = "url_check",
            },
        },
    }, [STRUCT_EBMLSYNTAX] = {
        .name = "EbmlSyntax",
        .n_contents = 5,
        .contents = {
            {
                .tag = "id",
            }, {
                .tag = "type",
            }, {
                .tag = "list_elem_size",
            }, {
                .tag = "data_offset",
            }, {
                .tag = "def",
                .struct_type_name = "EbmlSyntax:def",
            },
        },
    }, [STRUCT_EBMLSYNTAX_DEFAULTVAL] = {
        .name = "EbmlSyntax:def",
        .n_contents = 4,
        .contents = {
            {
                .tag = "u",
            }, {
                .tag = "f",
            }, {
                .tag = "s",
            }, {
                .tag = "n",
            },
        },
    }, [STRUCT_MOVATOM] = {
        .name = "MOVAtom",
        .n_contents = 2,
        .contents = {
            {
                .tag = "type",
            }, {
                .tag = "size",
            }
        },
    }, [STRUCT_AVPACKET] = {
        .name = "AVPacket",
        .n_contents = 13,
        .contents = {
            {
                .tag = "pts",
            }, {
                .tag = "dts",
            }, {
                .tag = "data",
            }, {
                .tag = "size",
            }, {
                .tag = "stream_index",
            }, {
                .tag = "flags",
            }, {
                .tag = "side_data",
            }, {
                .tag = "side_data_elems",
            }, {
                .tag = "duration",
            }, {
                .tag = "destruct",
            }, {
                .tag = "priv",
            }, {
                .tag = "pos",
            }, {
                .tag = "convergence_duration",
            },
        },
    }, [STRUCT_POLLFD] = {
        .name = "pollfd",
        .n_contents = 3,
        .contents = {
            {
                .tag = "fd",
            }, {
                .tag = "events",
            }, {
                .tag = "revents",
            },
        },
    }, [STRUCT_SYNCPOINT] = {
        .name = "Syncpoint",
        .n_contents = 3,
        .contents = {
            {
                .tag = "pos",
            }, {
                .tag = "back_ptr",
            }, {
                .tag = "ts",
            },
        },
    }, [STRUCT_OGG_CODEC] = {
        .name = "ogg_codec",
        .n_contents = 7,
        .contents = {
            {
                .tag = "magic",
            }, {
                .tag = "magicsize",
            }, {
                .tag = "name",
            }, {
                .tag = "header",
            }, {
                .tag = "packet",
            }, {
                .tag = "gptopts",
            }, {
                .tag = "granule_is_start",
            },
        },
    }, [STRUCT_RTPDYNAMICPROTOCOLHANDLER] = {
        .name = "RTPDynamicProtocolHandler",
        .n_contents = 10,
        .contents = {
            {
                .tag = "enc_name",
                .array_count = 50,
            }, {
                .tag = "codec_type",
            }, {
                .tag = "codec_id",
            }, {
                .tag = "static_payload_id",
            }, {
                .tag = "init",
            }, {
                .tag = "parse_sdp_a_line",
            }, {
                .tag = "alloc",
            }, {
                .tag = "free",
            }, {
                .tag = "parse_packet",
            }, {
                .tag = "next"
            },
        },
    }, [STRUCT_AVCODECPARSER] = {
        .name = "AVCodecParser",
        .n_contents = 7,
        .contents = {
            {
                .tag = "codec_ids",
                .array_count = 5,
            }, {
                .tag = "priv_data_size",
            }, {
                .tag = "parser_init",
            }, {
                .tag = "parser_parse",
            }, {
                .tag = "parser_close",
            }, {
                .tag = "split",
            }, {
                .tag = "next",
            },
        },
    }, [STRUCT_ELEMTOCHANNEL] = {
        .name = "elem_to_channel",
        .n_contents = 4,
        .contents = {
            {
                .tag = "av_position",
            }, {
                .tag = "syn_ele",
            }, {
                .tag = "elem_id",
            }, {
                .tag = "aac_position",
            },
        },
    }, [STRUCT_PSYMODEL] = {
        .name = "FFPsyModel",
        .n_contents = 5,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "init",
            }, {
                .tag = "window",
            }, {
                .tag = "analyze",
            }, {
                .tag = "end",
            },
        },
    }, [STRUCT_DVPROFILE] = {
        .name = "DVprofile",
        .n_contents = 19,
        .contents = {
            {
                .tag = "dsf",
            }, {
                .tag = "video_stype",
            }, {
                .tag = "frame_size",
            }, {
                .tag = "difseg_size",
            }, {
                .tag = "n_difchan",
            }, {
                .tag = "time_base",
                .struct_type_name = "AVRational",
            }, {
                .tag = "ltc_divisor",
            }, {
                .tag = "height",
            }, {
                .tag = "width",
            }, {
                .tag = "sar",
                .struct_type_name = "AVRational",
                .array_count = 2,
            }, {
                .tag = "work_chunks",
            }, {
                .tag = "idct_factor",
            }, {
                .tag = "pix_fmt",
            }, {
                .tag = "bpm",
            }, {
                .tag = "block_sizes",
            }, {
                .tag = "audio_stride",
            }, {
                .tag = "audio_min_samples",
                .array_count = 3,
            }, {
                .tag = "audio_samples_dist",
                .array_count = 5,
            }, {
                .tag = "audio_shuffle",
            },
        },
    }, [STRUCT_AVHWACCEL] = {
        .name = "AVHWAccel",
        .n_contents = 10,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "type",
            }, {
                .tag = "id",
            }, {
                .tag = "pix_fmt",
            }, {
                .tag = "capabilities",
            }, {
                .tag = "next",
            }, {
                .tag = "start_frame",
            }, {
                .tag = "decode_slice",
            }, {
                .tag = "end_frame",
            }, {
                .tag = "priv_data_size",
            },
        },
    }, [STRUCT_MACROBLOCK] = { // in escape124 decoder
        .name = "MacroBlock",
        .n_contents = 1,
        .contents = {
            {
                .tag = "pixels",
                .array_count = 4,
            },
        },
    }, [STRUCT_PIXFMTINFO] = {
        .name = "PixFmtInfo",
        .n_contents = 5,
        .contents = {
            {
                .tag = "nb_channels",
            }, {
                .tag = "color_type",
            }, {
                .tag = "pixel_type",
            }, {
                .tag = "is_alpha",
            }, {
                .tag = "depth",
            },
        },
    }, [STRUCT_AVBITSTREAMFILTER] = {
        .name = "AVBitStreamFilter",
        .n_contents = 5,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "priv_data_size",
            }, {
                .tag = "filter",
            }, {
                .tag = "close",
            }, {
                .tag = "next",
            },
        },
    }, [STRUCT_PRORESPROFILE] = {
        .name = "prores_profile",
        .n_contents = 6,
        .contents = {
            {
                .tag = "full_name",
            }, {
                .tag = "tag",
            }, {
                .tag = "min_quant",
            }, {
                .tag = "max_quant",
            }, {
                .tag = "br_tab",
                .array_count = 4,
            }, {
                .tag = "quant",
            },
        },
    }, [STRUCT_MOTIONVECT] = {
        .name = "motion_vect",
        .n_contents = 1,
        .contents = {
            {
                .tag = "d",
                .array_count = 2,
            },
        },
    }, [STRUCT_SIPRMODEPARAMS] = {
        .name = "SiprModeParam",
        .n_contents = 12,
        .contents = {
            {
                .tag = "mode_name",
            }, {
                .tag = "bits_per_frame",
            }, {
                .tag = "subframe_count",
            }, {
                .tag = "frames_per_packet",
            }, {
                .tag = "pitch_sharp_factor",
            }, {
                .tag = "number_of_fc_indexes",
            }, {
                .tag = "ma_predictor_bits",
            }, {
                .tag = "vq_indexes_bits",
                .array_count = 5,
            }, {
                .tag = "pitch_delay_bits",
                .array_count = 5,
            }, {
                .tag = "gp_index_bits",
            }, {
                .tag = "fc_index_bits",
                .array_count = 10,
            }, {
                .tag = "gc_index_bits",
            },
        },
    }, [STRUCT_BLOCKNODE] = {
        .name = "BlockNode",
        .n_contents = 6,
        .contents = {
            {
                .tag = "mx",
            }, {
                .tag = "my",
            }, {
                .tag = "ref",
            }, {
                .tag = "color",
                .array_count = 3,
            }, {
                .tag = "type",
            }, {
                .tag = "level",
            },
        },
    }, [STRUCT_TWINVQDATA] = {
        .name = "twinvq_data",
        .n_contents = 84,
        .contents = {
            {
                .tag = "lsp08",
            }, {
                .tag = "fcb08l",
            }, {
                .tag = "fcb08m",
            }, {
                .tag = "fcb08s",
            }, {
                .tag = "shape08",
            }, {
                .tag = "cb0808l0",
            }, {
                .tag = "cb0808l1",
            }, {
                .tag = "cb0808s0",
            }, {
                .tag = "cb0808s1",
            }, {
                .tag = "cb0808m0",
            }, {
                .tag = "cb0808m1",
            }, {
                .tag = "cb1108l0",
            }, {
                .tag = "cb1108l1",
            }, {
                .tag = "cb1108m0",
            }, {
                .tag = "cb1108m1",
            }, {
                .tag = "cb1108s0",
            }, {
                .tag = "cb1108s1",
            }, {
                .tag = "fcb11l",
            }, {
                .tag = "fcb11m",
            }, {
                .tag = "fcb11s",
            }, {
                .tag = "shape11",
            }, {
                .tag = "lsp11",
            }, {
                .tag = "cb1110l0",
            }, {
                .tag = "cb1110l1",
            }, {
                .tag = "cb1110m0",
            }, {
                .tag = "cb1110m1",
            }, {
                .tag = "cb1110s0",
            }, {
                .tag = "cb1110s1",
            }, {
                .tag = "fcb16l",
            }, {
                .tag = "fcb16m",
            }, {
                .tag = "fcb16s",
            }, {
                .tag = "shape16",
            }, {
                .tag = "lsp16",
            }, {
                .tag = "cb1616l0",
            }, {
                .tag = "cb1616l1",
            }, {
                .tag = "cb1616m0",
            }, {
                .tag = "cb1616m1",
            }, {
                .tag = "cb1616s0",
            }, {
                .tag = "cb1616s1",
            }, {
                .tag = "cb2220l0",
            }, {
                .tag = "cb2220l1",
            }, {
                .tag = "cb2220m0",
            }, {
                .tag = "cb2220m1",
            }, {
                .tag = "cb2220s0",
            }, {
                .tag = "cb2220s1",
            }, {
                .tag = "fcb22l_1",
            }, {
                .tag = "fcb22m_1",
            }, {
                .tag = "fcb22s_1",
            }, {
                .tag = "shape22_1",
            }, {
                .tag = "lsp22_1",
            }, {
                .tag = "cb2224l0",
            }, {
                .tag = "cb2224l1",
            }, {
                .tag = "cb2224m0",
            }, {
                .tag = "cb2224m1",
            }, {
                .tag = "cb2224s0",
            }, {
                .tag = "cb2224s1",
            }, {
                .tag = "fcb22l_2",
            }, {
                .tag = "fcb22m_2",
            }, {
                .tag = "fcb22s_2",
            }, {
                .tag = "shape22_2",
            }, {
                .tag = "lsp22_2",
            }, {
                .tag = "cb2232l0",
            }, {
                .tag = "cb2232l1",
            }, {
                .tag = "cb2232m0",
            }, {
                .tag = "cb2232m1",
            }, {
                .tag = "cb2232s0",
            }, {
                .tag = "cb2232s1",
            }, {
                .tag = "cb4440l0",
            }, {
                .tag = "cb4440l1",
            }, {
                .tag = "cb4440m0",
            }, {
                .tag = "cb4440m1",
            }, {
                .tag = "cb4440s0",
            }, {
                .tag = "cb4440s1",
            }, {
                .tag = "fcb44l",
            }, {
                .tag = "fcb44m",
            }, {
                .tag = "fcb44s",
            }, {
                .tag = "shape44",
            }, {
                .tag = "lsp44",
            }, {
                .tag = "cb4448l0",
            }, {
                .tag = "cb4448l1",
            }, {
                .tag = "cb4448m0",
            }, {
                .tag = "cb4448m1",
            }, {
                .tag = "cb4448s0",
            }, {
                .tag = "cb4448s1",
            },
        },
    }, [STRUCT_VP56MV] = {
        .name = "VP56mv",
        .n_contents = 2,
        .contents = {
            {
                .tag = "x",
            }, {
                .tag = "y",
            },
        },
    }, [STRUCT_FORMATENTRY] = {
        .name = "FormatEntry",
        .n_contents = 2,
        .contents = {
            {
                .tag = "is_supported_in",
            }, {
                .tag = "is_supported_out",
            },
        },
    }, [STRUCT_OPTIONDEF] = {
		.name = "OptionDef",
		.n_contents = 5,
		.contents = {
			{
				.tag = "name",
			}, {
				.tag = "flags",
			}, {
				.tag = "u",
				.struct_type_name = "OptionDef:u",
			}, {
				.tag = "help",
			}, {
				.tag = "argname",
			},
		},
	}, [STRUCT_OPTIONDEF_U] = {
		.name = "OptionDef:u",
		.n_contents = 1,
		.contents = {
			{
				.tag = "off",
			},
		},
	},

    /* the rest is just internal testing stuff - ignore */
    [STRUCT_S_LAYOUT] = {
        .name = "s_layout",
        .n_contents = 3,
        .contents = {
            {
                .tag = "name",
            }, {
                .tag = "n_contents",
            }, {
                .tag = "contents",
                .struct_type_name = "s_contents",
                .array_count = S_ENTRIES,
            },
        },
    }, [STRUCT_S_CONTENTS] = {
        .name = "s_contents",
        .n_contents = 3,
        .contents = {
            {
                .tag = "tag",
            }, {
                .tag = "struct_type_name",
            }, {
                .tag = "array_count",
            },
        },
    }, [STRUCT_LNUM_POS_PAIR] = {
        .name = "lnum_pos_pair",
        .n_contents = 2,
        .contents = {
            {
                .tag = "lnum",
            }, {
                .tag = "pos",
            }
        },
    }, [STRUCT_TAG_VAL_PAIR] = {
        .name = "tag_val_pair",
        .n_contents = 2,
        .contents = {
            {
                .tag = "tag",
                .array_count = LINELEN,
            }, {
                .tag = "val",
                .array_count = TVALLEN,
            },
        },
    }, [STRUCT_STATE_NIC] = {
        .name = "state:named_initializer_cache",
        .n_contents = 11,
        .contents = {
            {
                .tag = "nvp",
                .struct_type_name = "state:named_initialized_cache:nvp",
                .array_count = S_ENTRIES,
            }, {
                .tag = "obracket",
                .struct_type_name = "lnum_pos_pair",
            }, {
                .tag = "cbracket",
                .struct_type_name = "lnum_pos_pair",
            }, {
                .tag = "ret",
                .struct_type_name = "lnum_pos_pair",
            }, {
                .tag = "n_contents",
            }, {
                .tag = "tag",
                .array_count = LINELEN,
            }, {
                .tag = "struct_name",
                .array_count = LINELEN,
            }, {
                .tag = "array_index",
                .array_count = LINELEN,
            }, {
                .tag = "cast_name",
                .array_count = LINELEN,
            }, {
                .tag = "is_array",
            }, {
                .tag = "is_struct",
            },
        },
    }, [STRUCT_STATE_NIC_NVP] = {
        .name = "state:named_initialized_cache:nvp",
        .n_contents = 6,
        .contents = {
            {
                .tag = "dot",
                .struct_type_name = "lnum_pos_pair",
            }, {
                .tag = "equals",
                .struct_type_name = "lnum_pos_pair",
            }, {
                .tag = "comma",
                .struct_type_name = "lnum_pos_pair",
            }, {
                .tag = "aidxobracket",
                .struct_type_name = "lnum_pos_pair",
            }, {
                .tag = "aidxcbracket",
                .struct_type_name = "lnum_pos_pair",
            }, {
                .tag = "contents",
                .struct_type_name = "tag_val_pair",
            },
        },
    }, [STRUCT_STATE] = {
        .name = "state",
        .n_contents = 8,
        .contents = {
            {
                .tag = "parent",
                .struct_type_name = "state",
            }, {
                .tag = "struct_ctx",
                .struct_type_name = "s_layout",
            }, {
                .tag = "line",
            }, {
                .tag = "n_lines",
            }, {
                .tag = "cur",
            }, {
                .tag = "lnum",
            }, {
                .tag = "depth",
            }, {
                .tag = "named_initializer_cache",
                .struct_type_name = "state:named_initializer_cache",
            },
        },
    }, [STRUCT_ENUM_MAPPING] = {
        .name = "enum_mapping",
        .n_contents = 2,
        .contents = {
            {
                .tag = "tag",
            }, {
                .tag = "val",
            },
        },
    }
};

/* aac.h */
enum WindowSequence {
    ONLY_LONG_SEQUENCE,
    LONG_START_SEQUENCE,
    EIGHT_SHORT_SEQUENCE,
    LONG_STOP_SEQUENCE,
};

/* samplefmt.h */
enum AVSampleFormat {
    AV_SAMPLE_FMT_NONE = -1,
    AV_SAMPLE_FMT_U8,          ///< unsigned 8 bits
    AV_SAMPLE_FMT_S16,         ///< signed 16 bits
    AV_SAMPLE_FMT_S32,         ///< signed 32 bits
    AV_SAMPLE_FMT_FLT,         ///< float
    AV_SAMPLE_FMT_DBL,         ///< double
    
    AV_SAMPLE_FMT_U8P,         ///< unsigned 8 bits, planar
    AV_SAMPLE_FMT_S16P,        ///< signed 16 bits, planar
    AV_SAMPLE_FMT_S32P,        ///< signed 32 bits, planar
    AV_SAMPLE_FMT_FLTP,        ///< float, planar
    AV_SAMPLE_FMT_DBLP,        ///< double, planar
    
    AV_SAMPLE_FMT_NB           ///< Number of sample formats. DO NOT USE if linking dynamically
};

typedef enum {
    AV_CRC_8_ATM,
    AV_CRC_16_ANSI,
    AV_CRC_16_CCITT,
    AV_CRC_32_IEEE,
    AV_CRC_32_IEEE_LE,  /*< reversed bitorder version of AV_CRC_32_IEEE */
    AV_CRC_MAX,         /*< Not part of public API! Do not use outside libavutil. */
}AVCRCId;

enum PixelFormat {
    PIX_FMT_NONE= -1,
    PIX_FMT_YUV420P,   ///< planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)
    PIX_FMT_YUYV422,   ///< packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr
    PIX_FMT_RGB24,     ///< packed RGB 8:8:8, 24bpp, RGBRGB...
    PIX_FMT_BGR24,     ///< packed RGB 8:8:8, 24bpp, BGRBGR...
    PIX_FMT_YUV422P,   ///< planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
    PIX_FMT_YUV444P,   ///< planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
    PIX_FMT_YUV410P,   ///< planar YUV 4:1:0,  9bpp, (1 Cr & Cb sample per 4x4 Y samples)
    PIX_FMT_YUV411P,   ///< planar YUV 4:1:1, 12bpp, (1 Cr & Cb sample per 4x1 Y samples)
    PIX_FMT_GRAY8,     ///<        Y        ,  8bpp
    PIX_FMT_MONOWHITE, ///<        Y        ,  1bpp, 0 is white, 1 is black, in each byte pixels are ordered from the msb to the lsb
    PIX_FMT_MONOBLACK, ///<        Y        ,  1bpp, 0 is black, 1 is white, in each byte pixels are ordered from the msb to the lsb
    PIX_FMT_PAL8,      ///< 8 bit with PIX_FMT_RGB32 palette
    PIX_FMT_YUVJ420P,  ///< planar YUV 4:2:0, 12bpp, full scale (JPEG), deprecated in favor of PIX_FMT_YUV420P and setting color_range
    PIX_FMT_YUVJ422P,  ///< planar YUV 4:2:2, 16bpp, full scale (JPEG), deprecated in favor of PIX_FMT_YUV422P and setting color_range
    PIX_FMT_YUVJ444P,  ///< planar YUV 4:4:4, 24bpp, full scale (JPEG), deprecated in favor of PIX_FMT_YUV444P and setting color_range
    PIX_FMT_XVMC_MPEG2_MC,///< XVideo Motion Acceleration via common packet passing
    PIX_FMT_XVMC_MPEG2_IDCT,
    PIX_FMT_UYVY422,   ///< packed YUV 4:2:2, 16bpp, Cb Y0 Cr Y1
    PIX_FMT_UYYVYY411, ///< packed YUV 4:1:1, 12bpp, Cb Y0 Y1 Cr Y2 Y3
    PIX_FMT_BGR8,      ///< packed RGB 3:3:2,  8bpp, (msb)2B 3G 3R(lsb)
    PIX_FMT_BGR4,      ///< packed RGB 1:2:1 bitstream,  4bpp, (msb)1B 2G 1R(lsb), a byte contains two pixels, the first pixel in the byte is the one composed by the 4 msb bits
    PIX_FMT_BGR4_BYTE, ///< packed RGB 1:2:1,  8bpp, (msb)1B 2G 1R(lsb)
    PIX_FMT_RGB8,      ///< packed RGB 3:3:2,  8bpp, (msb)2R 3G 3B(lsb)
    PIX_FMT_RGB4,      ///< packed RGB 1:2:1 bitstream,  4bpp, (msb)1R 2G 1B(lsb), a byte contains two pixels, the first pixel in the byte is the one composed by the 4 msb bits
    PIX_FMT_RGB4_BYTE, ///< packed RGB 1:2:1,  8bpp, (msb)1R 2G 1B(lsb)
    PIX_FMT_NV12,      ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)
    PIX_FMT_NV21,      ///< as above, but U and V bytes are swapped
    
    PIX_FMT_ARGB,      ///< packed ARGB 8:8:8:8, 32bpp, ARGBARGB...
    PIX_FMT_RGBA,      ///< packed RGBA 8:8:8:8, 32bpp, RGBARGBA...
    PIX_FMT_ABGR,      ///< packed ABGR 8:8:8:8, 32bpp, ABGRABGR...
    PIX_FMT_BGRA,      ///< packed BGRA 8:8:8:8, 32bpp, BGRABGRA...
    
    PIX_FMT_GRAY16BE,  ///<        Y        , 16bpp, big-endian
    PIX_FMT_GRAY16LE,  ///<        Y        , 16bpp, little-endian
    PIX_FMT_YUV440P,   ///< planar YUV 4:4:0 (1 Cr & Cb sample per 1x2 Y samples)
    PIX_FMT_YUVJ440P,  ///< planar YUV 4:4:0 full scale (JPEG), deprecated in favor of PIX_FMT_YUV440P and setting color_range
    PIX_FMT_YUVA420P,  ///< planar YUV 4:2:0, 20bpp, (1 Cr & Cb sample per 2x2 Y & A samples)
    PIX_FMT_VDPAU_H264,///< H.264 HW decoding with VDPAU, data[0] contains a vdpau_render_state struct which contains the bitstream of the slices as well as various fields extracted from headers
    PIX_FMT_VDPAU_MPEG1,///< MPEG-1 HW decoding with VDPAU, data[0] contains a vdpau_render_state struct which contains the bitstream of the slices as well as various fields extracted from headers
    PIX_FMT_VDPAU_MPEG2,///< MPEG-2 HW decoding with VDPAU, data[0] contains a vdpau_render_state struct which contains the bitstream of the slices as well as various fields extracted from headers
    PIX_FMT_VDPAU_WMV3,///< WMV3 HW decoding with VDPAU, data[0] contains a vdpau_render_state struct which contains the bitstream of the slices as well as various fields extracted from headers
    PIX_FMT_VDPAU_VC1, ///< VC-1 HW decoding with VDPAU, data[0] contains a vdpau_render_state struct which contains the bitstream of the slices as well as various fields extracted from headers
    PIX_FMT_RGB48BE,   ///< packed RGB 16:16:16, 48bpp, 16R, 16G, 16B, the 2-byte value for each R/G/B component is stored as big-endian
    PIX_FMT_RGB48LE,   ///< packed RGB 16:16:16, 48bpp, 16R, 16G, 16B, the 2-byte value for each R/G/B component is stored as little-endian
    
    PIX_FMT_RGB565BE,  ///< packed RGB 5:6:5, 16bpp, (msb)   5R 6G 5B(lsb), big-endian
    PIX_FMT_RGB565LE,  ///< packed RGB 5:6:5, 16bpp, (msb)   5R 6G 5B(lsb), little-endian
    PIX_FMT_RGB555BE,  ///< packed RGB 5:5:5, 16bpp, (msb)1A 5R 5G 5B(lsb), big-endian, most significant bit to 0
    PIX_FMT_RGB555LE,  ///< packed RGB 5:5:5, 16bpp, (msb)1A 5R 5G 5B(lsb), little-endian, most significant bit to 0
    
    PIX_FMT_BGR565BE,  ///< packed BGR 5:6:5, 16bpp, (msb)   5B 6G 5R(lsb), big-endian
    PIX_FMT_BGR565LE,  ///< packed BGR 5:6:5, 16bpp, (msb)   5B 6G 5R(lsb), little-endian
    PIX_FMT_BGR555BE,  ///< packed BGR 5:5:5, 16bpp, (msb)1A 5B 5G 5R(lsb), big-endian, most significant bit to 1
    PIX_FMT_BGR555LE,  ///< packed BGR 5:5:5, 16bpp, (msb)1A 5B 5G 5R(lsb), little-endian, most significant bit to 1
    
    PIX_FMT_VAAPI_MOCO, ///< HW acceleration through VA API at motion compensation entry-point, Picture.data[3] contains a vaapi_render_state struct which contains macroblocks as well as various fields extracted from headers
    PIX_FMT_VAAPI_IDCT, ///< HW acceleration through VA API at IDCT entry-point, Picture.data[3] contains a vaapi_render_state struct which contains fields extracted from headers
    PIX_FMT_VAAPI_VLD,  ///< HW decoding through VA API, Picture.data[3] contains a vaapi_render_state struct which contains the bitstream of the slices as well as various fields extracted from headers
    
    PIX_FMT_YUV420P16LE,  ///< planar YUV 4:2:0, 24bpp, (1 Cr & Cb sample per 2x2 Y samples), little-endian
    PIX_FMT_YUV420P16BE,  ///< planar YUV 4:2:0, 24bpp, (1 Cr & Cb sample per 2x2 Y samples), big-endian
    PIX_FMT_YUV422P16LE,  ///< planar YUV 4:2:2, 32bpp, (1 Cr & Cb sample per 2x1 Y samples), little-endian
    PIX_FMT_YUV422P16BE,  ///< planar YUV 4:2:2, 32bpp, (1 Cr & Cb sample per 2x1 Y samples), big-endian
    PIX_FMT_YUV444P16LE,  ///< planar YUV 4:4:4, 48bpp, (1 Cr & Cb sample per 1x1 Y samples), little-endian
    PIX_FMT_YUV444P16BE,  ///< planar YUV 4:4:4, 48bpp, (1 Cr & Cb sample per 1x1 Y samples), big-endian
    PIX_FMT_VDPAU_MPEG4,  ///< MPEG4 HW decoding with VDPAU, data[0] contains a vdpau_render_state struct which contains the bitstream of the slices as well as various fields extracted from headers
    PIX_FMT_DXVA2_VLD,    ///< HW decoding through DXVA2, Picture.data[3] contains a LPDIRECT3DSURFACE9 pointer
    
    PIX_FMT_RGB444LE,  ///< packed RGB 4:4:4, 16bpp, (msb)4A 4R 4G 4B(lsb), little-endian, most significant bits to 0
    PIX_FMT_RGB444BE,  ///< packed RGB 4:4:4, 16bpp, (msb)4A 4R 4G 4B(lsb), big-endian, most significant bits to 0
    PIX_FMT_BGR444LE,  ///< packed BGR 4:4:4, 16bpp, (msb)4A 4B 4G 4R(lsb), little-endian, most significant bits to 1
    PIX_FMT_BGR444BE,  ///< packed BGR 4:4:4, 16bpp, (msb)4A 4B 4G 4R(lsb), big-endian, most significant bits to 1
    PIX_FMT_Y400A,     ///< 8bit gray, 8bit alpha
    PIX_FMT_BGR48BE,   ///< packed RGB 16:16:16, 48bpp, 16B, 16G, 16R, the 2-byte value for each R/G/B component is stored as big-endian
    PIX_FMT_BGR48LE,   ///< packed RGB 16:16:16, 48bpp, 16B, 16G, 16R, the 2-byte value for each R/G/B component is stored as little-endian
    PIX_FMT_YUV420P9BE, ///< planar YUV 4:2:0, 13.5bpp, (1 Cr & Cb sample per 2x2 Y samples), big-endian
    PIX_FMT_YUV420P9LE, ///< planar YUV 4:2:0, 13.5bpp, (1 Cr & Cb sample per 2x2 Y samples), little-endian
    PIX_FMT_YUV420P10BE,///< planar YUV 4:2:0, 15bpp, (1 Cr & Cb sample per 2x2 Y samples), big-endian
    PIX_FMT_YUV420P10LE,///< planar YUV 4:2:0, 15bpp, (1 Cr & Cb sample per 2x2 Y samples), little-endian
    PIX_FMT_YUV422P10BE,///< planar YUV 4:2:2, 20bpp, (1 Cr & Cb sample per 2x1 Y samples), big-endian
    PIX_FMT_YUV422P10LE,///< planar YUV 4:2:2, 20bpp, (1 Cr & Cb sample per 2x1 Y samples), little-endian
    PIX_FMT_YUV444P9BE, ///< planar YUV 4:4:4, 27bpp, (1 Cr & Cb sample per 1x1 Y samples), big-endian
    PIX_FMT_YUV444P9LE, ///< planar YUV 4:4:4, 27bpp, (1 Cr & Cb sample per 1x1 Y samples), little-endian
    PIX_FMT_YUV444P10BE,///< planar YUV 4:4:4, 30bpp, (1 Cr & Cb sample per 1x1 Y samples), big-endian
    PIX_FMT_YUV444P10LE,///< planar YUV 4:4:4, 30bpp, (1 Cr & Cb sample per 1x1 Y samples), little-endian
    PIX_FMT_YUV422P9BE, ///< planar YUV 4:2:2, 18bpp, (1 Cr & Cb sample per 2x1 Y samples), big-endian
    PIX_FMT_YUV422P9LE, ///< planar YUV 4:2:2, 18bpp, (1 Cr & Cb sample per 2x1 Y samples), little-endian
    PIX_FMT_VDA_VLD,    ///< hardware decoding through VDA
    PIX_FMT_GBRP,      ///< planar GBR 4:4:4 24bpp
    PIX_FMT_GBRP9BE,   ///< planar GBR 4:4:4 27bpp, big endian
    PIX_FMT_GBRP9LE,   ///< planar GBR 4:4:4 27bpp, little endian
    PIX_FMT_GBRP10BE,  ///< planar GBR 4:4:4 30bpp, big endian
    PIX_FMT_GBRP10LE,  ///< planar GBR 4:4:4 30bpp, little endian
    PIX_FMT_GBRP16BE,  ///< planar GBR 4:4:4 48bpp, big endian
    PIX_FMT_GBRP16LE,  ///< planar GBR 4:4:4 48bpp, little endian
    PIX_FMT_NB,        ///< number of pixel formats, DO NOT USE THIS if you want to link with shared libav* because the number of formats might differ between versions
};

typedef enum {
    EBML_NONE,
    EBML_UINT,
    EBML_FLOAT,
    EBML_STR,
    EBML_UTF8,
    EBML_BIN,
    EBML_NEST,
    EBML_PASS,
    EBML_STOP,
    EBML_TYPE_COUNT
} EbmlType;

typedef enum {
    MODE_16k,
    MODE_8k5,
    MODE_6k5,
    MODE_5k0,
    MODE_COUNT
} SiprMode;

#define DC_PRED8x8             0
#define HOR_PRED8x8            1
#define VERT_PRED8x8           2
#define PLANE_PRED8x8          3

#define tvp(x) { "" #x, x }
const static struct enum_mapping known_enums[] = {
    tvp(STRUCT_S_CONTENTS),
    tvp(STRUCT_DUMMY),
    tvp(STRUCT_S_LAYOUT),
    tvp(STRUCT_LNUM_POS_PAIR),
    tvp(STRUCT_TAG_VAL_PAIR),
    tvp(STRUCT_STATE_NIC),
    tvp(STRUCT_STATE_NIC_NVP),
    tvp(STRUCT_STATE),
    tvp(STRUCT_ENUM_MAPPING),
    tvp(STRUCT_AVRATIONAL),
    tvp(STRUCT_SAMPLEFMTINFO),
    tvp(STRUCT_AVPIXFMTDESCRIPTOR),
    tvp(STRUCT_AVCOMPONENTDESCRIPTOR),
    tvp(STRUCT_AVCODEC),
    tvp(N_STRUCTS),
    tvp(ONLY_LONG_SEQUENCE),
    tvp(LONG_START_SEQUENCE),
    tvp(EIGHT_SHORT_SEQUENCE),
    tvp(LONG_STOP_SEQUENCE),
    tvp(AV_SAMPLE_FMT_NONE),
    tvp(AV_SAMPLE_FMT_U8),
    tvp(AV_SAMPLE_FMT_S16),
    tvp(AV_SAMPLE_FMT_S32),
    tvp(AV_SAMPLE_FMT_FLT),
    tvp(AV_SAMPLE_FMT_DBL),
    tvp(AV_SAMPLE_FMT_U8P),
    tvp(AV_SAMPLE_FMT_S16P),
    tvp(AV_SAMPLE_FMT_S32P),
    tvp(AV_SAMPLE_FMT_FLTP),
    tvp(AV_SAMPLE_FMT_DBLP),
    tvp(AV_SAMPLE_FMT_NB),
    tvp(AV_CRC_8_ATM),
    tvp(AV_CRC_16_ANSI),
    tvp(AV_CRC_16_CCITT),
    tvp(AV_CRC_32_IEEE),
    tvp(AV_CRC_32_IEEE_LE),
    tvp(AV_CRC_MAX),
    tvp(PIX_FMT_NONE),
    tvp(PIX_FMT_YUV420P),
    tvp(PIX_FMT_YUYV422),
    tvp(PIX_FMT_RGB24),
    tvp(PIX_FMT_BGR24),
    tvp(PIX_FMT_YUV422P),
    tvp(PIX_FMT_YUV444P),
    tvp(PIX_FMT_YUV410P),
    tvp(PIX_FMT_YUV411P),
    tvp(PIX_FMT_GRAY8),
    tvp(PIX_FMT_MONOWHITE),
    tvp(PIX_FMT_MONOBLACK),
    tvp(PIX_FMT_PAL8),
    tvp(PIX_FMT_YUVJ420P),
    tvp(PIX_FMT_YUVJ422P),
    tvp(PIX_FMT_YUVJ444P),
    tvp(PIX_FMT_XVMC_MPEG2_MC),
    tvp(PIX_FMT_XVMC_MPEG2_IDCT),
    tvp(PIX_FMT_UYVY422),
    tvp(PIX_FMT_UYYVYY411),
    tvp(PIX_FMT_BGR8),
    tvp(PIX_FMT_BGR4),
    tvp(PIX_FMT_BGR4_BYTE),
    tvp(PIX_FMT_RGB8),
    tvp(PIX_FMT_RGB4),
    tvp(PIX_FMT_RGB4_BYTE),
    tvp(PIX_FMT_NV12),
    tvp(PIX_FMT_NV21),
    tvp(PIX_FMT_ARGB),
    tvp(PIX_FMT_RGBA),
    tvp(PIX_FMT_ABGR),
    tvp(PIX_FMT_BGRA),
    tvp(PIX_FMT_GRAY16BE),
    tvp(PIX_FMT_GRAY16LE),
    tvp(PIX_FMT_YUV440P),
    tvp(PIX_FMT_YUVJ440P),
    tvp(PIX_FMT_YUVA420P),
    tvp(PIX_FMT_VDPAU_H264),
    tvp(PIX_FMT_VDPAU_MPEG1),
    tvp(PIX_FMT_VDPAU_MPEG2),
    tvp(PIX_FMT_VDPAU_WMV3),
    tvp(PIX_FMT_VDPAU_VC1),
    tvp(PIX_FMT_RGB48BE),
    tvp(PIX_FMT_RGB48LE),
    tvp(PIX_FMT_RGB565BE),
    tvp(PIX_FMT_RGB565LE),
    tvp(PIX_FMT_RGB555BE),
    tvp(PIX_FMT_RGB555LE),
    tvp(PIX_FMT_BGR565BE),
    tvp(PIX_FMT_BGR565LE),
    tvp(PIX_FMT_BGR555BE),
    tvp(PIX_FMT_BGR555LE),
    tvp(PIX_FMT_VAAPI_MOCO),
    tvp(PIX_FMT_VAAPI_IDCT),
    tvp(PIX_FMT_VAAPI_VLD),
    tvp(PIX_FMT_YUV420P16LE),
    tvp(PIX_FMT_YUV420P16BE),
    tvp(PIX_FMT_YUV422P16LE),
    tvp(PIX_FMT_YUV422P16BE),
    tvp(PIX_FMT_YUV444P16LE),
    tvp(PIX_FMT_YUV444P16BE),
    tvp(PIX_FMT_VDPAU_MPEG4),
    tvp(PIX_FMT_DXVA2_VLD),
    tvp(PIX_FMT_RGB444LE),
    tvp(PIX_FMT_RGB444BE),
    tvp(PIX_FMT_BGR444LE),
    tvp(PIX_FMT_BGR444BE),
    tvp(PIX_FMT_Y400A),
    tvp(PIX_FMT_BGR48BE),
    tvp(PIX_FMT_BGR48LE),
    tvp(PIX_FMT_YUV420P9BE),
    tvp(PIX_FMT_YUV420P9LE),
    tvp(PIX_FMT_YUV420P10BE),
    tvp(PIX_FMT_YUV420P10LE),
    tvp(PIX_FMT_YUV422P10BE),
    tvp(PIX_FMT_YUV422P10LE),
    tvp(PIX_FMT_YUV444P9BE),
    tvp(PIX_FMT_YUV444P9LE),
    tvp(PIX_FMT_YUV444P10BE),
    tvp(PIX_FMT_YUV444P10LE),
    tvp(PIX_FMT_YUV422P9BE),
    tvp(PIX_FMT_YUV422P9LE),
    tvp(PIX_FMT_VDA_VLD),
    tvp(PIX_FMT_GBRP),
    tvp(PIX_FMT_GBRP9BE),
    tvp(PIX_FMT_GBRP9LE),
    tvp(PIX_FMT_GBRP10BE),
    tvp(PIX_FMT_GBRP10LE),
    tvp(PIX_FMT_GBRP16BE),
    tvp(PIX_FMT_GBRP16LE),
    tvp(PIX_FMT_NB),
    tvp(EBML_NONE),
    tvp(EBML_UINT),
    tvp(EBML_FLOAT),
    tvp(EBML_STR),
    tvp(EBML_UTF8),
    tvp(EBML_BIN),
    tvp(EBML_NEST),
    tvp(EBML_PASS),
    tvp(EBML_STOP),
    tvp(MODE_16k),
    tvp(MODE_8k5),
    tvp(MODE_6k5),
    tvp(MODE_5k0),
    tvp(MODE_COUNT),
    tvp(DC_PRED8x8),
    tvp(HOR_PRED8x8),
    tvp(VERT_PRED8x8),
    tvp(PLANE_PRED8x8),
    { NULL }
};

/*
 * Locate in *s1 any of the chars in *s2, and return a pointer
 * to it, or NULL if none.
 *
 * s2 is NULL-terminated.
 */
static char *strsbrk(char *s1, const char **s2)
{
    char *res = NULL;

    while (*s2) {
        char *p = strstr(s1, *s2++);
        if (!res || (p && p < res))
            res = p;
    }

    return res;
}

struct lnum_pos_pair {
    int lnum, pos;
};

struct tag_val_pair {
    char tag[LINELEN];
    char val[TVALLEN];
};

struct state {
    struct state *parent;
    const struct s_layout *struct_ctx;
    char **line;
    int n_lines;
    char *cur;
    int lnum;
    int depth;
    struct {
        struct {
            struct lnum_pos_pair dot, equals, comma, aidxobracket, aidxcbracket;
            struct tag_val_pair contents;
        } nvp[S_ENTRIES];
        struct lnum_pos_pair obracket, cbracket, ret, qmark, colon, if_st, while_st, for_st, do_st, else_st;
        int n_contents;
        char tag[LINELEN];
        char struct_name[LINELEN];
        char array_index[LINELEN];
        char cast_name[LINELEN];
        int is_array, is_struct;
        int bracket_type;
        int start_index;
        int add_closing_bracket;
    } named_initializer_cache;
};

/*
 * Handle named initializer replaces.
 */
static void add_contents(struct state *s, char *val,
                         struct lnum_pos_pair *from,
                         struct lnum_pos_pair *to)
{
    int l, p, f = 1;

    val[0] = 0;
    for (l = from->lnum, p = from->pos + 1;
         l < to->lnum || (l == to->lnum && p < to->pos);) {
        char *ptr = s->line[l] + p;
        if (f) {
            while (isspace(*ptr))
                ptr++;
        }
        if (strlen(val) + strlen(ptr) >= TVALLEN) {
            fprintf(stderr, "Overflow in tval string, increase TVALLEN\n");
            exit(1);
        }
        strcat(val, ptr);
        if (l == to->lnum) {
            int cut = strlen(s->line[l]) - to->pos;
            cut = strlen(val) - cut;
            val[cut] = 0;
        }
        p = 0; l++; f = 0;
    }
    s->named_initializer_cache.n_contents++;
}

static void add_tag_val_pair(struct state *s, const char *src, const char *type)
{
    int idx = s->named_initializer_cache.n_contents;

    if (idx >= S_ENTRIES) {
        fprintf(stderr, "Ran out of space adding a struct entry - increase S_ENTRIES\n");
        exit(1);
    }

    strcpy(s->named_initializer_cache.nvp[idx].contents.tag, src);
    add_contents(s,
                 s->named_initializer_cache.nvp[idx].contents.val,
                 &s->named_initializer_cache.nvp[idx].equals,
                 &s->named_initializer_cache.nvp[idx].comma);
    DEBUG fprintf(stderr,
                  "Added %s[%d] tag='%s' val='%s' (%d:%d-%d:%d), struct_ctx='%s'\n",
                  type, idx,
                  s->named_initializer_cache.nvp[idx].contents.tag,
                  s->named_initializer_cache.nvp[idx].contents.val,
                  s->named_initializer_cache.nvp[idx].equals.lnum,
                  s->named_initializer_cache.nvp[idx].equals.pos,
                  s->named_initializer_cache.nvp[idx].comma.lnum,
                  s->named_initializer_cache.nvp[idx].comma.pos,
                  s->struct_ctx ? s->struct_ctx->name : "(no struct)");
}

static void add_tag_val_pair_array(struct state *s)
{
    add_tag_val_pair(s, s->named_initializer_cache.array_index, "array");
    s->named_initializer_cache.is_array = 1;
}

static void add_tag_val_pair_struct(struct state *s)
{
    add_tag_val_pair(s, s->named_initializer_cache.tag, "struct");
    s->named_initializer_cache.is_struct = 1;
}

static void add_compound_literal_value(struct state *s)
{
    int idx = s->named_initializer_cache.n_contents;
    struct lnum_pos_pair start;
    
    if (idx >= S_ENTRIES) {
        fprintf(stderr, "Ran out of space adding a struct entry - increase S_ENTRIES\n");
        exit(1);
    }

    if (s->named_initializer_cache.nvp[idx].dot.lnum != -1 &&
        s->named_initializer_cache.nvp[idx].equals.lnum != -1) {
        struct lnum_pos_pair *dot = &s->named_initializer_cache.nvp[idx].dot;
        char *ptr = s->line[dot->lnum] + dot->pos + 1;
        int nchars;
        start = s->named_initializer_cache.nvp[idx].equals;
        if (dot->lnum != start.lnum) {
            fprintf(stderr, "Compound named initializer with dot/equals not on same line, not supported\n");
            exit(1);
        }
        nchars = start.pos - dot->pos - 1;
        if (nchars < 0) return; // can happen during parameter declaration in function prologue
        while (nchars > 0 && isspace(ptr[nchars-1])) nchars--;
        strncpy(s->named_initializer_cache.nvp[idx].contents.tag,
                ptr, nchars);
    } else {
        start = idx == 0 ? s->named_initializer_cache.obracket :
                           s->named_initializer_cache.nvp[idx - 1].comma;
        s->named_initializer_cache.nvp[idx].contents.tag[0] = 0;
    }
    add_contents(s, s->named_initializer_cache.nvp[idx].contents.val,
                 &start, &s->named_initializer_cache.nvp[idx].comma);
    DEBUG fprintf(stderr,
                  "Added compound literal[%d] value='%s' for cast '%s' (tag='%s'), dot=%d\n",
                  idx,
                  s->named_initializer_cache.nvp[idx].contents.val,
                  s->parent->parent->named_initializer_cache.cast_name,
                  s->named_initializer_cache.nvp[idx].contents.tag,
                  s->named_initializer_cache.nvp[idx].dot.lnum);
}

static void backup_line_intermezzos(struct state *s,
                                    char buf[S_ENTRIES][LINELEN], int n, int n_cnt,
                                    struct lnum_pos_pair *lpp,
                                    struct lnum_pos_pair *lpn)
{
    int is_line_end, lnum, pos = lpp->pos, len, off = 0;
    int last_lnum = (n < n_cnt - 1) ? lpn->lnum : lpp->lnum;
    
    buf[n][0] = 0;
    for (lnum = lpp->lnum; lnum <= last_lnum; lnum++) {
        if (n < n_cnt - 1 && lnum < lpn->lnum) {
            is_line_end = 1;
        } else if (n == n_cnt - 1) {
            is_line_end = 1;
        } else {
            is_line_end = 0;
        }
        
        if (is_line_end) {
            len = strlen(s->line[lnum] + pos);
        } else {
            len = lpn->pos - pos;
        }
        if (off + len + 1 > LINELEN) abort();
        memcpy(buf[n] + off, s->line[lnum] + pos, len);
        off += len;
        pos = 0;
    }
    buf[n][off] = 0;
    DEBUG fprintf(stderr, "Backed up '%s'\n", buf[n]);
}

static void copy_multi_line(struct state *s,
                            struct lnum_pos_pair *pos,
                            char *val)
{
    int len = strlen(val);
    char *ptr = val;

    while (len > 0) {
        char *off = strchr(ptr, '\n');
        if (!off) {
            if (pos->pos + len + 1 > LINELEN) abort();
            memcpy(s->line[pos->lnum] + pos->pos, ptr, len);
            s->line[pos->lnum][pos->pos + len] = 0;
            DEBUG fprintf(stderr, "Copying remainder (%d): %s\n",
                          len, s->line[pos->lnum] + pos->pos);
            pos->pos += len;
            break;
        } else {
            int slen = (int) (off - ptr + 1);
            if (pos->pos + slen + 1 > LINELEN) abort();
            memcpy(s->line[pos->lnum] + pos->pos, ptr, slen);
            s->line[pos->lnum][pos->pos + slen] = 0;
            DEBUG fprintf(stderr, "Copying %d chars until newline[%d]: %s",
                          slen, pos->lnum, s->line[pos->lnum] + pos->pos);
            DEBUG fprintf(stderr, "Finished line[%d/%d]: %s",
                          pos->lnum, pos->pos, s->line[pos->lnum]);
            pos->lnum++;
            pos->pos = 0;
            len -= slen;
            ptr += slen;
        }
    }
}

static void replace_named_initializer_array(struct state *s)
{
    int idx[S_ENTRIES], n, max = -1, m,
        n_cnt = s->named_initializer_cache.n_contents;
    char buf[S_ENTRIES][LINELEN];
    struct lnum_pos_pair pos = s->named_initializer_cache.nvp[0].aidxobracket;
    struct lnum_pos_pair *lastcomma = &s->named_initializer_cache.nvp[n_cnt - 1].comma;

    for (n = 0; n < S_ENTRIES; n++)
        idx[n] = -1;

    // backup line endings for each line
    for (n = 0; n < n_cnt; n++) {
        struct lnum_pos_pair *lpp = &s->named_initializer_cache.nvp[n].comma,
                             *lpn = &s->named_initializer_cache.nvp[n + 1].aidxobracket;
        backup_line_intermezzos(s, buf, n, n_cnt, lpp, lpn);
    }

    // Currently supports enums or number literals
    for (n = 0; n < n_cnt; n++) {
        char *idxstr = s->named_initializer_cache.nvp[n].contents.tag;
        int operator = 0, result;
        char orig[LINELEN];

        strcpy(orig, idxstr);
        while (idxstr[0] != 0) {
            const struct enum_mapping *e;
            char *ptr = strpbrk(idxstr, "-+"), op;
            int idxlen;
            int p = 0;

            if (!ptr) {
                ptr = memchr(idxstr, 0, LINELEN);
                op = 0;
            } else {
                op = (*ptr == '+') ? 1 : -1;
                *ptr = 0;
            }
            while (ptr - p > idxstr && ptr[-p] == ' ')
                ptr[-(p++)] = 0;
            idxlen = strlen(idxstr);

            for (e = known_enums; e->tag != NULL; e++) {
                if (!strcmp(e->tag, idxstr))
                    break;
            }

            if (e->tag) {
                // enums
                if (!operator) {
                    result = e->val;
                } else {
                    result += e->val * operator;
                }
            } else {
                for (m = 0; m < idxlen; m++) {
                    if (idxstr[m] < '0' || idxstr[m] > '9')
                        break;
                }
                if (m < idxlen) {
                    if (idxlen == 3 && idxstr[0] == '\'' && idxstr[2] == '\'') {
                        // 'x' characters
                        m = idxstr[1];
                    } else {
                        fprintf(stderr,
                                "Unknown enum tag '%s'\n", idxstr);
                        exit(1);
                    }
                } else {
                    // number
                    m = atoi(idxstr);
                }
                if (!operator) {
                    result = m;
                } else {
                    result += m * operator;
                }
            }

            idxstr = op ? ptr + 1 : ptr;
            while (*idxstr == ' ') idxstr++;
            operator = op;
        }

        if (result < 0 || result >= S_ENTRIES) {
            fprintf(stderr,
                    "Invalid tag/number value %d for '%s', not in 0-%d range (increase S_ENTRIES)\n",
                    result, orig, S_ENTRIES);
            exit(1);
        }
        
        idx[result] = n;
        max = max > result ? max : result;
    }

    for (n = 0, m = 0; n <= max; n++) {
        if (idx[n] >= 0) {
            char *idxtag = s->named_initializer_cache.nvp[idx[n]].contents.tag;
            if (idxtag[0]) {
                copy_multi_line(s, &pos, "/* ");
                copy_multi_line(s, &pos, idxtag);
                copy_multi_line(s, &pos, " */ ");
            }
            copy_multi_line(s, &pos,
                            s->named_initializer_cache.nvp[idx[n]].contents.val);
            copy_multi_line(s, &pos, buf[m++]);
        } else {
            if (pos.pos + 4 + 1 > LINELEN) abort();
            if (s->struct_ctx) {
                // FIXME the struct may contain a sub-struct, then we need
                // something like { { 0 } }; we can probably derive this
                // from the struct_ctx
                memcpy(s->line[pos.lnum] + pos.pos, "{ 0 }, ", 7);
                pos.pos += 7;
            } else {
                memcpy(s->line[pos.lnum] + pos.pos, "0, ", 3);
                pos.pos += 3;
            }
            s->line[pos.lnum][pos.pos] = 0;
        }
    }

    // the -1 is normally not right, but adjusts for the "terminal
    // comma emulation" which brings us one line further and is
    // adjusted for in line backupping anyway
    if (pos.lnum - 1 > s->named_initializer_cache.cbracket.lnum) {
        fprintf(stderr, "Wrote too many lines (%d > %d)\n",
                pos.lnum, s->named_initializer_cache.cbracket.lnum);
        exit(1);
    }
    while (pos.lnum <= lastcomma->lnum) {
        DEBUG fprintf(stderr, "Emptying line [%d/%d comma=%d,%d]: %s",
                      pos.lnum, pos.pos,
                      lastcomma->lnum, lastcomma->pos, s->line[pos.lnum]);
        s->line[pos.lnum][pos.pos] = 0;
        pos.lnum++;
        pos.pos = 0;
    }
}

// FIXME if the last value is an array, and takes multiple lines, we only
// save the line endings until the last line of its content, not the line
// of the closign bracket, which could be one more. this causes pixdesc.c
// last line to be lost in state and thus the transform doesn't occur
static void replace_named_initializer_struct(struct state *s)
{
    const struct s_layout *str = s->struct_ctx;
    int n, m, n_cnt = s->named_initializer_cache.n_contents,
        m_cnt = str->n_contents;
    char buf[S_ENTRIES][LINELEN];
    struct lnum_pos_pair pos = s->named_initializer_cache.nvp[0].dot;
    struct lnum_pos_pair *lastcomma = &s->named_initializer_cache.nvp[n_cnt - 1].comma;
    int n_compl = 0;
    int fnum = (int) (s->cur - s->line[s->lnum]), flen = strlen(s->line[s->lnum]);

    // backup line endings for each line
    for (n = 0; n < n_cnt; n++) {
        struct lnum_pos_pair *lpp = &s->named_initializer_cache.nvp[n].comma,
                             *lpn = &s->named_initializer_cache.nvp[n + 1].dot;
        backup_line_intermezzos(s, buf, n, n_cnt, lpp, lpn);
    }

    // search in 'val' for |.ABC = DEF, [..], .UVW = XYZ|, and replace the
    // stuff within the || with just the values, ordered.
    DEBUG fprintf(stderr, "Starting at index %d\n", s->named_initializer_cache.start_index);
    for (m = s->named_initializer_cache.start_index; n_compl < n_cnt && m < m_cnt; m++) {
        const struct s_contents *content = &str->contents[m];
        struct tag_val_pair *tvp;
        for (n = 0; n < n_cnt; n++) {
            tvp = &s->named_initializer_cache.nvp[n].contents;
            if (!strcmp(tvp->tag, content->tag))
                break;
        }

        fprintf(stderr, "Tested %s, found idx=%d of %d (m=%d/%d, n=%d/%d)\n",content->tag, n, n_cnt, m, m_cnt, n_compl, n_cnt);
        if (n == n_cnt) {
            int len = strlen(content->tag);
            // no value, zero
            if (pos.pos + 5 + 5 + len + 1 > LINELEN) {
                fprintf(stderr, "No space left, curpos=%d, len=%d, LINELEN=%d (%s)\n",
                        pos.pos, len, LINELEN, s->line[pos.lnum]);
                abort();
            }
            if (content->array_count) {
                if (content->struct_type_name) {
                    memcpy(s->line[pos.lnum] + pos.pos, "{{ 0 }}", 7);
                    pos.pos += 7;
                } else {
                    memcpy(s->line[pos.lnum] + pos.pos, "{ 0 }", 5);
                    pos.pos += 5;
                }
            } else {
                if (content->struct_type_name) {
                    memcpy(s->line[pos.lnum] + pos.pos, "{ 0 }", 5);
                    pos.pos += 5;
                } else {
                    memcpy(s->line[pos.lnum] + pos.pos, "0", 1);
                    pos.pos += 1;
                }
            }
#if 0 // for debugging only, it leads to occasional overflows
            memcpy(s->line[pos.lnum] + pos.pos, " /* ", 4);
            pos.pos += 4;
            memcpy(s->line[pos.lnum] + pos.pos, content->tag, len);
            pos.pos += len;
            memcpy(s->line[pos.lnum] + pos.pos, " */, ", 5);
            pos.pos += 5;
#else
            memcpy(s->line[pos.lnum] + pos.pos, ", ", 2);
            pos.pos += 2;
#endif
            s->line[pos.lnum][pos.pos] = 0;
        } else {
            copy_multi_line(s, &pos, tvp->val);
            copy_multi_line(s, &pos, buf[n_compl++]);
        }
    }

    if (m == m_cnt && n_compl < n_cnt) {
        fprintf(stderr, "Probably a misdefined struct, didn't completely write out\n");
        //exit(1);
    }

    // the -1 is normally not right, but adjusts for the "terminal
    // comma emulation" which brings us one line further and is
    // adjusted for in line backupping anyway
    if (pos.lnum - 1 > s->named_initializer_cache.cbracket.lnum) {
        fprintf(stderr, "Wrote too many lines (%d > %d)\n",
                pos.lnum, s->named_initializer_cache.cbracket.lnum);
        exit(1);
    }
    while (pos.lnum <= lastcomma->lnum) {
        DEBUG fprintf(stderr, "Emptying line [%d/%d comma=%d,%d]: %s",
                      pos.lnum, pos.pos,
                      lastcomma->lnum, lastcomma->pos, s->line[pos.lnum]);
        s->line[pos.lnum][pos.pos] = 0;
        pos = (struct lnum_pos_pair) { pos.lnum + 1, 0 };
        if (s->lnum <= lastcomma->lnum)
            s->lnum--;
    }
    s->cur = s->line[s->lnum] + strlen(s->line[s->lnum]) - (flen - fnum);
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
    DEBUG { for (n = MAX(0, s->named_initializer_cache.nvp[0].dot.lnum - 1);
                 n <= MIN(s->n_lines, lastcomma->lnum + 1); n++) {
                fprintf(stderr, "[%d] %s", n, s->line[n]);
            }
          }
}

static const struct s_layout *find_struct(const char *name)
{
    const struct s_layout *p;
    int idx;
    char buf[256];
    char *q;

    if ((q = strrchr(name, '['))) {
        memcpy(buf, name, q - name);
        buf[q - name] = 0;
        name = buf;
    }
    for (idx = 0; idx < N_STRUCTS; idx++) {
        p = &known_structs[idx];
        if (!p->name) continue; // test for something else, ignore
        if (!strcmp(p->name, name)) {
            return p;
        }
    }
    
    DEBUG fprintf(stderr, "Could not find definition for struct '%s'\n",
                  name);
    
    return NULL;
}

static int is_recognized_struct(const char *name)
{
    return !strcmp(name, "AVFilterPad[]") || !strcmp(name, "const uint8_t[]") || !strcmp(name, "motion_vect") || !strcmp(name, "const int[]");
}

static void find_value_name(struct state *s, struct state *comp, const char *varname, char *ptr,
                             struct lnum_pos_pair *from, struct lnum_pos_pair *to)
{
    if (comp->named_initializer_cache.n_contents > 0) {
        // used pre-parsed elements
        char *cast_name = s->parent->parent->named_initializer_cache.cast_name;
        const struct s_layout *str =
            find_struct(strncmp(cast_name, "struct ", 7) ? cast_name : cast_name + 7);
        int n_cnt = s->named_initializer_cache.n_contents, n;
        int varlen = strlen(varname);

        for (n = 0; n < n_cnt; n++) {
            char *val = s->named_initializer_cache.nvp[n].contents.val;
            const struct s_contents *content = &str->contents[n];
            char *tag = content->tag;
            int taglen = strlen(tag), vallen = strlen(val);

            if (varname[0] == '*') {
                if (varname[1] == '*' || varname[1] == '(')
                    *ptr++ = '(';
                memcpy(ptr, varname + 1, varlen - 1);
                ptr += varlen - 1;
                if (varname[1] == '*' || varname[1] == '(')
                    *ptr++ = ')';
                *ptr++ = '-';
                *ptr++ = '>';
            } else {
                if (varname[0] == '(')
                    *ptr++ = '(';
                memcpy(ptr, varname, varlen);
                ptr += varlen;
                if (varname[0] == '(')
                    *ptr++ = ')';
                *ptr++ = '.';
            }
            memcpy(ptr, tag, taglen);
            ptr += taglen;
            *ptr++ = ' ';
            *ptr++ = '=';
            *ptr++ = ' ';
            memcpy(ptr, val, vallen);
            ptr += vallen;
            *ptr++ = ';';
        }
    } else {
        int l1 = from->lnum, p1 = from->pos + 1;
        int l2 = to->lnum, p2 = to->pos;

        do {
            while (s->line[l1][p1] && (isspace(s->line[l1][p1]) || s->line[l1][p1] == '\n')) p1++;
            if (!s->line[l1][p1]) {
                p1 = 0;
                l1++;
            } else
                break;
        } while (1);
        do {
            while (s->line[l2][p2] && l2 > 0 && (isspace(s->line[l2][p2 - 1]) || s->line[l2][p2 - 1] == '\n')) p2--;
            if (p2 == 0) {
                l2--;
                p2 = strlen(s->line[l2]);
            } else
                break;
        } while (1);
        if (l2 != l1) {
            fprintf(stderr, "%d:%d/%d - %d:%d/%d Implement support for multi-line values in a=b?c:d compound literals\n",
                l1, p1, strlen(s->line[l1]), l2, p2, strlen(s->line[l2]));
            exit(1);
        }
        strcpy(ptr, varname);
        ptr += strlen(varname);
        strcpy(ptr, " = ");
        ptr += 3;
        strncpy(ptr, s->line[l1] + p1, p2 - p1);
        ptr += p2 - p1;
        *ptr++ = ';';
    }

    *ptr = 0;
}

// s is the target, a contains the '=', ':' and '?' elements, b/c contain the content for
// the var = cond ? b : c; parts
// FIXME backup/copy_multi_line support
static void replace_compound_initializer_conditional(struct state *s, struct state *a, struct state *b, struct state *c)
{
    struct lnum_pos_pair *eq, *qm, *col;
    char *ptr, *ptr2, *ptr3;
    int n, oldchar, indent, last_lnum;
    char varname[LINELEN], cond[LINELEN], aname[LINELEN], bname[LINELEN];
    struct lnum_pos_pair pos;

    DEBUG fprintf(stderr, "Replacing conditional compound literal assignment\n");
    eq  = &a->named_initializer_cache.nvp[0].equals;
    qm  = &a->named_initializer_cache.qmark;
    col = &a->named_initializer_cache.colon;
    // FIXME the variable name could probably have been split over multiple
    // lines; it's also possible that multiple assignments/things were all
    // in a single line. all of that won't work right now
    ptr = s->line[eq->lnum];
    for (n = eq->pos; n > 1 && isspace(ptr[n - 1]); n--);
    oldchar = ptr[n];
    ptr[n] = 0;
    if ((ptr2 = strrchr(ptr, ';'))) {
        ptr2++;
    } else {
        ptr2 = ptr;
    }
    while (isspace(*ptr2))
        ptr2++;
    ptr3 = ptr2;
    indent = (int) (ptr3 - s->line[eq->lnum]);
    strcpy(varname, ptr2);
    ptr[n] = oldchar;

    ptr = s->line[qm->lnum];
    for (n = qm->pos; n > 1 && isspace(ptr[n - 1]); n--);
    oldchar = ptr[n];
    ptr[n] = 0;
    if ((ptr2 = strrchr(ptr, '='))) {
        ptr2++;
    } else {
        ptr2 = ptr;
    }
    while (isspace(*ptr2))
        ptr2++;
    strcpy(cond, ptr2);
    ptr[n] = oldchar;

    find_value_name(s, b, varname, aname,
                           &a->named_initializer_cache.qmark,
                           &a->named_initializer_cache.colon);
    pos.lnum = s->lnum;
    pos.pos  = 1 + (int) (s->cur - s->line[s->lnum]);
    find_value_name(s, c, varname, bname,
                           &a->named_initializer_cache.colon, &pos);
    strcpy(ptr3, "{ if (");
    ptr3 += 6;
    strcpy(ptr3, cond);
    ptr3 += strlen(cond);
    strcpy(ptr3, ") { ");
    ptr3 += 4;
    strcpy(ptr3, aname);
    ptr3 += strlen(aname);
    strcpy(ptr3, " }");
    ptr3 += 2;
    if (eq->lnum < s->named_initializer_cache.cbracket.lnum) {
        last_lnum = eq->lnum + 1;
        *ptr3++ = '\n';
        *ptr3++ = 0;
        ptr3 = s->line[last_lnum];
        indent += 2;
        while (indent--)
            *ptr3++ = ' ';
    } else {
        last_lnum = eq->lnum;
        *ptr3++ = ' ';
    }
    strcpy(ptr3, "else { ");
    ptr3 += 7;
    strcpy(ptr3, bname);
    ptr3 += strlen(bname);
    strcpy(ptr3, " } }\n");
    ptr3 += 5;

    // zero remaining lines
    for (n = last_lnum + 1; n <= s->named_initializer_cache.cbracket.lnum; n++)
        s->line[n][0] = 0;
}

static void backup_multi_line(struct state *s, char *val, int size,
                              struct lnum_pos_pair *from, struct lnum_pos_pair *to)
{
    int l, p;
    char *end = val + size;

    for (l = from->lnum, p = from->pos; l <= to->lnum; l++, p = 0) {
        int len = l == to->lnum ? to->pos - p : strlen(s->line[l] + p);

        if (val + len >= end) abort();
        memcpy(val, s->line[l] + p, len);
        val += len;
    }
    *val = 0;
}

static int cntr = 0;
static void replace_compound_initializer(struct state *s)
{
    char *cast_name = s->parent->parent->named_initializer_cache.cast_name;

    // support function((const float[2]) { 1.0, 2.0 });
    // which is changed to:
    // static const float tmp__X[2] = { 1.0, 2.0 };
    // [..]
    // function(tmp__x);
    if ((strstr(cast_name, "const") && !strstr(cast_name, "*[]")) ||
        !strcmp(cast_name, "AVFilterPad[]")) {
        struct lnum_pos_pair cb, *ob, cp, ls;
        char *array_start = strchr(cast_name, '[');
        char tmpname[LINELEN], linestart[LINELEN], lineend[LINELEN], val[TVALLEN];
        int n, padding = strlen(s->line[s->lnum]) - (int) (s->cur - s->line[s->lnum]);

        // temporary variable name
        sprintf(tmpname, "tmp__%d", cntr++);
        ob = &s->named_initializer_cache.obracket;
        cb =  s->named_initializer_cache.cbracket;
        cb.pos++;
        backup_multi_line(s, val, sizeof(val), ob, &cb);

        // the variable itself will be declared outside any context
        if (!array_start) {
            fprintf(stdout,
                    "static %s %s = ",
                    cast_name, tmpname);
        } else {
            *array_start = 0;
            fprintf(stdout,
                    "static %s %s[%s = ",
                    cast_name, tmpname, array_start + 1);
        }
        fprintf(stdout, "%s", val);
        fprintf(stdout, ";\n");
        if (array_start)
            *array_start = '[';

        ls = s->parent->parent->named_initializer_cache.obracket;
        ls.pos = 0;
        ob = &s->parent->parent->named_initializer_cache.obracket;
        backup_multi_line(s, linestart, sizeof(linestart), &ls, ob);
        cp = cb;
        cp.pos = strlen(s->line[cp.lnum]) + 1;
        backup_multi_line(s, lineend, sizeof(lineend), &cb, &cp);
        copy_multi_line(s, &ls, linestart);
        copy_multi_line(s, &ls, tmpname);
        copy_multi_line(s, &ls, lineend);

        // zero other lines
        if (ls.pos != 0) abort();
        for (n = ls.lnum; n <= cb.lnum; n++)
            s->line[n][0] = 0;
        s->lnum = ls.lnum - 1;
        s->cur = s->line[s->lnum] + strlen(s->line[s->lnum]) - padding;

    // support AVRational var; var = (AVRational) { 1, 2 };
    // which is changed to
    // AVRational var; { var.num = 1; var.den = 2; }
    // FIXME copy/backup_multi_line support
    } else if (s->parent->parent->parent &&
               s->parent->parent->parent->named_initializer_cache.n_contents == 0 &&
               s->parent->parent->parent->named_initializer_cache.nvp[0].equals.lnum != -1) {
        struct lnum_pos_pair *eq;
        char lineend[LINELEN], varname[LINELEN], *ptr, *ptr2, *ptr3, *varptr[4];
        int endpos, n_cnt = s->named_initializer_cache.n_contents, n, varlen[4], m;
        const struct s_layout *str =
            find_struct(strncmp(cast_name, "struct ", 7) ? cast_name : cast_name + 7);

        if (s->parent->parent->parent->named_initializer_cache.qmark.lnum != -1) {
            if (s->parent->parent->parent->named_initializer_cache.colon.lnum == -1) {
                DEBUG fprintf(stderr, "First half of a x=c?a:b - backup and continue (FIXME test this)\n");
                for (n = 0; n < n_cnt; n++) {
                    strcpy(s->parent->parent->parent->named_initializer_cache.nvp[n].contents.val,
                           s->named_initializer_cache.nvp[n].contents.val);
                }
                s->parent->parent->parent->named_initializer_cache.n_contents = n_cnt;
                return;
            } else {
                DEBUG fprintf(stderr, "Time for second half now (%d+%d)\n",
                    s->parent->parent->parent->named_initializer_cache.n_contents,
                    s->named_initializer_cache.n_contents);
                replace_compound_initializer_conditional(s, s->parent->parent->parent, s->parent->parent->parent, s);
                return;
            }
        }

        eq = &s->parent->parent->parent->named_initializer_cache.nvp[0].equals;
        // FIXME the variable name could probably have been split over multiple
        // lines; it's also possible that multiple assignments/things were all
        // in a single line. all of that won't work right now
        ptr = s->line[eq->lnum];
        ptr[eq->pos] = 0;
        if ((ptr2 = strrchr(ptr, ';'))) {
            ptr2++;
        } else if ((ptr2 = strchr(ptr, ':'))) {
            ptr2++;
        } else {
            ptr2 = ptr;
        }
        while (isspace(*ptr2))
            ptr2++;

        /* handle if/while/... */
        if ((ptr3 = strstr(ptr2, "if")) ||
            (ptr3 = strstr(ptr2, "while"))) {
            int level = 1;
            ptr3 = strchr(ptr3, '(');
            if (!ptr3) abort();
            for (ptr3 = ptr3 + 1; ptr3 < ptr + eq->pos; ptr3++) {
                if (*ptr3 == '(')
                    level++;
                else if (*ptr3 == ')')
                    level--;
                if (!level) {
                    break;
                }
            }
            ptr2 = ptr3 + 1;
            if (ptr2 >= ptr + eq->pos) abort();
        } else if ((ptr3 = strstr(ptr2, "else"))) {
            ptr2 = ptr3 + 5;
            if (ptr2 >= ptr + eq->pos) abort();
        }
        while (isspace(*ptr2))
            ptr2++;

        for (endpos = eq->pos - 1; isspace(ptr[endpos]); endpos--) ;
        varlen[0] = endpos - (ptr2 - ptr) + 1;
        memcpy(varname, ptr2, varlen[0]);
        varname[varlen[0]] = 0;
        varptr[0] = varname;
        for (n = 0; n < 4 && (varptr[n] = strtok(n == 0 ? varptr[n - 0] : NULL, "=")) != NULL; n++)
            varlen[n] = strlen(varptr[n]);
        if (n >= 4) {
            fprintf(stderr, "Too many assignments\n");
            exit(1);
        }
        strcpy(lineend, s->line[s->named_initializer_cache.cbracket.lnum] +
                            s->named_initializer_cache.cbracket.pos + 1);
        *ptr2++ = '{';
        for (n = 0; n < n_cnt; n++) {
            char *val = s->named_initializer_cache.nvp[n].contents.val;
            const struct s_contents *content = &str->contents[n];
            char *tag;
            int taglen, vallen;

            if (s->named_initializer_cache.nvp[n].contents.tag[0]) {
                int m;
                for (m = 0; m < str->n_contents; m++) {
                    if (!strcmp(s->named_initializer_cache.nvp[n].contents.tag,
                                str->contents[m].tag)) {
                        break;
                    }
                }
                if (m == str->n_contents) {
                    fprintf(stderr, "Could not find struct entry %s in %s\n",
                            s->named_initializer_cache.nvp[n].contents.tag,
                            str->name);
                    exit(1);
                }
                content = &str->contents[m];
            }

            tag = content->tag;
            taglen = strlen(tag);
            vallen = strlen(val);

            for (m = 0; varptr[m]; m++) {
                if (varname[0] == '*') {
                    if (varptr[m][1] == '*' || varptr[m][1] == '(')
                        *ptr2++ = '(';
                    memcpy(ptr2, varptr[m] + 1, varlen[m] - 1);
                    ptr2 += varlen[m] - 1;
                    if (varptr[m][1] == '*' || varptr[m][1] == '(')
                       *ptr2++ = ')';
                    *ptr2++ = '-';
                    *ptr2++ = '>';
                } else {
                    if (varname[0] == '(')
                        *ptr2++ = '(';
                    memcpy(ptr2, varptr[m], varlen[m]);
                    ptr2 += varlen[m];
                    if (varptr[m][0] == '(')
                        *ptr2++ = ')';
                    *ptr2++ = '.';
                }
                memcpy(ptr2, tag, taglen);
                ptr2 += taglen;
                *ptr2++ = ' ';
                *ptr2++ = '=';
                *ptr2++ = ' ';
            }
            memcpy(ptr2, val, vallen);
            ptr2 += vallen;
            *ptr2++ = ';';
        }
        *ptr2++ = '}';
        // Con't copy a trailing ';' which
        // messes up if () { .. } else .. statements
        ptr3 = lineend;
        while (ptr3[0] == ' ' || ptr3[0] == ';') ptr3++;
        strcpy(ptr2, ptr3);
        ptr2 += strlen(ptr3);
        *ptr2++ = 0;

        // zero remaining lines
        for (n = eq->lnum + 1; n <= s->named_initializer_cache.cbracket.lnum; n++)
            s->line[n][0] = 0;

    // or return (AVRational) { a + 1, b - 1 };
    } else if (s->parent->parent->parent &&
               s->parent->parent->parent->named_initializer_cache.ret.lnum != -1) {
        char lineend[LINELEN], tmpname[LINELEN], val[LINELEN];
        struct lnum_pos_pair cb, *ob, cp, ls;
        int len;

        sprintf(tmpname, "tmp__%d", cntr++);
        ob = &s->named_initializer_cache.obracket;
        cb =  s->named_initializer_cache.cbracket;

        cb.pos++;
        backup_multi_line(s, val, sizeof(val), ob, &cb);
        cp = cb;
        if (s->line[cb.lnum][cb.pos] != ';') abort();
        cb.pos++;
        cp.pos = strlen(s->line[cp.lnum]);
        backup_multi_line(s, lineend, sizeof(lineend), &cb, &cp);
        ls = s->parent->parent->parent->named_initializer_cache.ret;

        copy_multi_line(s, &ls, "{ ");
        copy_multi_line(s, &ls, cast_name);
        copy_multi_line(s, &ls, " ");
        copy_multi_line(s, &ls, tmpname);
        copy_multi_line(s, &ls, " = ");
        copy_multi_line(s, &ls, val);
        copy_multi_line(s, &ls, "; return ");
        copy_multi_line(s, &ls, tmpname);
        copy_multi_line(s, &ls, "; }");
        copy_multi_line(s, &ls, lineend);

    } else {
        struct state *p;
        for (p = s->parent->parent; p; p = p->parent) {
            char x = p->parent->named_initializer_cache.bracket_type;
            if (x != '(')
                break;
        }
        if (!p || !p->parent) {
            fprintf(stderr, "Lost track of parsing state - exiting\n");
            exit(1);
        }

        // return function((int[2]) { a + 1, b - 1 });
        if (p->parent->named_initializer_cache.ret.lnum != -1) {
            char lineend[LINELEN], linestart[LINELEN], tmpname[LINELEN], *ptr, val[TVALLEN];
            char bak[LINELEN];
            struct lnum_pos_pair cb, *ob, cp, ls;
            int len, padding = strlen(s->line[s->lnum]);
            
            sprintf(tmpname, "tmp__%d", cntr++);
            ob = &s->named_initializer_cache.obracket;
            cb = s->named_initializer_cache.cbracket;

            if (p->parent->named_initializer_cache.ret.lnum !=
                    s->named_initializer_cache.cbracket.lnum) {
                fprintf(stderr,
                        "Return compound initializer spread over multiple lines not supported\n");
                exit(1);
            }
            cb.pos++;
            backup_multi_line(s, val, sizeof(val), ob, &cb);
            ob = &s->parent->parent->named_initializer_cache.obracket;
            backup_multi_line(s, linestart, sizeof(linestart),
                              &p->parent->named_initializer_cache.ret, ob);
            cp = cb;
            cp.pos = strlen(s->line[cp.lnum]);
            backup_multi_line(s, lineend, sizeof(lineend), &cb, &cp);

            ls = p->parent->named_initializer_cache.ret;
            copy_multi_line(s, &ls, "{ ");
            copy_multi_line(s, &ls, cast_name);
            copy_multi_line(s, &ls, " ");
            copy_multi_line(s, &ls, tmpname);
            copy_multi_line(s, &ls, " = ");
            copy_multi_line(s, &ls, val);
            copy_multi_line(s, &ls, "; ");
            copy_multi_line(s, &ls, linestart);
            copy_multi_line(s, &ls, tmpname);
            copy_multi_line(s, &ls, lineend);
            p->parent->named_initializer_cache.add_closing_bracket++;
            s->cur += strlen(s->line[s->lnum]) - padding;

        // x = function((int[2]) { a + 1, b - 1 });
        // this also works for bracketed statements, e.g.
        // x = ((int[4]){0,1,2,3})[idx];
        // this is changed to:
        // { int tmp__X[2] = { a + 1, b - 1 }; x = function(tmp__X); }
        } else if (p->parent->named_initializer_cache.nvp[0].equals.lnum != -1) {
                       char lineend[LINELEN], linestart[LINELEN], tmpname[LINELEN], *ptr, val[LINELEN*10], *ptr2, *ptr3;
            char bak[LINELEN], var[LINELEN];
            struct lnum_pos_pair cb, *ob, cp, ls;
            int len, pos, padding;
            char *array_start;

            DEBUG fprintf(stderr, "Assign-function-based compound literal replacement\n");
            sprintf(tmpname, "tmp__%d", cntr++);
            ob = &s->named_initializer_cache.obracket;
            cb =  s->named_initializer_cache.cbracket;
            cb.pos++;

            backup_multi_line(s, val, sizeof(val), ob, &cb);
            ptr = s->line[cb.lnum] + cb.pos;
            cp = cb;
            while (1) {
                char *colptr = strchr(ptr, ';');
                if (!colptr) {
                    cp.lnum++;
                    if (cp.lnum >= s->n_lines) {
                        fprintf(stderr, "Couldn't find function close\n");
                        exit(1);
                    }
                    cp.pos = 0;
                    ptr = s->line[cp.lnum] + cp.pos;
                } else {
                    ptr = colptr;
                    break;
                }
            }
            // We want to backup the full next line, including garbage, so we can re-set
            // parsing state afterwards (and when having recursed back into the parent
            // parser, we'll add the appropriate brackets). Thus, backup full line, not
            // just the part relevant for our own context.
            cp.pos = strlen(s->line[cp.lnum]);
            padding = strlen(s->line[s->lnum]);
            backup_multi_line(s, lineend, sizeof(lineend), &cb, &cp);
            cp.pos = (int) (ptr - s->line[cp.lnum]);

            // Backup line start, and find the entry point where to insert the new-context
            // bracket so we can declare a variable:
            // { var tmp = { val }; xyz = function(tmp); }
            // instead of
            // xyz = function((var){val});
            // in the above, find the position of 'xyz'
            ls = p->parent->named_initializer_cache.nvp[0].equals;
            ls.pos--;
            ptr = s->line[ls.lnum] + ls.pos;
            // 'ls' is now the position of 'z' in the expression above
#define ismathsymbol(c) \
    c == '+' || c == '-' || c == '<' || c == '>' || c == '*' || c == '/' || c == '=' || c == '&' || c == '|' || c == '^'
            while (ls.pos > 0 || ls.lnum > 0) {
                int prev_strlen = ls.lnum > 0 ? strlen(s->line[ls.lnum]) - 1 : 0;
                char prev = ls.pos == 0 ? (ls.lnum > 0 ? s->line[ls.lnum - 1][prev_strlen] : 0) : s->line[ls.lnum][ls.pos - 1];
                if (ls.pos > 0 &&
                   ((prev >= 'a' && prev <= 'z') ||
                    (prev >= 'A' && prev <= 'Z') ||
                    (prev >= '0' && prev <= '9') ||
                    isspace(prev) || ismathsymbol(prev) || prev == '_' ||prev == '.' || prev == '[' || prev == ']')) {
                    ptr--;
                    ls.pos--;
                    if (ls.pos < 0) {
                        ls.pos = prev_strlen;
                        ls.lnum--;
                        ptr = &s->line[ls.lnum][ls.pos];
                    }
                } else {
                    break;
                }
            }
            while (isspace(*ptr)) {
                ptr++;
                if (!*ptr) {
                    ls.lnum++;
                    ls.pos = 0;
                    ptr = s->line[ls.lnum];
                } else {
                    ls.pos++;
                }
            }
            // 'ls' is now the position of 'x' in the expression above
            backup_multi_line(s, linestart, sizeof(linestart), &ls,
                              &s->parent->parent->named_initializer_cache.obracket);

            ptr3 = strchr(linestart, '=');
            while (ptr3 > linestart && (isspace(ptr3[-1]) || ismathsymbol(ptr3[-1]))) ptr3--;
            if ((ptr2 = strchr(linestart, ' ')) < ptr3) {
                /* if it's in the form 'type var = function(..)', make sure to declare
                 * the variable 'type var' outside the scope of the brackets, i.e.:
                 * 'type var; { ...; var = function(..); }'. */
                char old = *ptr3;
                int len;
                *ptr3 = 0;
                len = strlen(linestart);
                memcpy(s->line[ls.lnum] + ls.pos, linestart, len);
                ls.pos += len;
                s->line[ls.lnum][ls.pos++] = ';';
                s->line[ls.lnum][ls.pos] = ';';
                *ptr3 = old;
                memmove(linestart, ptr2, strlen(ptr2) + 1);
            }
            copy_multi_line(s, &ls, "{ ");
            array_start = strchr(cast_name, '[');
            if (!array_start) {
                copy_multi_line(s, &ls, cast_name);
                copy_multi_line(s, &ls, " ");
                copy_multi_line(s, &ls, tmpname);
            } else {
                *array_start = 0;
                copy_multi_line(s, &ls, cast_name);
                copy_multi_line(s, &ls, " ");
                copy_multi_line(s, &ls, tmpname);
                *array_start = '[';
                copy_multi_line(s, &ls, array_start);
            }
            copy_multi_line(s, &ls, " = ");
            copy_multi_line(s, &ls, val);
            copy_multi_line(s, &ls, "; ");
            copy_multi_line(s, &ls, linestart);
            copy_multi_line(s, &ls, tmpname);
            copy_multi_line(s, &ls, lineend);

            // Now signal to the appropriate above parsing layer that we need to add
            // a closing bracket in the output stream after the ';'. We can't do that
            // here since it will mess up the parent parsing state.
            s->cur += strlen(s->line[s->lnum]) - padding;
            ptr3 = s->line[p->parent->named_initializer_cache.nvp[0].equals.lnum];
            ptr2 = strrchr(ptr3, '=');
	            p->parent->named_initializer_cache.nvp[0].equals.pos = (int) (ptr2 - ptr3);
            p->parent->named_initializer_cache.add_closing_bracket++;

            // and function((int[2]) { a + 2, b - 1 });
            // FIXME this triggers for complex if/while/else loops also, need to fix that
            // to execute on its own so we can better handle the bracket hell

        } else if (s->parent && s->parent->parent && s->parent->parent->parent &&
                   s->parent->parent->parent->named_initializer_cache.bracket_type == '(' &&
                   s->parent->parent->named_initializer_cache.bracket_type == '(' &&
                   s->parent->named_initializer_cache.bracket_type == '{' &&
                   s->named_initializer_cache.bracket_type == '{') {
            char lineend[LINELEN], linestart[LINELEN], tmpname[LINELEN], *ptr, val[LINELEN];
            char bak[LINELEN], var[LINELEN];
            struct lnum_pos_pair cb, *ob, cp, ls;
            int len, pos, padding;

            DEBUG fprintf(stderr, "Default function-bound compound replacement\n");
            sprintf(tmpname, "tmp__%d", cntr++);
            ob = &s->named_initializer_cache.obracket;
            cb =  s->named_initializer_cache.cbracket;
            cb.pos++;

            backup_multi_line(s, val, sizeof(val), ob, &cb);
            ptr = s->line[cb.lnum] + cb.pos;
            cp = cb;
            while (1) {
                char *colptr = strchr(ptr, ';');
                if (!colptr) {
                    cp.lnum++;
                    if (cp.lnum >= s->n_lines) {
                        fprintf(stderr, "Couldn't find function close\n");
                        exit(1);
                    }
                    cp.pos = 0;
                    ptr = s->line[cp.lnum] + cp.pos;
                } else {
                    ptr = colptr;
                    break;
                }
            }
            // We want to backup the full next line, including garbage, so we can re-set
            // parsing state afterwards (and when having recursed back into the parent
            // parser, we'll add the appropriate brackets). Thus, backup full line, not
            // just the part relevant for our own context.
            cp.pos = strlen(s->line[cp.lnum]);
            padding = strlen(s->line[s->lnum]);
            backup_multi_line(s, lineend, sizeof(lineend), &cb, &cp);
            cp.pos = (int) (ptr - s->line[cp.lnum]);

            // Backup line start, and find the entry point where to insert the new-context
            // bracket so we can declare a variable:
            // { var tmp = { val }; function(tmp); }
            // instead of
            // function((var){val});
            // in the above, find the position of 'fun...'
            ls = p->named_initializer_cache.obracket;
            ls.pos--;
            ptr = s->line[ls.lnum] + ls.pos;
            while (isspace(*ptr)) {
                if (ls.pos == 0) {
                    ls.lnum--;
                    if (ls.lnum < 0) {
                        fprintf(stderr, "Couldn't find start of assign expression\n");
                        exit(1);
                    }
                    ls.pos = strlen(s->line[ls.lnum]) - 1;
                    ptr = s->line[ls.lnum] + ls.pos;
                } else {
                    ls.pos--;
                    ptr--;
                }
            }
            // 'ls' is now the position of 'n' in the expression above
            while (1) {
                if  (ls.pos > 1 && ptr[-1] == '>' && ptr[-2] == '-') {
                    ptr -= 2;
                    ls.pos -= 2;
                } else if (ls.pos > 0 &&
                   ((ptr[-1] >= 'a' && ptr[-1] <= 'z') ||
                    (ptr[-1] >= 'A' && ptr[-1] <= 'Z') ||
                    (ptr[-1] >= '0' && ptr[-1] <= '9') ||
                    ptr[-1] == '_' || ptr[-1] == '.' || ptr[-1] == '[' || ptr[-1] == ']' ||
                    ptr[-1] == '(' || ptr[-1] == ')' || ismathsymbol(ptr[-1])) || isspace(ptr[-1])) {
                    ptr--;
                    ls.pos--;
                } else {
                    break;
                }
            }
            while (isspace(*ptr)) {
                ptr++;
                ls.pos++;
                if (ls.pos == strlen(s->line[ls.lnum])) {
                    ls.lnum++;
                    ls.pos = 0;
                    ptr = s->line[ls.lnum];
                }
            }
            // 'ls' is now the position of 'f' in the expression above
            backup_multi_line(s, linestart, sizeof(linestart), &ls,
                              &s->parent->parent->named_initializer_cache.obracket);

            copy_multi_line(s, &ls, "{ ");
            if ((ptr =  strchr(cast_name, '['))) {
                *ptr = 0;
                copy_multi_line(s, &ls, cast_name);
                *ptr = '[';
                copy_multi_line(s, &ls, " ");
                copy_multi_line(s, &ls, tmpname);
                copy_multi_line(s, &ls, ptr);
            } else {
                copy_multi_line(s, &ls, cast_name);
                copy_multi_line(s, &ls, " ");
                copy_multi_line(s, &ls, tmpname);
            }
            copy_multi_line(s, &ls, " = ");
            copy_multi_line(s, &ls, val);
            copy_multi_line(s, &ls, "; ");
            copy_multi_line(s, &ls, linestart);
            copy_multi_line(s, &ls, tmpname);
            copy_multi_line(s, &ls, lineend);

            // Now signal to the appropriate above parsing layer that we need to add
            // a closing bracket in the output stream after the ';'. We can't do that
            // here since it will mess up the parent parsing state.
            s->cur += strlen(s->line[s->lnum]) - padding;
            p->parent->named_initializer_cache.add_closing_bracket++;
#if 0
            fprintf(stderr, "function((cast){vals}) not yet supported\n");
            exit(1);
#endif
#if 0
            if (0)
    fprintf(stderr, "FIXME %s (%d/%d - %d/%d = %d/%d) %d\n",
            s->parent->parent->named_initializer_cache.cast_name,
            s->parent->parent->named_initializer_cache.obracket.lnum,
            s->parent->parent->named_initializer_cache.obracket.pos,
            s->parent->parent->named_initializer_cache.cbracket.lnum,
            s->parent->parent->named_initializer_cache.cbracket.pos,
            s->parent->parent->parent->named_initializer_cache.nvp[0].equals.lnum,
            s->parent->parent->parent->named_initializer_cache.nvp[0].equals.pos,
            s->named_initializer_cache.n_contents);
            exit(1);
#endif
        } else {
            /* do nothing */
            fprintf(stderr, "do nothing\n");
        }
    }
}

static int find_sub_struct(struct state *s, const struct s_layout *parent, const char *name)
{
    int n;

    for (n = 0; n < parent->n_contents; n++) {
        if (!strcmp(parent->contents[n].tag, name)) {
            if (parent->contents[n].struct_type_name) {
                DEBUG fprintf(stderr, "trying to find struct %s\n",
                              parent->contents[n].struct_type_name);
                s->struct_ctx = find_struct(parent->contents[n].struct_type_name);
            } else {
                char buf[LINELEN];
                sprintf(buf, "%s:%s", parent->name, name);
                DEBUG fprintf(stderr, "trying to find struct %s in %s\n",
                              name, parent->name);
                s->struct_ctx = find_struct(buf);
            }
            return s->struct_ctx != NULL;
        }
    }

    DEBUG fprintf(stderr, "Couldn't find %s in %s, setting to parent\n",
                  name, parent->name);

    return 0;
}

static void parse_statement_nobrackets(struct state *s, char *st, struct lnum_pos_pair *p)
{
    int off = strlen(st);

    /* check that there is a space, newline, ';' or so before the statement,
     * and a { or space directly following it. */
    if ((s->line[s->lnum] == s->cur || isspace(s->cur[-1]) || s->cur[-1] == ';' || s->cur[-1] == '}') &&
        (s->cur[off] == '{' || isspace(s->cur[off]))) {
        p->lnum = s->lnum;
        p->pos  = (int) (s->cur - s->line[s->lnum]);
    }
    s->cur += off;
}

static void parse_statement_brackets(struct state *s, char *st, struct lnum_pos_pair *p)
{
    int off = strlen(st);

    /* check that there is a space, newline, ';' or so before the statement,
     * and a ( or space directly following it. */
    if ((s->line[s->lnum] == s->cur || isspace(s->cur[-1]) || s->cur[-1] == ';' || s->cur[-1] == '}') &&
        (s->cur[off] == '(' || isspace(s->cur[off]))) {
        p->lnum = s->lnum;
        p->pos  = (int) (s->cur - s->line[s->lnum]);
    }
    s->cur += off;
}

static int is_null_value(char *ptr, int pos, int to)
{
    while (to > pos && (isspace(ptr[to - 1]) || ptr[to - 1] == '}')) to--;
    while (pos < to && (isspace(ptr[pos]) || ptr[pos] == '{')) pos++;
    if (to-pos == 1 && !memcmp(ptr+pos, "0", 1)) return 1;
    if (to-pos == 4 && !memcmp(ptr+pos, "NULL", 4)) return 1;
    return 0;
}

static void reset_parsing_context(struct state *s)
{
    int l;

    s->named_initializer_cache.ret.lnum = -1;
    s->named_initializer_cache.if_st.lnum = -1;
    s->named_initializer_cache.while_st.lnum = -1;
    s->named_initializer_cache.for_st.lnum = -1;
    s->named_initializer_cache.do_st.lnum = -1;
    s->named_initializer_cache.else_st.lnum = -1;
    s->named_initializer_cache.obracket.lnum = -1;
    s->named_initializer_cache.qmark.lnum = -1;;
    s->named_initializer_cache.colon.lnum = -1;
    s->named_initializer_cache.cbracket.lnum = -1;
    s->named_initializer_cache.struct_name[0] = 0;
    s->named_initializer_cache.n_contents = 0;
    for (l = 0; l < S_ENTRIES; l++) {
        s->named_initializer_cache.nvp[l].equals.lnum =
        s->named_initializer_cache.nvp[l].comma.lnum =
        s->named_initializer_cache.nvp[l].aidxcbracket.lnum =
        s->named_initializer_cache.nvp[l].aidxobracket.lnum =
        s->named_initializer_cache.nvp[l].dot.lnum = -1;
    }
    s->named_initializer_cache.array_index[0] = 0;
    s->named_initializer_cache.cast_name[0] = 0;
    s->named_initializer_cache.tag[0] = 0;
    s->named_initializer_cache.is_struct     = 0;
    s->named_initializer_cache.is_array      = 0;
    s->named_initializer_cache.start_index   = 0;
    s->named_initializer_cache.add_closing_bracket = 0;
    s->named_initializer_cache.bracket_type = 0;
}

#define pos_is_after(a, b) \
    (a.lnum > b.lnum || \
     (a.lnum == b.lnum && a.pos > b.pos))

/*
 * Handle one level of recursion.
 *
 * Returns pointer to closer ('//', '/"', '/)' or abort).
 */
static int handle_open(struct state *s);
static int handle_open_in(struct state *s)
{
    int n;

    if (!s->cur) {
        DEBUG fprintf(stderr, "Entered EOL at line=%d\n", s->lnum);
        s->lnum++;
        if (s->lnum < s->n_lines)
            s->cur = s->line[s->lnum];
        return 0;
    } else if  (s->cur[0] == '/' && s->cur[1] == '/') {
        DEBUG fprintf(stderr, "Entered // comment at line=%d,pos=%d\n",
                      s->lnum, (int) (s->cur - s->line[s->lnum]));
        if (++s->lnum > s->n_lines) {
            fprintf(stderr,
                    "Line overflow at comment starting at line=%d,pos=%d, increase N_LINES\n",
                    s->lnum, (int) (s->cur - s->line[s->lnum]));
            exit(1);
        }
        s->cur = s->line[s->lnum];
        return 0;
    } else if (s->cur[0] == '\"' || s->cur[0] == '\'') {
        // find closing double quote mark
        static const char *sep2[] = { "\"", "\\\"", "\\\'", "\\\\", NULL };
        static const char *sep3[] = { "\'", "\\\"", "\\\'", "\\\\", NULL };
        char *quote = s->cur;
        int qline = s->lnum;

        DEBUG fprintf(stderr, "Entered quote type=%c @ line=%d,pos=%d\n",
                      *quote, s->lnum, (int) (s->cur - s->line[s->lnum]));
        s->cur++;
        for (;;) {
            s->cur = strsbrk(s->cur, *quote == '\"' ? sep2 : sep3);
            if (!s->cur) {
                fprintf(stderr,
                        "Can't find closing quote in line=%d,pos=%d (%s)\n",
                        qline, (int) (quote - s->line[qline]), s->line[s->lnum]);
                exit(1);
            } else if (s->cur[0] == '\\' && (s->cur[1] == '\"' ||
                                             s->cur[1] == '\'' ||
                                             s->cur[1] == '\\')) {
                DEBUG fprintf(stderr, "Commented quote type=%c at line=%d,pos=%d\n",
                              s->cur[1], s->lnum, (int) (s->cur - s->line[s->lnum]));
                s->cur += 2;
                continue;
            } else if (s->cur[0] == '\'' || s->cur[0] == '\"') {
                DEBUG fprintf(stderr, "Closing quote type=%c at line=%d,pos=%d\n",
                              s->cur[0], s->lnum, (int) (s->cur - s->line[s->lnum]));
                s->cur++;
                break;
            } else {
                abort();
            }
        }
        return 0;
    } else if (s->cur[0] == '/' && s->cur[1] == '*') {
        // find closing comment '*/'
        char *comment = s->cur;
        int cline = s->lnum;

        DEBUG fprintf(stderr, "Entering /* comment at line=%d,pos=%d\n",
                      s->lnum, (int) (s->cur - s->line[s->lnum]));
        s->cur += 2;
        while (!(s->cur = strstr(s->cur, "*/"))) {
            if (++s->lnum > s->n_lines) {
                fprintf(stderr,
                        "Too big comment starting at line=%d,pos=%d, increase N_LINES\n",
                        cline, (int) (comment - s->line[cline]));
                exit(1);
            }
            s->cur = s->line[s->lnum];
        }
        DEBUG fprintf(stderr, "Found closing */ comment at line=%d,pos=%d\n",
                      s->lnum, (int) (s->cur - s->line[s->lnum]));
        s->cur += 2; // skip '*/'
        return 0;
    } else if (s->cur[0] == '(' || s->cur[0] == '{' || s->cur[0] == '[') {
        // find closing bracket and do magic here
        static const char *sep2[] = { ")", "\"", "//", "/*", "(", "{", "[", "\'", NULL };
        static const char *sep3[] = { "}", "\"", "//", "/*", "(", "{", "[", "\'", ".", "=", ":", "?",
                                      ",", "static ", "struct ", "return ", "MOVAtom ", "AVPacket ",
                                      "Syncpoint ", ",", ";", "for", "while", "do", "else", "if", NULL };
        static const char *sep4[] = { "]", "\"", "//", "/*", "(", "{", "[", "\'", NULL };
        char bracket = *s->cur;
        int bnum = s->lnum, bpos = (int) (s->cur - s->line[bnum]);
        struct state *s3 = malloc(sizeof(struct state));
        int idx = s->named_initializer_cache.n_contents;
        if (idx >= S_ENTRIES) {
            fprintf(stderr, "Array doesn't fit, increase S_ENTRIES\n");
            exit(1);
        }

        if (!s3) {
            fprintf(stderr, "Ran out of memory\n");
            exit(1);
        }
        DEBUG fprintf(stderr, "Found opening bracket %c at line=%d,pos=%d\n",
                      s->cur[0], s->lnum, (int) (s->cur - s->line[s->lnum]));
        s->cur++; // skip '('
        if (s->named_initializer_cache.struct_name[0]) {
            s3->struct_ctx = find_struct(s->named_initializer_cache.struct_name);
            DEBUG fprintf(stderr, "struct_ctx=%p (%s)\n",
                          s3->struct_ctx,
                          s->named_initializer_cache.struct_name);
        } else if (s->struct_ctx &&
                   s->named_initializer_cache.tag[0] &&
                   find_sub_struct(s3, s->struct_ctx,
                             s->named_initializer_cache.tag)) {
            DEBUG fprintf(stderr, "derived struct_ctx=%p (%s:%s)\n",
                          s3->struct_ctx, s->struct_ctx->name,
                          s->named_initializer_cache.tag);
        } else if (s->struct_ctx && (!s->parent || !s->parent->named_initializer_cache.is_array) &&
                   s->named_initializer_cache.start_index < s->struct_ctx->n_contents &&
                   s->struct_ctx->contents[s->named_initializer_cache.start_index].struct_type_name &&
                   find_sub_struct(s3, s->struct_ctx, s->struct_ctx->contents[s->named_initializer_cache.start_index].tag)) {
            DEBUG fprintf(stderr, "derived struct_ctx=%p (%s:%d)\n",
                          s3->struct_ctx, s->struct_ctx->name,
                          s->named_initializer_cache.start_index);
        } else {
            // array?
            s3->struct_ctx = s->struct_ctx;
            DEBUG fprintf(stderr, "copying struct_ctx=%p\n",
                          s3->struct_ctx);
        }
        reset_parsing_context(s3);
        s3->parent  = s;
        s3->line    = s->line;
        s3->n_lines = s->n_lines;
        s3->cur     = s->cur;
        s3->lnum    = s->lnum;
        s3->depth   = s->depth;
        s3->named_initializer_cache.obracket.lnum = bnum;
        s3->named_initializer_cache.obracket.pos  = (int) (s->cur - 1 - s->line[bnum]);        
        s3->named_initializer_cache.bracket_type = bracket;

        for (;;) {
            int close;
            s3->cur = strsbrk(s3->cur, bracket == '(' ? sep2 : (bracket == '[' ? sep4 : sep3));
            close = handle_open(s3);
            if (!s3->cur) {
                fprintf(stderr, "Did not find closing bracket for opener %c\n", bracket);
                exit(1);
            } else if (close) {
                if ((bracket == '{' && close == '}') ||
                    (bracket == '(' && close == ')') ||
                    (bracket == '[' && close == ']')) {
                    // we found a closer. now (if '()'), look for
                    // potentially enclosed '{}', or else look for
                    // ';' at the end. If none, return current state.
                    // this allows grouping of stuff like:
                    // 1) (cast_type) { array }
                    // 2) if (condition) { code }
                    // 3) void function(int argument) { body }
                    if (bracket == '(') {
                        struct state *s2 = malloc(sizeof(struct state));

                        if (!s2) {
                            fprintf(stderr, "Ran out of memory\n");
                            exit(1);
                        }
                        reset_parsing_context(s2);
                        s2->parent  = s3;
                        s2->struct_ctx = s3->struct_ctx;
                        s2->line    = s3->line;
                        s2->n_lines = s3->n_lines;
                        s2->cur     = s3->cur;
                        s2->lnum    = s3->lnum;
                        s2->depth   = s3->depth;

                        DEBUG fprintf(stderr, "Found ')' s=%s/s3=%s, looking for '{'\n",
                                      s->named_initializer_cache.cast_name,
                                      s3->named_initializer_cache.cast_name);
                        s3->cur = s2->cur;
                        for (;;) {
                            int len = strspn(s2->cur, "\r\n\t ");

                            DEBUG fprintf(stderr, "Skipping %d chars of whitespace at line %d,%d\n",
                                          len, s2->lnum, (int) (s2->cur - s->line[s2->lnum]));
                            s2->cur += len;
                            if (!s2->cur[0]) {
                                if (s2->lnum < s2->n_lines) {
                                    s2->lnum++;
                                    s2->cur = s2->line[s2->lnum];
                                } else {
                                    s2->cur = NULL;
                                    return 0;
                                }
                            } else if ((s2->cur[0] == '/' && s2->cur[1] == '/') ||
                                       (s2->cur[0] == '/' && s2->cur[1] == '*')) {
                                DEBUG fprintf(stderr,
                                              "Skipping %s at line=%d,pos=%d\n",
                                              s2->cur[0] ? "comment" : "empty line",
                                              s2->lnum, (int) (s2->cur - s2->line[s2->lnum]));
                                close = handle_open(s2);
                                if (close) {
                                    fprintf(stderr, "Unexpected close state - aborting\n");
                                    exit(1);
                                }
                            } else if (s2->cur[0] == '{') {
                                DEBUG fprintf(stderr, "Found {, looking for }\n");
                                s2->named_initializer_cache.obracket.lnum = s2->lnum;
                                s2->named_initializer_cache.obracket.pos  = (int) (s2->cur - s2->line[s2->lnum]);
                                s2->named_initializer_cache.bracket_type = '{';
                                close = handle_open(s2);
                                if (close) {
                                    free(s2);
                                    goto err;
                                } else {
                                    s3->cur  = s2->cur;
                                    s3->lnum = s2->lnum;
                                    break;
                                }
                            } else if (s2->cur[0] == '[') {
                                DEBUG fprintf(stderr, "Found [, looking for ]\n");
                                s2->named_initializer_cache.obracket.lnum = s2->lnum;
                                s2->named_initializer_cache.obracket.pos  = (int) (s2->cur - s2->line[s2->lnum]);
                                s2->named_initializer_cache.bracket_type = '[';
                                close = handle_open(s2);
                                if (close) {
                                    free(s2);
                                    goto err;
                                } else {
                                    s3->cur  = s2->cur;
                                    s3->lnum = s2->lnum;
                                    break;
                                }
                            } else {
                                DEBUG fprintf(stderr, "Unknown char %c, resetting... (parent bc=%d)\n",
                                              s2->cur[0], s->named_initializer_cache.add_closing_bracket);
                                break;
                            }
                        }
                        free(s2);
                    } else if (bracket == '[') {
                        strcpy(s->named_initializer_cache.array_index,
                               s3->named_initializer_cache.array_index);
                        s->named_initializer_cache.nvp[idx].aidxcbracket =
                                s3->named_initializer_cache.cbracket;
                        s->named_initializer_cache.nvp[idx].aidxobracket =
                                s3->named_initializer_cache.obracket;
                        s->named_initializer_cache.is_array = 1;
                    }

                    s->cur  = s3->cur;
                    s->lnum = s3->lnum;
                    if (s->named_initializer_cache.add_closing_bracket) { // FIXME; while?
                        char lineend[LINELEN], *ptr = s->cur;
                        int l;
                        if (*ptr != ';') {
                            l = s->lnum;
                            DEBUG fprintf(stderr, "Weird char %c in closing bracket state, search for next ';'\n", s->cur[0]);
                            ptr = s->cur;
                            do {
                                ptr = strchr(ptr, ';');
                                if (!ptr) {
                                    fprintf(stderr, "Could not find ; in '%s'\n", s->line[l]);
                                    l++;
                                    if (l < s->n_lines)
                                        ptr = s->line[l];
                                } else {
                                    break;
                                }
                            } while (l < s->n_lines);
                            if (!ptr) {
                                fprintf(stderr, "Couldn't find - exiting\n");
                                exit(1);
                            }
                        } else {
                            DEBUG fprintf(stderr, "Adding closing bracket\n");
                        }
                        ptr++;
                        strcpy(lineend, ptr);
                        *ptr++ = ' ';
                        while (s->named_initializer_cache.add_closing_bracket--)
                            *ptr++ = '}';
                        strcpy(ptr, lineend);
                        ptr += strlen(lineend);
                        if (*s->cur == ';')
                            s->cur = ptr;
                        s->named_initializer_cache.add_closing_bracket = 0;

                        /* since we're essentially replacing a ';' by a '}' here, we
                         * will miss a trailing ';' to reset context. So manually reset
                         * parsing context here. */
                        reset_parsing_context(s);
                    }

                    free(s3);

                    return 0;
                } else {
                err:
                    fprintf(stderr,
                            "Unexpected bracket close %c at line=%d,pos=%d for open %c at line=%d,pos=%d\n",
                            close, s->lnum, (int) (s->cur - s->line[s->lnum]),
                            bracket, bnum, bpos);
                    free(s3);
                    exit(1);
                }
            }
        }
    } else if (s->cur[0] == ')' || s->cur[0] == '}' || s->cur[0] == ']') {
        char res = *s->cur;
        DEBUG fprintf(stderr, "Found closing bracket %c at line=%d,pos=%d, contents=%d (s=%d/a=%d)\n",
                      s->cur[0], s->lnum, (int) (s->cur - s->line[s->lnum]),
                      s->named_initializer_cache.n_contents,
                      s->named_initializer_cache.is_struct,
                      s->named_initializer_cache.is_array);
        s->named_initializer_cache.cbracket.lnum = s->lnum;
        s->named_initializer_cache.cbracket.pos  = (int) (s->cur - s->line[s->lnum]);
        if (s->cur[0] == '}') {
            // handle missing comma
            s->cur[0] = ',';
            handle_open_in(s);
            s->cur--;
            s->cur[0] = '}';
            if (s->named_initializer_cache.n_contents > 0) {
                if (s->named_initializer_cache.is_struct) {
                    replace_named_initializer_struct(s);
                } else if (s->named_initializer_cache.n_contents > 1 &&
                           s->named_initializer_cache.is_array &&
                           (!s->parent->parent ||
                            !s->parent->parent->named_initializer_cache.cast_name[0])) {
                    replace_named_initializer_array(s);
                } else if ((s->named_initializer_cache.n_contents > 1 ||
                            is_recognized_struct(s->parent->parent->named_initializer_cache.cast_name) ||
                            (s->named_initializer_cache.cbracket.lnum ==
                                s->named_initializer_cache.obracket.lnum &&
                             is_null_value(s->line[s->named_initializer_cache.cbracket.lnum],
                                           s->named_initializer_cache.obracket.pos + 1,
                                           s->named_initializer_cache.cbracket.pos))) &&
                           s->parent && s->parent->parent &&
                           s->parent->parent->named_initializer_cache.cast_name[0]) {
                    replace_compound_initializer(s);
                }
            }
        } else if (s->cur[0] == ']' && /* FIXME reset if lnum != s->lnum? */
                   s->named_initializer_cache.obracket.lnum == s->lnum) {
            struct lnum_pos_pair *lpp = &s->named_initializer_cache.obracket,
                                 *lpn = &s->named_initializer_cache.cbracket;

            if (s->named_initializer_cache.obracket.pos == 0 ||
#define c(p) s->line[p.lnum][p.pos-1]
                isspace(c(s->named_initializer_cache.obracket)) ||
                c(s->named_initializer_cache.obracket) == ',') {
#undef c
                memcpy(s->named_initializer_cache.array_index,
                       s->line[lpp->lnum] + lpp->pos + 1,
                       lpn->pos - 1 - lpp->pos);
                s->named_initializer_cache.array_index[lpn->pos - 1 - lpp->pos] = 0;
                DEBUG fprintf(stderr, "Array index entry name '%s'\n",
                              s->named_initializer_cache.array_index);
            }
        } else if (s->cur[0] == ')' && /* FIXME reset if lnum != s->lnum? */
                   s->named_initializer_cache.obracket.lnum == s->lnum) {
            struct lnum_pos_pair *lpp = &s->named_initializer_cache.obracket,
                                 *lpn = &s->named_initializer_cache.cbracket;

            memcpy(s->named_initializer_cache.cast_name,
                   s->line[lpp->lnum] + lpp->pos + 1,
                   lpn->pos - 1 - lpp->pos);
            s->named_initializer_cache.cast_name[lpn->pos - 1 - lpp->pos] = 0;
            DEBUG fprintf(stderr, "Cast name '%s'\n",
                          s->named_initializer_cache.cast_name);
        }
        s->cur++;
        return res;
    } else if (s->cur[0] == '.') {
        int idx = s->named_initializer_cache.n_contents;
        if (idx >= S_ENTRIES) abort();
        DEBUG fprintf(stderr, "Found . (named initializer?) at line=%d,pos=%d\n",
                      s->lnum, (int) (s->cur - s->line[s->lnum]));
        if (!(s->cur[1] >= '0' && s->cur[1] <= '9') &&
            (s->cur == s->line[s->lnum] || !((s->cur[-1] >= '0' && s->cur[-1] <= '9') ||
                                             (s->cur[-1] >= 'a' && s->cur[-1] <= 'z') ||
                                             (s->cur[-1] >= 'A' && s->cur[-1] <= 'Z')))) {
            s->named_initializer_cache.nvp[idx].dot.pos  = (int) (s->cur - s->line[s->lnum]);
            s->named_initializer_cache.nvp[idx].dot.lnum = s->lnum;
        }
        s->cur++;
        return 0;
    } else if (s->cur[0] == '?') {
        s->named_initializer_cache.qmark.pos  = (int) (s->cur - s->line[s->lnum]);
        s->named_initializer_cache.qmark.lnum = s->lnum;
        s->cur++;
        return 0;
    } else if (s->cur[0] == ':') {
        s->named_initializer_cache.colon.pos  = (int) (s->cur - s->line[s->lnum]);
        s->named_initializer_cache.colon.lnum = s->lnum;
        s->cur++;
        return 0;
    } else if (s->cur[0] == '=') {
        int idx = s->named_initializer_cache.n_contents;
        if (idx >= S_ENTRIES) abort();
        int pos = (int) (s->cur - s->line[s->lnum]);
        DEBUG fprintf(stderr, "Found = at line=%d,pos=%d\n",
                      s->lnum, pos + 1);
        if (s->named_initializer_cache.nvp[idx].dot.lnum == s->lnum) {
            int pos2 = s->named_initializer_cache.nvp[idx].dot.pos + 1;
            if (pos - pos2 + 1 >= LINELEN) abort();
            while (isspace(s->line[s->lnum][pos2]))
                pos2++;
            memcpy(s->named_initializer_cache.tag,
                   s->line[s->lnum] + pos2, pos - pos2);
            pos2 = pos - pos2;
            do {
                s->named_initializer_cache.tag[pos2] = 0;
            } while (--pos2 >= 0 && isspace(s->named_initializer_cache.tag[pos2]));
            DEBUG fprintf(stderr, "Is static initializer, tag=%s\n",
                          s->named_initializer_cache.tag);
        }
        s->named_initializer_cache.nvp[idx].equals.lnum = s->lnum;
        s->named_initializer_cache.nvp[idx].equals.pos  = (int) (s->cur - s->line[s->lnum]);
        s->cur++;
        return 0;
    } else if (s->cur[0] == ',') {
        int idx = s->named_initializer_cache.n_contents;
        if (idx >= S_ENTRIES) abort();
        DEBUG fprintf(stderr, "%s comma at line=%d,pos=%d, tag='%s', arr='%s', cst='%s/%s', idx=%d,el=%d,dl=%d\n",
                      s->named_initializer_cache.cbracket.lnum == -1 ? "Found" : "Emulated",
                      s->lnum, (int) (s->cur - s->line[s->lnum]),
                      s->named_initializer_cache.tag,
                      s->named_initializer_cache.array_index,
                      s->named_initializer_cache.cast_name,
                      s->parent && s->parent->parent ?
                        s->parent->parent->named_initializer_cache.cast_name : "(null)",
                      idx,
                      s->named_initializer_cache.nvp[idx].equals.lnum,
                      s->named_initializer_cache.nvp[idx].dot.lnum);
        s->named_initializer_cache.nvp[idx].comma.lnum = s->lnum;
        s->named_initializer_cache.nvp[idx].comma.pos  = (int) (s->cur - s->line[s->lnum]);
        if (s->named_initializer_cache.tag[0] && s->struct_ctx &&
            s->named_initializer_cache.nvp[idx].equals.lnum >= 0 &&
            s->named_initializer_cache.nvp[idx].dot.lnum >= 0) {
            add_tag_val_pair_struct(s);
        } else if (s->named_initializer_cache.array_index[0] && //s->struct_ctx &&
                   s->named_initializer_cache.nvp[idx].equals.lnum >= 0 &&
                   s->named_initializer_cache.nvp[idx].aidxcbracket.lnum >= 0 &&
                   pos_is_after(s->named_initializer_cache.nvp[idx].equals,
                                s->named_initializer_cache.nvp[idx].aidxcbracket)) {
            add_tag_val_pair_array(s);
        } else if (s->parent && s->parent->parent &&
                   s->parent->parent->named_initializer_cache.cast_name[0] &&
                   !s->named_initializer_cache.is_struct /*&&
                   !s->named_initializer_cache.is_array*/) {
            add_compound_literal_value(s);
        } else {
            // reset [=.,]
            s->named_initializer_cache.nvp[idx].comma.lnum =
            s->named_initializer_cache.nvp[idx].dot.lnum =
            s->named_initializer_cache.nvp[idx].aidxcbracket.lnum =
            s->named_initializer_cache.nvp[idx].aidxobracket.lnum =
            s->named_initializer_cache.nvp[idx].equals.lnum = -1;
            s->named_initializer_cache.array_index[0] = 0;
            s->named_initializer_cache.cast_name[0] = 0;
            if (s->named_initializer_cache.cbracket.lnum == -1) {
                s->named_initializer_cache.start_index++;
                DEBUG fprintf(stderr, "Adding one start index\n");
            }
        }
        s->cur++;
        return 0;
    } else if (s->cur[0] == ';') {
        reset_parsing_context(s);
        s->cur++;
        return 0;
    } else if (!strncmp(s->cur, "return ", 7)) {
        s->named_initializer_cache.ret.lnum = s->lnum;
        s->named_initializer_cache.ret.pos  = (int) (s->cur - s->line[s->lnum]);
        s->cur += 7;
        return 0;
    } else if (!strncmp(s->cur, "static ", 7) ||
               !strncmp(s->cur, "const ", 6) ||
               !strncmp(s->cur, "struct ", 7) ||
               !strncmp(s->cur, "AVClass ", 8) ||
               !strncmp(s->cur, "AVCodec ", 8) ||
               !strncmp(s->cur, "AVHWAccel ", 10) ||
               !strncmp(s->cur, "AVCodecParser ", 14) ||
               !strncmp(s->cur, "AVFilter ", 9) ||
               !strncmp(s->cur, "AVBitStreamFilter ", 18) ||
               !strncmp(s->cur, "AVInputFormat ", 14) ||
               !strncmp(s->cur, "AVOutputFormat ", 14) ||
               !strncmp(s->cur, "URLProtocol ", 12) ||
               !strncmp(s->cur, "MOVAtom ", 8) ||
               !strncmp(s->cur, "AVPacket ", 9) ||
               !strncmp(s->cur, "Syncpoint ", 10) ||
               !strncmp(s->cur, "RTPDynamicProtocolHandler ", 26)) {
        // look for 'static const struct x val = {' pattern
        char *eq, *sta, *str, *cnst, *spc;
        eq   = strchr(s->cur, '=');
        if (!eq) eq = strchr(s->cur, '{');
        if (eq) {
            sta  = strstr(s->cur, "static ");
            if (sta && sta < eq)
                s->cur = sta + 7;
            cnst = strstr(s->cur, "const ");
            if (cnst && cnst < eq)
                s->cur = cnst + 6;
            str  = strstr(s->cur, "struct ");
            if (str && str < eq)
                s->cur = str + 7;
            while (isspace(*s->cur))
                s->cur++;
            if (s->cur < eq) {
                spc = strchr(s->cur, ' '); // FIXME doesn't work for '\n'/'\t'/etc.
                if (!spc) spc = strchr(s->cur, '{');
                if (!spc) spc = s->cur + strlen(s->cur);
                memcpy(s->named_initializer_cache.struct_name, s->cur,
                    spc - s->cur);
                s->named_initializer_cache.struct_name[spc - s->cur] = 0;
                DEBUG fprintf(stderr, "Struct: %s\n", s->named_initializer_cache.struct_name);
                s->cur = spc;
            }
        } else {
            s->cur = strchr(s->cur, ' ');
        }
        return 0;
    } else if (!strncmp(s->cur, "if", 2)) {
        parse_statement_brackets(s, "if", &s->named_initializer_cache.if_st);
        return 0;
    } else if (!strncmp(s->cur, "else", 4)) {
        parse_statement_nobrackets(s, "else", &s->named_initializer_cache.else_st);
        return 0;
    } else if (!strncmp(s->cur, "while", 5)) {
        parse_statement_brackets(s, "while", &s->named_initializer_cache.while_st);
        return 0;
    } else if (!strncmp(s->cur, "for", 3)) {
        parse_statement_brackets(s, "for", &s->named_initializer_cache.for_st);
        return 0;
    } else if (!strncmp(s->cur, "do", 2)) {
        parse_statement_nobrackets(s, "do", &s->named_initializer_cache.do_st);
        return 0;
    } else {
        fprintf(stderr, "Unexpected opener at line=%d,pos=%d\n",
                s->lnum, (int) (s->cur - s->line[s->lnum]));
        exit(1);
    }

    abort();
}

static int handle_open(struct state *s)
{
    int res;

    s->depth++;
    if (s->depth > 100) {
        fprintf(stderr, "Probably a bug, depth=%d > 100, aborting\n",
                s->depth);
        exit(1);
    } else {
        DEBUG fprintf(stderr, "Scanning[%d]: %s", s->depth, !s->cur ? "\n" : s->cur);
    }

    res = handle_open_in(s);

    s->depth--;

    return res;
}

/*
 * Take a number (>= 1) of input lines. Consume at least 1 line.
 * Return number of lines consumed from input.
 *
 * NOTE: the number of lines written to stdout has no relation to
 * any of this.
 */
static int output(char **line, int n_lines)
{
    int n, res;
    char *last = NULL;
    struct state *s = malloc(sizeof(struct state));
    if (!s) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    memset(s, 0, sizeof(struct state));
    reset_parsing_context(s);
    s->line    = line;
    s->n_lines = n_lines;
    s->cur     = line[0];

    do {
        s->cur = strsbrk(s->cur, (const char *[]) { "(", "{", "\"", "//", "/*", "\'", "=",
                                                    "static ", "const ", "struct ", "return ",
                                                    "AVClass ", "AVCodec ", "AVCodecParser ", "AVFilter ",
                                                    "AVInputFormat ", "AVOutputFormat ", "AVHWAccel ",
                                                    "URLProtocol ", "RTPDynamicProtocolHandler ",
                                                    "AVBitStreamFilter ",
                                                    ",", ";", "[", NULL });
        if (s->cur)
            last = s->cur;
        /*char res =*/ handle_open(s);
    } while ((last && last[0] == '=') ||
             (s->cur && s->cur != line[s->lnum] /* FIXME partial line support */));

    DEBUG fprintf(stderr, "n_lines: %d\n", s->lnum);

    for (n = 0; n < s->lnum; n++) {
        fprintf(stdout, "%s", line[n]);
    }

    res = s->lnum;
    free(s);

    return res;
}

static void scan_input(char *s)
{
    char *ptr;
    if ((ptr = strstr(s, "AV_TIME_BASE_Q")) && !strstr(s, "#define")) {
        char *repl = "(AVRational){1, AV_TIME_BASE}";
        int orlen = strlen("AV_TIME_BASE_Q");
        int repllen = strlen(repl);
        memmove(ptr+repllen, ptr+orlen, strlen(ptr+orlen) + 1);
        memcpy(ptr, repl, repllen);
    }
}

int main(int argc, char *argv[])
{
    FILE *input = stdin;
    char *linebuf = malloc(N_LINES * LINELEN), *line[N_LINES * 2];
    int inputline, outputline = 0;

    if (!linebuf) {
        fprintf(stderr, "No memory\n");
        return -1;
    }
    if (argc > 1) {
        input = fopen(argv[1], "r");
        if (!input) {
            fprintf(stderr, "Cannot open file %s for input: %s\n",
                    argv[1], strerror(errno));
            exit(1);
        }
    }
    if (argc > 2) cntr = atoi(argv[2]);

    for (inputline = 0; inputline < N_LINES; inputline++) {
        linebuf[inputline * LINELEN + LINELEN - 1] = 0;
        line[inputline] = line[inputline + N_LINES] = linebuf + inputline * LINELEN;
        if (!fgets(line[inputline], LINELEN - 1, input))
            break;
        scan_input(line[inputline]);
    }
    if (ferror(input)) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return 1;
    } else if (!feof(input)) {
        for (;;) {
            if (inputline - outputline == N_LINES)
                outputline += output(line + (outputline % N_LINES),
                                     inputline - outputline);
            if (!fgets(line[inputline % N_LINES], LINELEN - 1, input))
                break;
            scan_input(line[inputline % N_LINES]);
            inputline++;
        }

        if (ferror(input)) {
            fprintf(stderr, "Error: %s\n", strerror(errno));
            return 1;
        } else if (!feof(input)) {
            fprintf(stderr, "Unexpected non-EOF\n");
            return 1;
        }
    }
    if (input != stdin)
        fclose(input);

    while (inputline > outputline) {
        outputline += output(line + (outputline % N_LINES),
                             inputline - outputline);
    }

    return 0;
}
