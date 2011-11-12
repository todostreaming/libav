/*
 * Copyright (C) 2004 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (C) 2006 Robert Edele <yartrebo@earthlink.net>
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

#ifndef AVCODEC_SNOW_H
#define AVCODEC_SNOW_H

#include "dsputil.h"
#include "dwt.h"

#include "rangecoder.h"
#include "mathops.h"
#include "mpegvideo.h"

#define MID_STATE 128

#define MAX_PLANES 4
#define QSHIFT 5
#define QROOT (1<<QSHIFT)
#define LOSSLESS_QLOG -128
#define FRAC_BITS 4
#define MAX_REF_FRAMES 8

#define LOG2_OBMC_MAX 8
#define OBMC_MAX (1<<(LOG2_OBMC_MAX))
typedef struct BlockNode{
    int16_t mx;
    int16_t my;
    uint8_t ref;
    uint8_t color[3];
    uint8_t type;
//#define TYPE_SPLIT    1
#define BLOCK_INTRA   1
#define BLOCK_OPT     2
//#define TYPE_NOCOLOR  4
    uint8_t level; //FIXME merge into type?
}BlockNode;

static const BlockNode null_block= { //FIXME add border maybe
    .color= {128,128,128},
    .mx= 0,
    .my= 0,
    .ref= 0,
    .type= 0,
    .level= 0,
};

#define LOG2_MB_SIZE 4
#define MB_SIZE (1<<LOG2_MB_SIZE)
#define ENCODER_EXTRA_BITS 4
#define HTAPS_MAX 8

typedef struct x_and_coeff{
    int16_t x;
    uint16_t coeff;
} x_and_coeff;

typedef struct SubBand{
    int level;
    int stride;
    int width;
    int height;
    int qlog;        ///< log(qscale)/log[2^(1/6)]
    DWTELEM *buf;
    IDWTELEM *ibuf;
    int buf_x_offset;
    int buf_y_offset;
    int stride_line; ///< Stride measured in lines, not pixels.
    x_and_coeff * x_coeff;
    struct SubBand *parent;
    uint8_t state[/*7*2*/ 7 + 512][32];
}SubBand;

typedef struct Plane{
    int width;
    int height;
    SubBand band[MAX_DECOMPOSITIONS][4];

    int htaps;
    int8_t hcoeff[HTAPS_MAX/2];
    int diag_mc;
    int fast_mc;

    int last_htaps;
    int8_t last_hcoeff[HTAPS_MAX/2];
    int last_diag_mc;
}Plane;

typedef struct SnowContext{
    AVClass *class;
    AVCodecContext *avctx;
    RangeCoder c;
    DSPContext dsp;
    DWTContext dwt;
    AVFrame new_picture;
    AVFrame input_picture;              ///< new_picture with the internal linesizes
    AVFrame current_picture;
    AVFrame last_picture[MAX_REF_FRAMES];
    uint8_t *halfpel_plane[MAX_REF_FRAMES][4][4];
    AVFrame mconly_picture;
//     uint8_t q_context[16];
    uint8_t header_state[32];
    uint8_t block_state[128 + 32*128];
    int keyframe;
    int always_reset;
    int version;
    int spatial_decomposition_type;
    int last_spatial_decomposition_type;
    int temporal_decomposition_type;
    int spatial_decomposition_count;
    int last_spatial_decomposition_count;
    int temporal_decomposition_count;
    int max_ref_frames;
    int ref_frames;
    int16_t (*ref_mvs[MAX_REF_FRAMES])[2];
    uint32_t *ref_scores[MAX_REF_FRAMES];
    DWTELEM *spatial_dwt_buffer;
    IDWTELEM *spatial_idwt_buffer;
    int colorspace_type;
    int chroma_h_shift;
    int chroma_v_shift;
    int spatial_scalability;
    int qlog;
    int last_qlog;
    int lambda;
    int lambda2;
    int pass1_rc;
    int mv_scale;
    int last_mv_scale;
    int qbias;
    int last_qbias;
#define QBIAS_SHIFT 3
    int b_width;
    int b_height;
    int block_max_depth;
    int last_block_max_depth;
    Plane plane[MAX_PLANES];
    BlockNode *block;
#define ME_CACHE_SIZE 1024
    int me_cache[ME_CACHE_SIZE];
    int me_cache_generation;
    slice_buffer sb;
    int memc_only;

    MpegEncContext m; // needed for motion estimation, should not be used for anything else, the idea is to eventually make the motion estimation independent of MpegEncContext, so this will be removed then (FIXME/XXX)

    uint8_t *scratchbuf;
}SnowContext;

/* Tables */
extern const uint8_t * const obmc_tab[4];
#ifdef __sgi
// Avoid a name clash on SGI IRIX
#undef qexp
#endif
extern uint8_t qexp[QROOT];
extern int scale_mv_ref[MAX_REF_FRAMES][MAX_REF_FRAMES];

/* C bits used by mmx/sse2/altivec */

static av_always_inline void snow_interleave_line_header(int * i, int width, IDWTELEM * low, IDWTELEM * high){
    (*i) = (width) - 2;

    if (width & 1){
        low[(*i)+1] = low[((*i)+1)>>1];
        (*i)--;
    }
}

static av_always_inline void snow_interleave_line_footer(int * i, IDWTELEM * low, IDWTELEM * high){
    for (; (*i)>=0; (*i)-=2){
        low[(*i)+1] = high[(*i)>>1];
        low[*i] = low[(*i)>>1];
    }
}

static av_always_inline void snow_horizontal_compose_lift_lead_out(int i, IDWTELEM * dst, IDWTELEM * src, IDWTELEM * ref, int width, int w, int lift_high, int mul, int add, int shift){
    for(; i<w; i++){
        dst[i] = src[i] - ((mul * (ref[i] + ref[i + 1]) + add) >> shift);
    }

    if((width^lift_high)&1){
        dst[w] = src[w] - ((mul * 2 * ref[w] + add) >> shift);
    }
}

static av_always_inline void snow_horizontal_compose_liftS_lead_out(int i, IDWTELEM * dst, IDWTELEM * src, IDWTELEM * ref, int width, int w){
        for(; i<w; i++){
            dst[i] = src[i] + ((ref[i] + ref[(i+1)]+W_BO + 4 * src[i]) >> W_BS);
        }

        if(width&1){
            dst[w] = src[w] + ((2 * ref[w] + W_BO + 4 * src[w]) >> W_BS);
        }
}

/* common inline functions */
//XXX doublecheck all of them should stay inlined

static inline void snow_set_blocks(SnowContext *s, int level, int x, int y, int l, int cb, int cr, int mx, int my, int ref, int type){
    const int w= s->b_width << s->block_max_depth;
    const int rem_depth= s->block_max_depth - level;
    const int index= (x + y*w) << rem_depth;
    const int block_w= 1<<rem_depth;
    BlockNode block;
    int i,j;

    block.color[0]= l;
    block.color[1]= cb;
    block.color[2]= cr;
    block.mx= mx;
    block.my= my;
    block.ref= ref;
    block.type= type;
    block.level= level;

    for(j=0; j<block_w; j++){
        for(i=0; i<block_w; i++){
            s->block[index + i + j*w]= block;
        }
    }
}

static inline void pred_mv(SnowContext *s, int *mx, int *my, int ref,
                           const BlockNode *left, const BlockNode *top, const BlockNode *tr){
    if(s->ref_frames == 1){
        *mx = mid_pred(left->mx, top->mx, tr->mx);
        *my = mid_pred(left->my, top->my, tr->my);
    }else{
        const int *scale = scale_mv_ref[ref];
        *mx = mid_pred((left->mx * scale[left->ref] + 128) >>8,
                       (top ->mx * scale[top ->ref] + 128) >>8,
                       (tr  ->mx * scale[tr  ->ref] + 128) >>8);
        *my = mid_pred((left->my * scale[left->ref] + 128) >>8,
                       (top ->my * scale[top ->ref] + 128) >>8,
                       (tr  ->my * scale[tr  ->ref] + 128) >>8);
    }
}

static av_always_inline int same_block(BlockNode *a, BlockNode *b){
    if((a->type&BLOCK_INTRA) && (b->type&BLOCK_INTRA)){
        return !((a->color[0] - b->color[0]) | (a->color[1] - b->color[1]) | (a->color[2] - b->color[2]));
    }else{
        return !((a->mx - b->mx) | (a->my - b->my) | (a->ref - b->ref) | ((a->type ^ b->type)&BLOCK_INTRA));
    }
}

/* common code */

void snow_reset_contexts(SnowContext *s);
int snow_alloc_blocks(SnowContext *s);
int snow_common_init(AVCodecContext *avctx);
void snow_pred_block(SnowContext *s, uint8_t *dst, uint8_t *tmp, int stride,
                     int sx, int sy, int b_w, int b_h, BlockNode *block,
                     int plane_index, int w, int h);
#endif /* AVCODEC_SNOW_H */
