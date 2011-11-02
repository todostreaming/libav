/*
 * Copyright (C) 2004 Michael Niedermayer <michaelni@gmx.at>
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

#include "libavutil/intmath.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "avcodec.h"
#include "dsputil.h"
#include "dwt.h"
#include "snow.h"

#include "rangecoder.h"
#include "mathops.h"

#include "mpegvideo.h"
#include "h263.h"

#undef NDEBUG
#include <assert.h>

static const int8_t quant3bA[256]={
 0, 0, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1,
};

static const uint8_t obmc32[1024]={
  0,  0,  0,  0,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,  8,  4,  4,  4,  4,  4,  4,  4,  4,  0,  0,  0,  0,
  0,  4,  4,  4,  8,  8,  8, 12, 12, 16, 16, 16, 20, 20, 20, 24, 24, 20, 20, 20, 16, 16, 16, 12, 12,  8,  8,  8,  4,  4,  4,  0,
  0,  4,  8,  8, 12, 12, 16, 20, 20, 24, 28, 28, 32, 32, 36, 40, 40, 36, 32, 32, 28, 28, 24, 20, 20, 16, 12, 12,  8,  8,  4,  0,
  0,  4,  8, 12, 16, 20, 24, 28, 28, 32, 36, 40, 44, 48, 52, 56, 56, 52, 48, 44, 40, 36, 32, 28, 28, 24, 20, 16, 12,  8,  4,  0,
  4,  8, 12, 16, 20, 24, 28, 32, 40, 44, 48, 52, 56, 60, 64, 68, 68, 64, 60, 56, 52, 48, 44, 40, 32, 28, 24, 20, 16, 12,  8,  4,
  4,  8, 12, 20, 24, 32, 36, 40, 48, 52, 56, 64, 68, 76, 80, 84, 84, 80, 76, 68, 64, 56, 52, 48, 40, 36, 32, 24, 20, 12,  8,  4,
  4,  8, 16, 24, 28, 36, 44, 48, 56, 60, 68, 76, 80, 88, 96,100,100, 96, 88, 80, 76, 68, 60, 56, 48, 44, 36, 28, 24, 16,  8,  4,
  4, 12, 20, 28, 32, 40, 48, 56, 64, 72, 80, 88, 92,100,108,116,116,108,100, 92, 88, 80, 72, 64, 56, 48, 40, 32, 28, 20, 12,  4,
  4, 12, 20, 28, 40, 48, 56, 64, 72, 80, 88, 96,108,116,124,132,132,124,116,108, 96, 88, 80, 72, 64, 56, 48, 40, 28, 20, 12,  4,
  4, 16, 24, 32, 44, 52, 60, 72, 80, 92,100,108,120,128,136,148,148,136,128,120,108,100, 92, 80, 72, 60, 52, 44, 32, 24, 16,  4,
  4, 16, 28, 36, 48, 56, 68, 80, 88,100,112,120,132,140,152,164,164,152,140,132,120,112,100, 88, 80, 68, 56, 48, 36, 28, 16,  4,
  4, 16, 28, 40, 52, 64, 76, 88, 96,108,120,132,144,156,168,180,180,168,156,144,132,120,108, 96, 88, 76, 64, 52, 40, 28, 16,  4,
  8, 20, 32, 44, 56, 68, 80, 92,108,120,132,144,156,168,180,192,192,180,168,156,144,132,120,108, 92, 80, 68, 56, 44, 32, 20,  8,
  8, 20, 32, 48, 60, 76, 88,100,116,128,140,156,168,184,196,208,208,196,184,168,156,140,128,116,100, 88, 76, 60, 48, 32, 20,  8,
  8, 20, 36, 52, 64, 80, 96,108,124,136,152,168,180,196,212,224,224,212,196,180,168,152,136,124,108, 96, 80, 64, 52, 36, 20,  8,
  8, 24, 40, 56, 68, 84,100,116,132,148,164,180,192,208,224,240,240,224,208,192,180,164,148,132,116,100, 84, 68, 56, 40, 24,  8,
  8, 24, 40, 56, 68, 84,100,116,132,148,164,180,192,208,224,240,240,224,208,192,180,164,148,132,116,100, 84, 68, 56, 40, 24,  8,
  8, 20, 36, 52, 64, 80, 96,108,124,136,152,168,180,196,212,224,224,212,196,180,168,152,136,124,108, 96, 80, 64, 52, 36, 20,  8,
  8, 20, 32, 48, 60, 76, 88,100,116,128,140,156,168,184,196,208,208,196,184,168,156,140,128,116,100, 88, 76, 60, 48, 32, 20,  8,
  8, 20, 32, 44, 56, 68, 80, 92,108,120,132,144,156,168,180,192,192,180,168,156,144,132,120,108, 92, 80, 68, 56, 44, 32, 20,  8,
  4, 16, 28, 40, 52, 64, 76, 88, 96,108,120,132,144,156,168,180,180,168,156,144,132,120,108, 96, 88, 76, 64, 52, 40, 28, 16,  4,
  4, 16, 28, 36, 48, 56, 68, 80, 88,100,112,120,132,140,152,164,164,152,140,132,120,112,100, 88, 80, 68, 56, 48, 36, 28, 16,  4,
  4, 16, 24, 32, 44, 52, 60, 72, 80, 92,100,108,120,128,136,148,148,136,128,120,108,100, 92, 80, 72, 60, 52, 44, 32, 24, 16,  4,
  4, 12, 20, 28, 40, 48, 56, 64, 72, 80, 88, 96,108,116,124,132,132,124,116,108, 96, 88, 80, 72, 64, 56, 48, 40, 28, 20, 12,  4,
  4, 12, 20, 28, 32, 40, 48, 56, 64, 72, 80, 88, 92,100,108,116,116,108,100, 92, 88, 80, 72, 64, 56, 48, 40, 32, 28, 20, 12,  4,
  4,  8, 16, 24, 28, 36, 44, 48, 56, 60, 68, 76, 80, 88, 96,100,100, 96, 88, 80, 76, 68, 60, 56, 48, 44, 36, 28, 24, 16,  8,  4,
  4,  8, 12, 20, 24, 32, 36, 40, 48, 52, 56, 64, 68, 76, 80, 84, 84, 80, 76, 68, 64, 56, 52, 48, 40, 36, 32, 24, 20, 12,  8,  4,
  4,  8, 12, 16, 20, 24, 28, 32, 40, 44, 48, 52, 56, 60, 64, 68, 68, 64, 60, 56, 52, 48, 44, 40, 32, 28, 24, 20, 16, 12,  8,  4,
  0,  4,  8, 12, 16, 20, 24, 28, 28, 32, 36, 40, 44, 48, 52, 56, 56, 52, 48, 44, 40, 36, 32, 28, 28, 24, 20, 16, 12,  8,  4,  0,
  0,  4,  8,  8, 12, 12, 16, 20, 20, 24, 28, 28, 32, 32, 36, 40, 40, 36, 32, 32, 28, 28, 24, 20, 20, 16, 12, 12,  8,  8,  4,  0,
  0,  4,  4,  4,  8,  8,  8, 12, 12, 16, 16, 16, 20, 20, 20, 24, 24, 20, 20, 20, 16, 16, 16, 12, 12,  8,  8,  8,  4,  4,  4,  0,
  0,  0,  0,  0,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,  8,  4,  4,  4,  4,  4,  4,  4,  4,  0,  0,  0,  0,
 //error:0.000020
};
static const uint8_t obmc16[256]={
  0,  4,  4,  8,  8, 12, 12, 16, 16, 12, 12,  8,  8,  4,  4,  0,
  4,  8, 16, 20, 28, 32, 40, 44, 44, 40, 32, 28, 20, 16,  8,  4,
  4, 16, 24, 36, 44, 56, 64, 76, 76, 64, 56, 44, 36, 24, 16,  4,
  8, 20, 36, 48, 64, 76, 92,104,104, 92, 76, 64, 48, 36, 20,  8,
  8, 28, 44, 64, 80,100,116,136,136,116,100, 80, 64, 44, 28,  8,
 12, 32, 56, 76,100,120,144,164,164,144,120,100, 76, 56, 32, 12,
 12, 40, 64, 92,116,144,168,196,196,168,144,116, 92, 64, 40, 12,
 16, 44, 76,104,136,164,196,224,224,196,164,136,104, 76, 44, 16,
 16, 44, 76,104,136,164,196,224,224,196,164,136,104, 76, 44, 16,
 12, 40, 64, 92,116,144,168,196,196,168,144,116, 92, 64, 40, 12,
 12, 32, 56, 76,100,120,144,164,164,144,120,100, 76, 56, 32, 12,
  8, 28, 44, 64, 80,100,116,136,136,116,100, 80, 64, 44, 28,  8,
  8, 20, 36, 48, 64, 76, 92,104,104, 92, 76, 64, 48, 36, 20,  8,
  4, 16, 24, 36, 44, 56, 64, 76, 76, 64, 56, 44, 36, 24, 16,  4,
  4,  8, 16, 20, 28, 32, 40, 44, 44, 40, 32, 28, 20, 16,  8,  4,
  0,  4,  4,  8,  8, 12, 12, 16, 16, 12, 12,  8,  8,  4,  4,  0,
//error:0.000015
};

//linear *64
static const uint8_t obmc8[64]={
  4, 12, 20, 28, 28, 20, 12,  4,
 12, 36, 60, 84, 84, 60, 36, 12,
 20, 60,100,140,140,100, 60, 20,
 28, 84,140,196,196,140, 84, 28,
 28, 84,140,196,196,140, 84, 28,
 20, 60,100,140,140,100, 60, 20,
 12, 36, 60, 84, 84, 60, 36, 12,
  4, 12, 20, 28, 28, 20, 12,  4,
//error:0.000000
};

//linear *64
static const uint8_t obmc4[16]={
 16, 48, 48, 16,
 48,144,144, 48,
 48,144,144, 48,
 16, 48, 48, 16,
//error:0.000000
};

static const uint8_t * const obmc_tab[4]={
    obmc32, obmc16, obmc8, obmc4
};

static int scale_mv_ref[MAX_REF_FRAMES][MAX_REF_FRAMES];

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

#ifdef __sgi
// Avoid a name clash on SGI IRIX
#undef qexp
#endif
#define QEXPSHIFT (7-FRAC_BITS+8) //FIXME try to change this to 0
static uint8_t qexp[QROOT];

static inline void put_symbol(RangeCoder *c, uint8_t *state, int v, int is_signed){
    int i;

    if(v){
        const int a= FFABS(v);
        const int e= av_log2(a);
        const int el= FFMIN(e, 10);
        put_rac(c, state+0, 0);

        for(i=0; i<el; i++){
            put_rac(c, state+1+i, 1);  //1..10
        }
        for(; i<e; i++){
            put_rac(c, state+1+9, 1);  //1..10
        }
        put_rac(c, state+1+FFMIN(i,9), 0);

        for(i=e-1; i>=el; i--){
            put_rac(c, state+22+9, (a>>i)&1); //22..31
        }
        for(; i>=0; i--){
            put_rac(c, state+22+i, (a>>i)&1); //22..31
        }

        if(is_signed)
            put_rac(c, state+11 + el, v < 0); //11..21
    }else{
        put_rac(c, state+0, 1);
    }
}

static inline int get_symbol(RangeCoder *c, uint8_t *state, int is_signed){
    if(get_rac(c, state+0))
        return 0;
    else{
        int i, e, a;
        e= 0;
        while(get_rac(c, state+1 + FFMIN(e,9))){ //1..10
            e++;
        }

        a= 1;
        for(i=e-1; i>=0; i--){
            a += a + get_rac(c, state+22 + FFMIN(i,9)); //22..31
        }

        e= -(is_signed && get_rac(c, state+11 + FFMIN(e,10))); //11..21
        return (a^e)-e;
    }
}

static inline void put_symbol2(RangeCoder *c, uint8_t *state, int v, int log2){
    int i;
    int r= log2>=0 ? 1<<log2 : 1;

    assert(v>=0);
    assert(log2>=-4);

    while(v >= r){
        put_rac(c, state+4+log2, 1);
        v -= r;
        log2++;
        if(log2>0) r+=r;
    }
    put_rac(c, state+4+log2, 0);

    for(i=log2-1; i>=0; i--){
        put_rac(c, state+31-i, (v>>i)&1);
    }
}

static inline int get_symbol2(RangeCoder *c, uint8_t *state, int log2){
    int i;
    int r= log2>=0 ? 1<<log2 : 1;
    int v=0;

    assert(log2>=-4);

    while(get_rac(c, state+4+log2)){
        v+= r;
        log2++;
        if(log2>0) r+=r;
    }

    for(i=log2-1; i>=0; i--){
        v+= get_rac(c, state+31-i)<<i;
    }

    return v;
}

static inline void unpack_coeffs(SnowContext *s, SubBand *b, SubBand * parent, int orientation){
    const int w= b->width;
    const int h= b->height;
    int x,y;

    int run, runs;
    x_and_coeff *xc= b->x_coeff;
    x_and_coeff *prev_xc= NULL;
    x_and_coeff *prev2_xc= xc;
    x_and_coeff *parent_xc= parent ? parent->x_coeff : NULL;
    x_and_coeff *prev_parent_xc= parent_xc;

    runs= get_symbol2(&s->c, b->state[30], 0);
    if(runs-- > 0) run= get_symbol2(&s->c, b->state[1], 3);
    else           run= INT_MAX;

    for(y=0; y<h; y++){
        int v=0;
        int lt=0, t=0, rt=0;

        if(y && prev_xc->x == 0){
            rt= prev_xc->coeff;
        }
        for(x=0; x<w; x++){
            int p=0;
            const int l= v;

            lt= t; t= rt;

            if(y){
                if(prev_xc->x <= x)
                    prev_xc++;
                if(prev_xc->x == x + 1)
                    rt= prev_xc->coeff;
                else
                    rt=0;
            }
            if(parent_xc){
                if(x>>1 > parent_xc->x){
                    parent_xc++;
                }
                if(x>>1 == parent_xc->x){
                    p= parent_xc->coeff;
                }
            }
            if(/*ll|*/l|lt|t|rt|p){
                int context= av_log2(/*FFABS(ll) + */3*(l>>1) + (lt>>1) + (t&~1) + (rt>>1) + (p>>1));

                v=get_rac(&s->c, &b->state[0][context]);
                if(v){
                    v= 2*(get_symbol2(&s->c, b->state[context + 2], context-4) + 1);
                    v+=get_rac(&s->c, &b->state[0][16 + 1 + 3 + quant3bA[l&0xFF] + 3*quant3bA[t&0xFF]]);

                    xc->x=x;
                    (xc++)->coeff= v;
                }
            }else{
                if(!run){
                    if(runs-- > 0) run= get_symbol2(&s->c, b->state[1], 3);
                    else           run= INT_MAX;
                    v= 2*(get_symbol2(&s->c, b->state[0 + 2], 0-4) + 1);
                    v+=get_rac(&s->c, &b->state[0][16 + 1 + 3]);

                    xc->x=x;
                    (xc++)->coeff= v;
                }else{
                    int max_run;
                    run--;
                    v=0;

                    if(y) max_run= FFMIN(run, prev_xc->x - x - 2);
                    else  max_run= FFMIN(run, w-x-1);
                    if(parent_xc)
                        max_run= FFMIN(max_run, 2*parent_xc->x - x - 1);
                    x+= max_run;
                    run-= max_run;
                }
            }
        }
        (xc++)->x= w+1; //end marker
        prev_xc= prev2_xc;
        prev2_xc= xc;

        if(parent_xc){
            if(y&1){
                while(parent_xc->x != parent->width+1)
                    parent_xc++;
                parent_xc++;
                prev_parent_xc= parent_xc;
            }else{
                parent_xc= prev_parent_xc;
            }
        }
    }

    (xc++)->x= w+1; //end marker
}

static inline void decode_subband_slice_buffered(SnowContext *s, SubBand *b, slice_buffer * sb, int start_y, int h, int save_state[1]){
    const int w= b->width;
    int y;
    const int qlog= av_clip(s->qlog + b->qlog, 0, QROOT*16);
    int qmul= qexp[qlog&(QROOT-1)]<<(qlog>>QSHIFT);
    int qadd= (s->qbias*qmul)>>QBIAS_SHIFT;
    int new_index = 0;

    if(b->ibuf == s->spatial_idwt_buffer || s->qlog == LOSSLESS_QLOG){
        qadd= 0;
        qmul= 1<<QEXPSHIFT;
    }

    /* If we are on the second or later slice, restore our index. */
    if (start_y != 0)
        new_index = save_state[0];


    for(y=start_y; y<h; y++){
        int x = 0;
        int v;
        IDWTELEM * line = slice_buffer_get_line(sb, y * b->stride_line + b->buf_y_offset) + b->buf_x_offset;
        memset(line, 0, b->width*sizeof(IDWTELEM));
        v = b->x_coeff[new_index].coeff;
        x = b->x_coeff[new_index++].x;
        while(x < w){
            register int t= ( (v>>1)*qmul + qadd)>>QEXPSHIFT;
            register int u= -(v&1);
            line[x] = (t^u) - u;

            v = b->x_coeff[new_index].coeff;
            x = b->x_coeff[new_index++].x;
        }
    }

    /* Save our variables for the next slice. */
    save_state[0] = new_index;

    return;
}

static void reset_contexts(SnowContext *s){ //FIXME better initial contexts
    int plane_index, level, orientation;

    for(plane_index=0; plane_index<3; plane_index++){
        for(level=0; level<MAX_DECOMPOSITIONS; level++){
            for(orientation=level ? 1:0; orientation<4; orientation++){
                memset(s->plane[plane_index].band[level][orientation].state, MID_STATE, sizeof(s->plane[plane_index].band[level][orientation].state));
            }
        }
    }
    memset(s->header_state, MID_STATE, sizeof(s->header_state));
    memset(s->block_state, MID_STATE, sizeof(s->block_state));
}

static int alloc_blocks(SnowContext *s){
    int w= -((-s->avctx->width )>>LOG2_MB_SIZE);
    int h= -((-s->avctx->height)>>LOG2_MB_SIZE);

    s->b_width = w;
    s->b_height= h;

    av_free(s->block);
    s->block= av_mallocz(w * h * sizeof(BlockNode) << (s->block_max_depth*2));
    return 0;
}

static inline void set_blocks(SnowContext *s, int level, int x, int y, int l, int cb, int cr, int mx, int my, int ref, int type){
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

static inline void init_ref(MotionEstContext *c, uint8_t *src[3], uint8_t *ref[3], uint8_t *ref2[3], int x, int y, int ref_index){
    const int offset[3]= {
          y*c->  stride + x,
        ((y*c->uvstride + x)>>1),
        ((y*c->uvstride + x)>>1),
    };
    int i;
    for(i=0; i<3; i++){
        c->src[0][i]= src [i];
        c->ref[0][i]= ref [i] + offset[i];
    }
    assert(!ref_index);
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

static void decode_q_branch(SnowContext *s, int level, int x, int y){
    const int w= s->b_width << s->block_max_depth;
    const int rem_depth= s->block_max_depth - level;
    const int index= (x + y*w) << rem_depth;
    int trx= (x+1)<<rem_depth;
    const BlockNode *left  = x ? &s->block[index-1] : &null_block;
    const BlockNode *top   = y ? &s->block[index-w] : &null_block;
    const BlockNode *tl    = y && x ? &s->block[index-w-1] : left;
    const BlockNode *tr    = y && trx<w && ((x&1)==0 || level==0) ? &s->block[index-w+(1<<rem_depth)] : tl; //FIXME use lt
    int s_context= 2*left->level + 2*top->level + tl->level + tr->level;

    if(s->keyframe){
        set_blocks(s, level, x, y, null_block.color[0], null_block.color[1], null_block.color[2], null_block.mx, null_block.my, null_block.ref, BLOCK_INTRA);
        return;
    }

    if(level==s->block_max_depth || get_rac(&s->c, &s->block_state[4 + s_context])){
        int type, mx, my;
        int l = left->color[0];
        int cb= left->color[1];
        int cr= left->color[2];
        int ref = 0;
        int ref_context= av_log2(2*left->ref) + av_log2(2*top->ref);
        int mx_context= av_log2(2*FFABS(left->mx - top->mx)) + 0*av_log2(2*FFABS(tr->mx - top->mx));
        int my_context= av_log2(2*FFABS(left->my - top->my)) + 0*av_log2(2*FFABS(tr->my - top->my));

        type= get_rac(&s->c, &s->block_state[1 + left->type + top->type]) ? BLOCK_INTRA : 0;

        if(type){
            pred_mv(s, &mx, &my, 0, left, top, tr);
            l += get_symbol(&s->c, &s->block_state[32], 1);
            cb+= get_symbol(&s->c, &s->block_state[64], 1);
            cr+= get_symbol(&s->c, &s->block_state[96], 1);
        }else{
            if(s->ref_frames > 1)
                ref= get_symbol(&s->c, &s->block_state[128 + 1024 + 32*ref_context], 0);
            pred_mv(s, &mx, &my, ref, left, top, tr);
            mx+= get_symbol(&s->c, &s->block_state[128 + 32*(mx_context + 16*!!ref)], 1);
            my+= get_symbol(&s->c, &s->block_state[128 + 32*(my_context + 16*!!ref)], 1);
        }
        set_blocks(s, level, x, y, l, cb, cr, mx, my, ref, type);
    }else{
        decode_q_branch(s, level+1, 2*x+0, 2*y+0);
        decode_q_branch(s, level+1, 2*x+1, 2*y+0);
        decode_q_branch(s, level+1, 2*x+0, 2*y+1);
        decode_q_branch(s, level+1, 2*x+1, 2*y+1);
    }
}

static void decode_blocks(SnowContext *s){
    int x, y;
    int w= s->b_width;
    int h= s->b_height;

    for(y=0; y<h; y++){
        for(x=0; x<w; x++){
            decode_q_branch(s, 0, x, y);
        }
    }
}

static void mc_block(Plane *p, uint8_t *dst, const uint8_t *src, int stride, int b_w, int b_h, int dx, int dy){
    static const uint8_t weight[64]={
    8,7,6,5,4,3,2,1,
    7,7,0,0,0,0,0,1,
    6,0,6,0,0,0,2,0,
    5,0,0,5,0,3,0,0,
    4,0,0,0,4,0,0,0,
    3,0,0,5,0,3,0,0,
    2,0,6,0,0,0,2,0,
    1,7,0,0,0,0,0,1,
    };

    static const uint8_t brane[256]={
    0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x11,0x12,0x12,0x12,0x12,0x12,0x12,0x12,
    0x04,0x05,0xcc,0xcc,0xcc,0xcc,0xcc,0x41,0x15,0x16,0xcc,0xcc,0xcc,0xcc,0xcc,0x52,
    0x04,0xcc,0x05,0xcc,0xcc,0xcc,0x41,0xcc,0x15,0xcc,0x16,0xcc,0xcc,0xcc,0x52,0xcc,
    0x04,0xcc,0xcc,0x05,0xcc,0x41,0xcc,0xcc,0x15,0xcc,0xcc,0x16,0xcc,0x52,0xcc,0xcc,
    0x04,0xcc,0xcc,0xcc,0x41,0xcc,0xcc,0xcc,0x15,0xcc,0xcc,0xcc,0x16,0xcc,0xcc,0xcc,
    0x04,0xcc,0xcc,0x41,0xcc,0x05,0xcc,0xcc,0x15,0xcc,0xcc,0x52,0xcc,0x16,0xcc,0xcc,
    0x04,0xcc,0x41,0xcc,0xcc,0xcc,0x05,0xcc,0x15,0xcc,0x52,0xcc,0xcc,0xcc,0x16,0xcc,
    0x04,0x41,0xcc,0xcc,0xcc,0xcc,0xcc,0x05,0x15,0x52,0xcc,0xcc,0xcc,0xcc,0xcc,0x16,
    0x44,0x45,0x45,0x45,0x45,0x45,0x45,0x45,0x55,0x56,0x56,0x56,0x56,0x56,0x56,0x56,
    0x48,0x49,0xcc,0xcc,0xcc,0xcc,0xcc,0x85,0x59,0x5A,0xcc,0xcc,0xcc,0xcc,0xcc,0x96,
    0x48,0xcc,0x49,0xcc,0xcc,0xcc,0x85,0xcc,0x59,0xcc,0x5A,0xcc,0xcc,0xcc,0x96,0xcc,
    0x48,0xcc,0xcc,0x49,0xcc,0x85,0xcc,0xcc,0x59,0xcc,0xcc,0x5A,0xcc,0x96,0xcc,0xcc,
    0x48,0xcc,0xcc,0xcc,0x49,0xcc,0xcc,0xcc,0x59,0xcc,0xcc,0xcc,0x96,0xcc,0xcc,0xcc,
    0x48,0xcc,0xcc,0x85,0xcc,0x49,0xcc,0xcc,0x59,0xcc,0xcc,0x96,0xcc,0x5A,0xcc,0xcc,
    0x48,0xcc,0x85,0xcc,0xcc,0xcc,0x49,0xcc,0x59,0xcc,0x96,0xcc,0xcc,0xcc,0x5A,0xcc,
    0x48,0x85,0xcc,0xcc,0xcc,0xcc,0xcc,0x49,0x59,0x96,0xcc,0xcc,0xcc,0xcc,0xcc,0x5A,
    };

    static const uint8_t needs[16]={
    0,1,0,0,
    2,4,2,0,
    0,1,0,0,
    15
    };

    int x, y, b, r, l;
    int16_t tmpIt   [64*(32+HTAPS_MAX)];
    uint8_t tmp2t[3][stride*(32+HTAPS_MAX)];
    int16_t *tmpI= tmpIt;
    uint8_t *tmp2= tmp2t[0];
    const uint8_t *hpel[11];
    assert(dx<16 && dy<16);
    r= brane[dx + 16*dy]&15;
    l= brane[dx + 16*dy]>>4;

    b= needs[l] | needs[r];
    if(p && !p->diag_mc)
        b= 15;

    if(b&5){
        for(y=0; y < b_h+HTAPS_MAX-1; y++){
            for(x=0; x < b_w; x++){
                int a_1=src[x + HTAPS_MAX/2-4];
                int a0= src[x + HTAPS_MAX/2-3];
                int a1= src[x + HTAPS_MAX/2-2];
                int a2= src[x + HTAPS_MAX/2-1];
                int a3= src[x + HTAPS_MAX/2+0];
                int a4= src[x + HTAPS_MAX/2+1];
                int a5= src[x + HTAPS_MAX/2+2];
                int a6= src[x + HTAPS_MAX/2+3];
                int am=0;
                if(!p || p->fast_mc){
                    am= 20*(a2+a3) - 5*(a1+a4) + (a0+a5);
                    tmpI[x]= am;
                    am= (am+16)>>5;
                }else{
                    am= p->hcoeff[0]*(a2+a3) + p->hcoeff[1]*(a1+a4) + p->hcoeff[2]*(a0+a5) + p->hcoeff[3]*(a_1+a6);
                    tmpI[x]= am;
                    am= (am+32)>>6;
                }

                if(am&(~255)) am= ~(am>>31);
                tmp2[x]= am;
            }
            tmpI+= 64;
            tmp2+= stride;
            src += stride;
        }
        src -= stride*y;
    }
    src += HTAPS_MAX/2 - 1;
    tmp2= tmp2t[1];

    if(b&2){
        for(y=0; y < b_h; y++){
            for(x=0; x < b_w+1; x++){
                int a_1=src[x + (HTAPS_MAX/2-4)*stride];
                int a0= src[x + (HTAPS_MAX/2-3)*stride];
                int a1= src[x + (HTAPS_MAX/2-2)*stride];
                int a2= src[x + (HTAPS_MAX/2-1)*stride];
                int a3= src[x + (HTAPS_MAX/2+0)*stride];
                int a4= src[x + (HTAPS_MAX/2+1)*stride];
                int a5= src[x + (HTAPS_MAX/2+2)*stride];
                int a6= src[x + (HTAPS_MAX/2+3)*stride];
                int am=0;
                if(!p || p->fast_mc)
                    am= (20*(a2+a3) - 5*(a1+a4) + (a0+a5) + 16)>>5;
                else
                    am= (p->hcoeff[0]*(a2+a3) + p->hcoeff[1]*(a1+a4) + p->hcoeff[2]*(a0+a5) + p->hcoeff[3]*(a_1+a6) + 32)>>6;

                if(am&(~255)) am= ~(am>>31);
                tmp2[x]= am;
            }
            src += stride;
            tmp2+= stride;
        }
        src -= stride*y;
    }
    src += stride*(HTAPS_MAX/2 - 1);
    tmp2= tmp2t[2];
    tmpI= tmpIt;
    if(b&4){
        for(y=0; y < b_h; y++){
            for(x=0; x < b_w; x++){
                int a_1=tmpI[x + (HTAPS_MAX/2-4)*64];
                int a0= tmpI[x + (HTAPS_MAX/2-3)*64];
                int a1= tmpI[x + (HTAPS_MAX/2-2)*64];
                int a2= tmpI[x + (HTAPS_MAX/2-1)*64];
                int a3= tmpI[x + (HTAPS_MAX/2+0)*64];
                int a4= tmpI[x + (HTAPS_MAX/2+1)*64];
                int a5= tmpI[x + (HTAPS_MAX/2+2)*64];
                int a6= tmpI[x + (HTAPS_MAX/2+3)*64];
                int am=0;
                if(!p || p->fast_mc)
                    am= (20*(a2+a3) - 5*(a1+a4) + (a0+a5) + 512)>>10;
                else
                    am= (p->hcoeff[0]*(a2+a3) + p->hcoeff[1]*(a1+a4) + p->hcoeff[2]*(a0+a5) + p->hcoeff[3]*(a_1+a6) + 2048)>>12;
                if(am&(~255)) am= ~(am>>31);
                tmp2[x]= am;
            }
            tmpI+= 64;
            tmp2+= stride;
        }
    }

    hpel[ 0]= src;
    hpel[ 1]= tmp2t[0] + stride*(HTAPS_MAX/2-1);
    hpel[ 2]= src + 1;

    hpel[ 4]= tmp2t[1];
    hpel[ 5]= tmp2t[2];
    hpel[ 6]= tmp2t[1] + 1;

    hpel[ 8]= src + stride;
    hpel[ 9]= hpel[1] + stride;
    hpel[10]= hpel[8] + 1;

    if(b==15){
        const uint8_t *src1= hpel[dx/8 + dy/8*4  ];
        const uint8_t *src2= hpel[dx/8 + dy/8*4+1];
        const uint8_t *src3= hpel[dx/8 + dy/8*4+4];
        const uint8_t *src4= hpel[dx/8 + dy/8*4+5];
        dx&=7;
        dy&=7;
        for(y=0; y < b_h; y++){
            for(x=0; x < b_w; x++){
                dst[x]= ((8-dx)*(8-dy)*src1[x] + dx*(8-dy)*src2[x]+
                         (8-dx)*   dy *src3[x] + dx*   dy *src4[x]+32)>>6;
            }
            src1+=stride;
            src2+=stride;
            src3+=stride;
            src4+=stride;
            dst +=stride;
        }
    }else{
        const uint8_t *src1= hpel[l];
        const uint8_t *src2= hpel[r];
        int a= weight[((dx&7) + (8*(dy&7)))];
        int b= 8-a;
        for(y=0; y < b_h; y++){
            for(x=0; x < b_w; x++){
                dst[x]= (a*src1[x] + b*src2[x] + 4)>>3;
            }
            src1+=stride;
            src2+=stride;
            dst +=stride;
        }
    }
}

#define mca(dx,dy,b_w)\
static void mc_block_hpel ## dx ## dy ## b_w(uint8_t *dst, const uint8_t *src, int stride, int h){\
    assert(h==b_w);\
    mc_block(NULL, dst, src-(HTAPS_MAX/2-1)-(HTAPS_MAX/2-1)*stride, stride, b_w, b_w, dx, dy);\
}

mca( 0, 0,16)
mca( 8, 0,16)
mca( 0, 8,16)
mca( 8, 8,16)
mca( 0, 0,8)
mca( 8, 0,8)
mca( 0, 8,8)
mca( 8, 8,8)

static void pred_block(SnowContext *s, uint8_t *dst, uint8_t *tmp, int stride, int sx, int sy, int b_w, int b_h, BlockNode *block, int plane_index, int w, int h){
    if(block->type & BLOCK_INTRA){
        int x, y;
        const int color = block->color[plane_index];
        const int color4= color*0x01010101;
        if(b_w==32){
            for(y=0; y < b_h; y++){
                *(uint32_t*)&dst[0 + y*stride]= color4;
                *(uint32_t*)&dst[4 + y*stride]= color4;
                *(uint32_t*)&dst[8 + y*stride]= color4;
                *(uint32_t*)&dst[12+ y*stride]= color4;
                *(uint32_t*)&dst[16+ y*stride]= color4;
                *(uint32_t*)&dst[20+ y*stride]= color4;
                *(uint32_t*)&dst[24+ y*stride]= color4;
                *(uint32_t*)&dst[28+ y*stride]= color4;
            }
        }else if(b_w==16){
            for(y=0; y < b_h; y++){
                *(uint32_t*)&dst[0 + y*stride]= color4;
                *(uint32_t*)&dst[4 + y*stride]= color4;
                *(uint32_t*)&dst[8 + y*stride]= color4;
                *(uint32_t*)&dst[12+ y*stride]= color4;
            }
        }else if(b_w==8){
            for(y=0; y < b_h; y++){
                *(uint32_t*)&dst[0 + y*stride]= color4;
                *(uint32_t*)&dst[4 + y*stride]= color4;
            }
        }else if(b_w==4){
            for(y=0; y < b_h; y++){
                *(uint32_t*)&dst[0 + y*stride]= color4;
            }
        }else{
            for(y=0; y < b_h; y++){
                for(x=0; x < b_w; x++){
                    dst[x + y*stride]= color;
                }
            }
        }
    }else{
        uint8_t *src= s->last_picture[block->ref].data[plane_index];
        const int scale= plane_index ?  s->mv_scale : 2*s->mv_scale;
        int mx= block->mx*scale;
        int my= block->my*scale;
        const int dx= mx&15;
        const int dy= my&15;
        const int tab_index= 3 - (b_w>>2) + (b_w>>4);
        sx += (mx>>4) - (HTAPS_MAX/2-1);
        sy += (my>>4) - (HTAPS_MAX/2-1);
        src += sx + sy*stride;
        if(   (unsigned)sx >= w - b_w - (HTAPS_MAX-2)
           || (unsigned)sy >= h - b_h - (HTAPS_MAX-2)){
            s->dsp.emulated_edge_mc(tmp + MB_SIZE, src, stride, b_w+HTAPS_MAX-1, b_h+HTAPS_MAX-1, sx, sy, w, h);
            src= tmp + MB_SIZE;
        }
//        assert(b_w == b_h || 2*b_w == b_h || b_w == 2*b_h);
//        assert(!(b_w&(b_w-1)));
        assert(b_w>1 && b_h>1);
        assert((tab_index>=0 && tab_index<4) || b_w==32);
        if((dx&3) || (dy&3) || !(b_w == b_h || 2*b_w == b_h || b_w == 2*b_h) || (b_w&(b_w-1)) || !s->plane[plane_index].fast_mc )
            mc_block(&s->plane[plane_index], dst, src, stride, b_w, b_h, dx, dy);
        else if(b_w==32){
            int y;
            for(y=0; y<b_h; y+=16){
                s->dsp.put_h264_qpel_pixels_tab[0][dy+(dx>>2)](dst + y*stride, src + 3 + (y+3)*stride,stride);
                s->dsp.put_h264_qpel_pixels_tab[0][dy+(dx>>2)](dst + 16 + y*stride, src + 19 + (y+3)*stride,stride);
            }
        }else if(b_w==b_h)
            s->dsp.put_h264_qpel_pixels_tab[tab_index  ][dy+(dx>>2)](dst,src + 3 + 3*stride,stride);
        else if(b_w==2*b_h){
            s->dsp.put_h264_qpel_pixels_tab[tab_index+1][dy+(dx>>2)](dst    ,src + 3       + 3*stride,stride);
            s->dsp.put_h264_qpel_pixels_tab[tab_index+1][dy+(dx>>2)](dst+b_h,src + 3 + b_h + 3*stride,stride);
        }else{
            assert(2*b_w==b_h);
            s->dsp.put_h264_qpel_pixels_tab[tab_index  ][dy+(dx>>2)](dst           ,src + 3 + 3*stride           ,stride);
            s->dsp.put_h264_qpel_pixels_tab[tab_index  ][dy+(dx>>2)](dst+b_w*stride,src + 3 + 3*stride+b_w*stride,stride);
        }
    }
}

//FIXME name cleanup (b_w, block_w, b_width stuff)
static av_always_inline void add_yblock(SnowContext *s, int sliced, slice_buffer *sb, IDWTELEM *dst, uint8_t *dst8, const uint8_t *obmc, int src_x, int src_y, int b_w, int b_h, int w, int h, int dst_stride, int src_stride, int obmc_stride, int b_x, int b_y, int add, int offset_dst, int plane_index){
    const int b_width = s->b_width  << s->block_max_depth;
    const int b_height= s->b_height << s->block_max_depth;
    const int b_stride= b_width;
    BlockNode *lt= &s->block[b_x + b_y*b_stride];
    BlockNode *rt= lt+1;
    BlockNode *lb= lt+b_stride;
    BlockNode *rb= lb+1;
    uint8_t *block[4];
    int tmp_step= src_stride >= 7*MB_SIZE ? MB_SIZE : MB_SIZE*src_stride;
    uint8_t *tmp = s->scratchbuf;
    uint8_t *ptmp;
    int x,y;

    if(b_x<0){
        lt= rt;
        lb= rb;
    }else if(b_x + 1 >= b_width){
        rt= lt;
        rb= lb;
    }
    if(b_y<0){
        lt= lb;
        rt= rb;
    }else if(b_y + 1 >= b_height){
        lb= lt;
        rb= rt;
    }

    if(src_x<0){ //FIXME merge with prev & always round internal width up to *16
        obmc -= src_x;
        b_w += src_x;
        if(!sliced && !offset_dst)
            dst -= src_x;
        src_x=0;
    }else if(src_x + b_w > w){
        b_w = w - src_x;
    }
    if(src_y<0){
        obmc -= src_y*obmc_stride;
        b_h += src_y;
        if(!sliced && !offset_dst)
            dst -= src_y*dst_stride;
        src_y=0;
    }else if(src_y + b_h> h){
        b_h = h - src_y;
    }

    if(b_w<=0 || b_h<=0) return;

    assert(src_stride > 2*MB_SIZE + 5);

    if(!sliced && offset_dst)
        dst += src_x + src_y*dst_stride;
    dst8+= src_x + src_y*src_stride;
//    src += src_x + src_y*src_stride;

    ptmp= tmp + 3*tmp_step;
    block[0]= ptmp;
    ptmp+=tmp_step;
    pred_block(s, block[0], tmp, src_stride, src_x, src_y, b_w, b_h, lt, plane_index, w, h);

    if(same_block(lt, rt)){
        block[1]= block[0];
    }else{
        block[1]= ptmp;
        ptmp+=tmp_step;
        pred_block(s, block[1], tmp, src_stride, src_x, src_y, b_w, b_h, rt, plane_index, w, h);
    }

    if(same_block(lt, lb)){
        block[2]= block[0];
    }else if(same_block(rt, lb)){
        block[2]= block[1];
    }else{
        block[2]= ptmp;
        ptmp+=tmp_step;
        pred_block(s, block[2], tmp, src_stride, src_x, src_y, b_w, b_h, lb, plane_index, w, h);
    }

    if(same_block(lt, rb) ){
        block[3]= block[0];
    }else if(same_block(rt, rb)){
        block[3]= block[1];
    }else if(same_block(lb, rb)){
        block[3]= block[2];
    }else{
        block[3]= ptmp;
        pred_block(s, block[3], tmp, src_stride, src_x, src_y, b_w, b_h, rb, plane_index, w, h);
    }
    if(sliced){
        s->dwt.inner_add_yblock(obmc, obmc_stride, block, b_w, b_h, src_x,src_y, src_stride, sb, add, dst8);
    }else{
        for(y=0; y<b_h; y++){
            //FIXME ugly misuse of obmc_stride
            const uint8_t *obmc1= obmc + y*obmc_stride;
            const uint8_t *obmc2= obmc1+ (obmc_stride>>1);
            const uint8_t *obmc3= obmc1+ obmc_stride*(obmc_stride>>1);
            const uint8_t *obmc4= obmc3+ (obmc_stride>>1);
            for(x=0; x<b_w; x++){
                int v=   obmc1[x] * block[3][x + y*src_stride]
                        +obmc2[x] * block[2][x + y*src_stride]
                        +obmc3[x] * block[1][x + y*src_stride]
                        +obmc4[x] * block[0][x + y*src_stride];

                v <<= 8 - LOG2_OBMC_MAX;
                if(FRAC_BITS != 8){
                    v >>= 8 - FRAC_BITS;
                }
                if(add){
                    v += dst[x + y*dst_stride];
                    v = (v + (1<<(FRAC_BITS-1))) >> FRAC_BITS;
                    if(v&(~255)) v= ~(v>>31);
                    dst8[x + y*src_stride] = v;
                }else{
                    dst[x + y*dst_stride] -= v;
                }
            }
        }
    }
}

static av_always_inline void predict_slice_buffered(SnowContext *s, slice_buffer * sb, IDWTELEM * old_buffer, int plane_index, int add, int mb_y){
    Plane *p= &s->plane[plane_index];
    const int mb_w= s->b_width  << s->block_max_depth;
    const int mb_h= s->b_height << s->block_max_depth;
    int x, y, mb_x;
    int block_size = MB_SIZE >> s->block_max_depth;
    int block_w    = plane_index ? block_size/2 : block_size;
    const uint8_t *obmc  = plane_index ? obmc_tab[s->block_max_depth+1] : obmc_tab[s->block_max_depth];
    int obmc_stride= plane_index ? block_size : 2*block_size;
    int ref_stride= s->current_picture.linesize[plane_index];
    uint8_t *dst8= s->current_picture.data[plane_index];
    int w= p->width;
    int h= p->height;

    if(s->keyframe || (s->avctx->debug&512)){
        if(mb_y==mb_h)
            return;

        if(add){
            for(y=block_w*mb_y; y<FFMIN(h,block_w*(mb_y+1)); y++){
//                DWTELEM * line = slice_buffer_get_line(sb, y);
                IDWTELEM * line = sb->line[y];
                for(x=0; x<w; x++){
//                    int v= buf[x + y*w] + (128<<FRAC_BITS) + (1<<(FRAC_BITS-1));
                    int v= line[x] + (128<<FRAC_BITS) + (1<<(FRAC_BITS-1));
                    v >>= FRAC_BITS;
                    if(v&(~255)) v= ~(v>>31);
                    dst8[x + y*ref_stride]= v;
                }
            }
        }else{
            for(y=block_w*mb_y; y<FFMIN(h,block_w*(mb_y+1)); y++){
//                DWTELEM * line = slice_buffer_get_line(sb, y);
                IDWTELEM * line = sb->line[y];
                for(x=0; x<w; x++){
                    line[x] -= 128 << FRAC_BITS;
//                    buf[x + y*w]-= 128<<FRAC_BITS;
                }
            }
        }

        return;
    }

    for(mb_x=0; mb_x<=mb_w; mb_x++){
        add_yblock(s, 1, sb, old_buffer, dst8, obmc,
                   block_w*mb_x - block_w/2,
                   block_w*mb_y - block_w/2,
                   block_w, block_w,
                   w, h,
                   w, ref_stride, obmc_stride,
                   mb_x - 1, mb_y - 1,
                   add, 0, plane_index);
    }
}

static av_always_inline void predict_slice(SnowContext *s, IDWTELEM *buf, int plane_index, int add, int mb_y){
    Plane *p= &s->plane[plane_index];
    const int mb_w= s->b_width  << s->block_max_depth;
    const int mb_h= s->b_height << s->block_max_depth;
    int x, y, mb_x;
    int block_size = MB_SIZE >> s->block_max_depth;
    int block_w    = plane_index ? block_size/2 : block_size;
    const uint8_t *obmc  = plane_index ? obmc_tab[s->block_max_depth+1] : obmc_tab[s->block_max_depth];
    const int obmc_stride= plane_index ? block_size : 2*block_size;
    int ref_stride= s->current_picture.linesize[plane_index];
    uint8_t *dst8= s->current_picture.data[plane_index];
    int w= p->width;
    int h= p->height;

    if(s->keyframe || (s->avctx->debug&512)){
        if(mb_y==mb_h)
            return;

        if(add){
            for(y=block_w*mb_y; y<FFMIN(h,block_w*(mb_y+1)); y++){
                for(x=0; x<w; x++){
                    int v= buf[x + y*w] + (128<<FRAC_BITS) + (1<<(FRAC_BITS-1));
                    v >>= FRAC_BITS;
                    if(v&(~255)) v= ~(v>>31);
                    dst8[x + y*ref_stride]= v;
                }
            }
        }else{
            for(y=block_w*mb_y; y<FFMIN(h,block_w*(mb_y+1)); y++){
                for(x=0; x<w; x++){
                    buf[x + y*w]-= 128<<FRAC_BITS;
                }
            }
        }

        return;
    }

    for(mb_x=0; mb_x<=mb_w; mb_x++){
        add_yblock(s, 0, NULL, buf, dst8, obmc,
                   block_w*mb_x - block_w/2,
                   block_w*mb_y - block_w/2,
                   block_w, block_w,
                   w, h,
                   w, ref_stride, obmc_stride,
                   mb_x - 1, mb_y - 1,
                   add, 1, plane_index);
    }
}

static av_always_inline void predict_plane(SnowContext *s, IDWTELEM *buf, int plane_index, int add){
    const int mb_h= s->b_height << s->block_max_depth;
    int mb_y;
    for(mb_y=0; mb_y<=mb_h; mb_y++)
        predict_slice(s, buf, plane_index, add, mb_y);
}

static void dequantize_slice_buffered(SnowContext *s, slice_buffer * sb, SubBand *b, IDWTELEM *src, int stride, int start_y, int end_y){
    const int w= b->width;
    const int qlog= av_clip(s->qlog + b->qlog, 0, QROOT*16);
    const int qmul= qexp[qlog&(QROOT-1)]<<(qlog>>QSHIFT);
    const int qadd= (s->qbias*qmul)>>QBIAS_SHIFT;
    int x,y;

    if(s->qlog == LOSSLESS_QLOG) return;

    for(y=start_y; y<end_y; y++){
//        DWTELEM * line = slice_buffer_get_line_from_address(sb, src + (y * stride));
        IDWTELEM * line = slice_buffer_get_line(sb, (y * b->stride_line) + b->buf_y_offset) + b->buf_x_offset;
        for(x=0; x<w; x++){
            int i= line[x];
            if(i<0){
                line[x]= -((-i*qmul + qadd)>>(QEXPSHIFT)); //FIXME try different bias
            }else if(i>0){
                line[x]=  (( i*qmul + qadd)>>(QEXPSHIFT));
            }
        }
    }
}

static void correlate_slice_buffered(SnowContext *s, slice_buffer * sb, SubBand *b, IDWTELEM *src, int stride, int inverse, int use_median, int start_y, int end_y){
    const int w= b->width;
    int x,y;

    IDWTELEM * line=0; // silence silly "could be used without having been initialized" warning
    IDWTELEM * prev;

    if (start_y != 0)
        line = slice_buffer_get_line(sb, ((start_y - 1) * b->stride_line) + b->buf_y_offset) + b->buf_x_offset;

    for(y=start_y; y<end_y; y++){
        prev = line;
//        line = slice_buffer_get_line_from_address(sb, src + (y * stride));
        line = slice_buffer_get_line(sb, (y * b->stride_line) + b->buf_y_offset) + b->buf_x_offset;
        for(x=0; x<w; x++){
            if(x){
                if(use_median){
                    if(y && x+1<w) line[x] += mid_pred(line[x - 1], prev[x], prev[x + 1]);
                    else  line[x] += line[x - 1];
                }else{
                    if(y) line[x] += mid_pred(line[x - 1], prev[x], line[x - 1] + prev[x] - prev[x - 1]);
                    else  line[x] += line[x - 1];
                }
            }else{
                if(y) line[x] += prev[x];
            }
        }
    }
}

static void decode_qlogs(SnowContext *s){
    int plane_index, level, orientation;

    for(plane_index=0; plane_index<3; plane_index++){
        for(level=0; level<s->spatial_decomposition_count; level++){
            for(orientation=level ? 1:0; orientation<4; orientation++){
                int q;
                if     (plane_index==2) q= s->plane[1].band[level][orientation].qlog;
                else if(orientation==2) q= s->plane[plane_index].band[level][1].qlog;
                else                    q= get_symbol(&s->c, s->header_state, 1);
                s->plane[plane_index].band[level][orientation].qlog= q;
            }
        }
    }
}

#define GET_S(dst, check) \
    tmp= get_symbol(&s->c, s->header_state, 0);\
    if(!(check)){\
        av_log(s->avctx, AV_LOG_ERROR, "Error " #dst " is %d\n", tmp);\
        return -1;\
    }\
    dst= tmp;

static int decode_header(SnowContext *s){
    int plane_index, tmp;
    uint8_t kstate[32];

    memset(kstate, MID_STATE, sizeof(kstate));

    s->keyframe= get_rac(&s->c, kstate);
    if(s->keyframe || s->always_reset){
        reset_contexts(s);
        s->spatial_decomposition_type=
        s->qlog=
        s->qbias=
        s->mv_scale=
        s->block_max_depth= 0;
    }
    if(s->keyframe){
        GET_S(s->version, tmp <= 0U)
        s->always_reset= get_rac(&s->c, s->header_state);
        s->temporal_decomposition_type= get_symbol(&s->c, s->header_state, 0);
        s->temporal_decomposition_count= get_symbol(&s->c, s->header_state, 0);
        GET_S(s->spatial_decomposition_count, 0 < tmp && tmp <= MAX_DECOMPOSITIONS)
        s->colorspace_type= get_symbol(&s->c, s->header_state, 0);
        s->chroma_h_shift= get_symbol(&s->c, s->header_state, 0);
        s->chroma_v_shift= get_symbol(&s->c, s->header_state, 0);
        s->spatial_scalability= get_rac(&s->c, s->header_state);
//        s->rate_scalability= get_rac(&s->c, s->header_state);
        GET_S(s->max_ref_frames, tmp < (unsigned)MAX_REF_FRAMES)
        s->max_ref_frames++;

        decode_qlogs(s);
    }

    if(!s->keyframe){
        if(get_rac(&s->c, s->header_state)){
            for(plane_index=0; plane_index<2; plane_index++){
                int htaps, i, sum=0;
                Plane *p= &s->plane[plane_index];
                p->diag_mc= get_rac(&s->c, s->header_state);
                htaps= get_symbol(&s->c, s->header_state, 0)*2 + 2;
                if((unsigned)htaps > HTAPS_MAX || htaps==0)
                    return -1;
                p->htaps= htaps;
                for(i= htaps/2; i; i--){
                    p->hcoeff[i]= get_symbol(&s->c, s->header_state, 0) * (1-2*(i&1));
                    sum += p->hcoeff[i];
                }
                p->hcoeff[0]= 32-sum;
            }
            s->plane[2].diag_mc= s->plane[1].diag_mc;
            s->plane[2].htaps  = s->plane[1].htaps;
            memcpy(s->plane[2].hcoeff, s->plane[1].hcoeff, sizeof(s->plane[1].hcoeff));
        }
        if(get_rac(&s->c, s->header_state)){
            GET_S(s->spatial_decomposition_count, 0 < tmp && tmp <= MAX_DECOMPOSITIONS)
            decode_qlogs(s);
        }
    }

    s->spatial_decomposition_type+= get_symbol(&s->c, s->header_state, 1);
    if(s->spatial_decomposition_type > 1U){
        av_log(s->avctx, AV_LOG_ERROR, "spatial_decomposition_type %d not supported", s->spatial_decomposition_type);
        return -1;
    }
    if(FFMIN(s->avctx-> width>>s->chroma_h_shift,
             s->avctx->height>>s->chroma_v_shift) >> (s->spatial_decomposition_count-1) <= 0){
        av_log(s->avctx, AV_LOG_ERROR, "spatial_decomposition_count %d too large for size", s->spatial_decomposition_count);
        return -1;
    }

    s->qlog           += get_symbol(&s->c, s->header_state, 1);
    s->mv_scale       += get_symbol(&s->c, s->header_state, 1);
    s->qbias          += get_symbol(&s->c, s->header_state, 1);
    s->block_max_depth+= get_symbol(&s->c, s->header_state, 1);
    if(s->block_max_depth > 1 || s->block_max_depth < 0){
        av_log(s->avctx, AV_LOG_ERROR, "block_max_depth= %d is too large", s->block_max_depth);
        s->block_max_depth= 0;
        return -1;
    }

    return 0;
}

static void init_qexp(void){
    int i;
    double v=128;

    for(i=0; i<QROOT; i++){
        qexp[i]= lrintf(v);
        v *= pow(2, 1.0 / QROOT);
    }
}

static av_cold int common_init(AVCodecContext *avctx){
    SnowContext *s = avctx->priv_data;
    int width, height;
    int i, j;

    s->avctx= avctx;
    s->max_ref_frames=1; //just make sure its not an invalid value in case of no initial keyframe

    dsputil_init(&s->dsp, avctx);
    ff_dwt_init(&s->dwt);

#define mcf(dx,dy)\
    s->dsp.put_qpel_pixels_tab       [0][dy+dx/4]=\
    s->dsp.put_no_rnd_qpel_pixels_tab[0][dy+dx/4]=\
        s->dsp.put_h264_qpel_pixels_tab[0][dy+dx/4];\
    s->dsp.put_qpel_pixels_tab       [1][dy+dx/4]=\
    s->dsp.put_no_rnd_qpel_pixels_tab[1][dy+dx/4]=\
        s->dsp.put_h264_qpel_pixels_tab[1][dy+dx/4];

    mcf( 0, 0)
    mcf( 4, 0)
    mcf( 8, 0)
    mcf(12, 0)
    mcf( 0, 4)
    mcf( 4, 4)
    mcf( 8, 4)
    mcf(12, 4)
    mcf( 0, 8)
    mcf( 4, 8)
    mcf( 8, 8)
    mcf(12, 8)
    mcf( 0,12)
    mcf( 4,12)
    mcf( 8,12)
    mcf(12,12)

#define mcfh(dx,dy)\
    s->dsp.put_pixels_tab       [0][dy/4+dx/8]=\
    s->dsp.put_no_rnd_pixels_tab[0][dy/4+dx/8]=\
        mc_block_hpel ## dx ## dy ## 16;\
    s->dsp.put_pixels_tab       [1][dy/4+dx/8]=\
    s->dsp.put_no_rnd_pixels_tab[1][dy/4+dx/8]=\
        mc_block_hpel ## dx ## dy ## 8;

    mcfh(0, 0)
    mcfh(8, 0)
    mcfh(0, 8)
    mcfh(8, 8)

    if(!qexp[0])
        init_qexp();

//    dec += FFMAX(s->chroma_h_shift, s->chroma_v_shift);

    width= s->avctx->width;
    height= s->avctx->height;

    s->spatial_idwt_buffer= av_mallocz(width*height*sizeof(IDWTELEM));
    s->spatial_dwt_buffer= av_mallocz(width*height*sizeof(DWTELEM)); //FIXME this does not belong here

    for(i=0; i<MAX_REF_FRAMES; i++)
        for(j=0; j<MAX_REF_FRAMES; j++)
            scale_mv_ref[i][j] = 256*(i+1)/(j+1);

    s->avctx->get_buffer(s->avctx, &s->mconly_picture);
    s->scratchbuf = av_malloc(s->mconly_picture.linesize[0]*7*MB_SIZE);

    return 0;
}

static int common_init_after_header(AVCodecContext *avctx){
    SnowContext *s = avctx->priv_data;
    int plane_index, level, orientation;

    for(plane_index=0; plane_index<3; plane_index++){
        int w= s->avctx->width;
        int h= s->avctx->height;

        if(plane_index){
            w>>= s->chroma_h_shift;
            h>>= s->chroma_v_shift;
        }
        s->plane[plane_index].width = w;
        s->plane[plane_index].height= h;

        for(level=s->spatial_decomposition_count-1; level>=0; level--){
            for(orientation=level ? 1 : 0; orientation<4; orientation++){
                SubBand *b= &s->plane[plane_index].band[level][orientation];

                b->buf= s->spatial_dwt_buffer;
                b->level= level;
                b->stride= s->plane[plane_index].width << (s->spatial_decomposition_count - level);
                b->width = (w + !(orientation&1))>>1;
                b->height= (h + !(orientation>1))>>1;

                b->stride_line = 1 << (s->spatial_decomposition_count - level);
                b->buf_x_offset = 0;
                b->buf_y_offset = 0;

                if(orientation&1){
                    b->buf += (w+1)>>1;
                    b->buf_x_offset = (w+1)>>1;
                }
                if(orientation>1){
                    b->buf += b->stride>>1;
                    b->buf_y_offset = b->stride_line >> 1;
                }
                b->ibuf= s->spatial_idwt_buffer + (b->buf - s->spatial_dwt_buffer);

                if(level)
                    b->parent= &s->plane[plane_index].band[level-1][orientation];
                //FIXME avoid this realloc
                av_freep(&b->x_coeff);
                b->x_coeff=av_mallocz(((b->width+1) * b->height+1)*sizeof(x_and_coeff));
            }
            w= (w+1)>>1;
            h= (h+1)>>1;
        }
    }

    return 0;
}

#define QUANTIZE2 0

#if QUANTIZE2==1
#define Q2_STEP 8

static void find_sse(SnowContext *s, Plane *p, int *score, int score_stride, IDWTELEM *r0, IDWTELEM *r1, int level, int orientation){
    SubBand *b= &p->band[level][orientation];
    int x, y;
    int xo=0;
    int yo=0;
    int step= 1 << (s->spatial_decomposition_count - level);

    if(orientation&1)
        xo= step>>1;
    if(orientation&2)
        yo= step>>1;

    //FIXME bias for nonzero ?
    //FIXME optimize
    memset(score, 0, sizeof(*score)*score_stride*((p->height + Q2_STEP-1)/Q2_STEP));
    for(y=0; y<p->height; y++){
        for(x=0; x<p->width; x++){
            int sx= (x-xo + step/2) / step / Q2_STEP;
            int sy= (y-yo + step/2) / step / Q2_STEP;
            int v= r0[x + y*p->width] - r1[x + y*p->width];
            assert(sx>=0 && sy>=0 && sx < score_stride);
            v= ((v+8)>>4)<<4;
            score[sx + sy*score_stride] += v*v;
            assert(score[sx + sy*score_stride] >= 0);
        }
    }
}

static void dequantize_all(SnowContext *s, Plane *p, IDWTELEM *buffer, int width, int height){
    int level, orientation;

    for(level=0; level<s->spatial_decomposition_count; level++){
        for(orientation=level ? 1 : 0; orientation<4; orientation++){
            SubBand *b= &p->band[level][orientation];
            IDWTELEM *dst= buffer + (b->ibuf - s->spatial_idwt_buffer);

            dequantize(s, b, dst, b->stride);
        }
    }
}

static void dwt_quantize(SnowContext *s, Plane *p, DWTELEM *buffer, int width, int height, int stride, int type){
    int level, orientation, ys, xs, x, y, pass;
    IDWTELEM best_dequant[height * stride];
    IDWTELEM idwt2_buffer[height * stride];
    const int score_stride= (width + 10)/Q2_STEP;
    int best_score[(width + 10)/Q2_STEP * (height + 10)/Q2_STEP]; //FIXME size
    int score[(width + 10)/Q2_STEP * (height + 10)/Q2_STEP]; //FIXME size
    int threshold= (s->m.lambda * s->m.lambda) >> 6;

    //FIXME pass the copy cleanly ?

//    memcpy(dwt_buffer, buffer, height * stride * sizeof(DWTELEM));
    ff_spatial_dwt(buffer, width, height, stride, type, s->spatial_decomposition_count);

    for(level=0; level<s->spatial_decomposition_count; level++){
        for(orientation=level ? 1 : 0; orientation<4; orientation++){
            SubBand *b= &p->band[level][orientation];
            IDWTELEM *dst= best_dequant + (b->ibuf - s->spatial_idwt_buffer);
             DWTELEM *src=       buffer + (b-> buf - s->spatial_dwt_buffer);
            assert(src == b->buf); // code does not depend on this but it is true currently

            quantize(s, b, dst, src, b->stride, s->qbias);
        }
    }
    for(pass=0; pass<1; pass++){
        if(s->qbias == 0) //keyframe
            continue;
        for(level=0; level<s->spatial_decomposition_count; level++){
            for(orientation=level ? 1 : 0; orientation<4; orientation++){
                SubBand *b= &p->band[level][orientation];
                IDWTELEM *dst= idwt2_buffer + (b->ibuf - s->spatial_idwt_buffer);
                IDWTELEM *best_dst= best_dequant + (b->ibuf - s->spatial_idwt_buffer);

                for(ys= 0; ys<Q2_STEP; ys++){
                    for(xs= 0; xs<Q2_STEP; xs++){
                        memcpy(idwt2_buffer, best_dequant, height * stride * sizeof(IDWTELEM));
                        dequantize_all(s, p, idwt2_buffer, width, height);
                        ff_spatial_idwt(idwt2_buffer, width, height, stride, type, s->spatial_decomposition_count);
                        find_sse(s, p, best_score, score_stride, idwt2_buffer, s->spatial_idwt_buffer, level, orientation);
                        memcpy(idwt2_buffer, best_dequant, height * stride * sizeof(IDWTELEM));
                        for(y=ys; y<b->height; y+= Q2_STEP){
                            for(x=xs; x<b->width; x+= Q2_STEP){
                                if(dst[x + y*b->stride]<0) dst[x + y*b->stride]++;
                                if(dst[x + y*b->stride]>0) dst[x + y*b->stride]--;
                                //FIXME try more than just --
                            }
                        }
                        dequantize_all(s, p, idwt2_buffer, width, height);
                        ff_spatial_idwt(idwt2_buffer, width, height, stride, type, s->spatial_decomposition_count);
                        find_sse(s, p, score, score_stride, idwt2_buffer, s->spatial_idwt_buffer, level, orientation);
                        for(y=ys; y<b->height; y+= Q2_STEP){
                            for(x=xs; x<b->width; x+= Q2_STEP){
                                int score_idx= x/Q2_STEP + (y/Q2_STEP)*score_stride;
                                if(score[score_idx] <= best_score[score_idx] + threshold){
                                    best_score[score_idx]= score[score_idx];
                                    if(best_dst[x + y*b->stride]<0) best_dst[x + y*b->stride]++;
                                    if(best_dst[x + y*b->stride]>0) best_dst[x + y*b->stride]--;
                                    //FIXME copy instead
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    memcpy(s->spatial_idwt_buffer, best_dequant, height * stride * sizeof(IDWTELEM)); //FIXME work with that directly instead of copy at the end
}

#endif /* QUANTIZE2==1 */

#define USE_HALFPEL_PLANE 0

static void halfpel_interpol(SnowContext *s, uint8_t *halfpel[4][4], AVFrame *frame){
    int p,x,y;

    for(p=0; p<3; p++){
        int is_chroma= !!p;
        int w= s->avctx->width  >>is_chroma;
        int h= s->avctx->height >>is_chroma;
        int ls= frame->linesize[p];
        uint8_t *src= frame->data[p];

        halfpel[1][p]= (uint8_t*)av_malloc(ls * (h+2*EDGE_WIDTH)) + EDGE_WIDTH*(1+ls);
        halfpel[2][p]= (uint8_t*)av_malloc(ls * (h+2*EDGE_WIDTH)) + EDGE_WIDTH*(1+ls);
        halfpel[3][p]= (uint8_t*)av_malloc(ls * (h+2*EDGE_WIDTH)) + EDGE_WIDTH*(1+ls);

        halfpel[0][p]= src;
        for(y=0; y<h; y++){
            for(x=0; x<w; x++){
                int i= y*ls + x;

                halfpel[1][p][i]= (20*(src[i] + src[i+1]) - 5*(src[i-1] + src[i+2]) + (src[i-2] + src[i+3]) + 16 )>>5;
            }
        }
        for(y=0; y<h; y++){
            for(x=0; x<w; x++){
                int i= y*ls + x;

                halfpel[2][p][i]= (20*(src[i] + src[i+ls]) - 5*(src[i-ls] + src[i+2*ls]) + (src[i-2*ls] + src[i+3*ls]) + 16 )>>5;
            }
        }
        src= halfpel[1][p];
        for(y=0; y<h; y++){
            for(x=0; x<w; x++){
                int i= y*ls + x;

                halfpel[3][p][i]= (20*(src[i] + src[i+ls]) - 5*(src[i-ls] + src[i+2*ls]) + (src[i-2*ls] + src[i+3*ls]) + 16 )>>5;
            }
        }

//FIXME border!
    }
}

static void release_buffer(AVCodecContext *avctx){
    SnowContext *s = avctx->priv_data;
    int i;

    if(s->last_picture[s->max_ref_frames-1].data[0]){
        avctx->release_buffer(avctx, &s->last_picture[s->max_ref_frames-1]);
        for(i=0; i<9; i++)
            if(s->halfpel_plane[s->max_ref_frames-1][1+i/3][i%3])
                av_free(s->halfpel_plane[s->max_ref_frames-1][1+i/3][i%3] - EDGE_WIDTH*(1+s->current_picture.linesize[i%3]));
    }
}

static int frame_start(SnowContext *s){
   AVFrame tmp;
   int w= s->avctx->width; //FIXME round up to x16 ?
   int h= s->avctx->height;

    if (s->current_picture.data[0] && !(s->avctx->flags&CODEC_FLAG_EMU_EDGE)) {
        s->dsp.draw_edges(s->current_picture.data[0],
                          s->current_picture.linesize[0], w   , h   ,
                          EDGE_WIDTH  , EDGE_WIDTH  , EDGE_TOP | EDGE_BOTTOM);
        s->dsp.draw_edges(s->current_picture.data[1],
                          s->current_picture.linesize[1], w>>1, h>>1,
                          EDGE_WIDTH/2, EDGE_WIDTH/2, EDGE_TOP | EDGE_BOTTOM);
        s->dsp.draw_edges(s->current_picture.data[2],
                          s->current_picture.linesize[2], w>>1, h>>1,
                          EDGE_WIDTH/2, EDGE_WIDTH/2, EDGE_TOP | EDGE_BOTTOM);
    }

    release_buffer(s->avctx);

    tmp= s->last_picture[s->max_ref_frames-1];
    memmove(s->last_picture+1, s->last_picture, (s->max_ref_frames-1)*sizeof(AVFrame));
    memmove(s->halfpel_plane+1, s->halfpel_plane, (s->max_ref_frames-1)*sizeof(void*)*4*4);
    if(USE_HALFPEL_PLANE && s->current_picture.data[0])
        halfpel_interpol(s, s->halfpel_plane[0], &s->current_picture);
    s->last_picture[0]= s->current_picture;
    s->current_picture= tmp;

    if(s->keyframe){
        s->ref_frames= 0;
    }else{
        int i;
        for(i=0; i<s->max_ref_frames && s->last_picture[i].data[0]; i++)
            if(i && s->last_picture[i-1].key_frame)
                break;
        s->ref_frames= i;
        if(s->ref_frames==0){
            av_log(s->avctx,AV_LOG_ERROR, "No reference frames\n");
            return -1;
        }
    }

    s->current_picture.reference= 1;
    if(s->avctx->get_buffer(s->avctx, &s->current_picture) < 0){
        av_log(s->avctx, AV_LOG_ERROR, "get_buffer() failed\n");
        return -1;
    }

    s->current_picture.key_frame= s->keyframe;

    return 0;
}

static av_cold void common_end(SnowContext *s){
    int plane_index, level, orientation, i;

    av_freep(&s->spatial_dwt_buffer);
    av_freep(&s->spatial_idwt_buffer);

    s->m.me.temp= NULL;
    av_freep(&s->m.me.scratchpad);
    av_freep(&s->m.me.map);
    av_freep(&s->m.me.score_map);
    av_freep(&s->m.obmc_scratchpad);

    av_freep(&s->block);
    av_freep(&s->scratchbuf);

    for(i=0; i<MAX_REF_FRAMES; i++){
        av_freep(&s->ref_mvs[i]);
        av_freep(&s->ref_scores[i]);
        if(s->last_picture[i].data[0])
            s->avctx->release_buffer(s->avctx, &s->last_picture[i]);
    }

    for(plane_index=0; plane_index<3; plane_index++){
        for(level=s->spatial_decomposition_count-1; level>=0; level--){
            for(orientation=level ? 1 : 0; orientation<4; orientation++){
                SubBand *b= &s->plane[plane_index].band[level][orientation];

                av_freep(&b->x_coeff);
            }
        }
    }
    if (s->mconly_picture.data[0])
        s->avctx->release_buffer(s->avctx, &s->mconly_picture);
    if (s->current_picture.data[0])
        s->avctx->release_buffer(s->avctx, &s->current_picture);
}

static av_cold int decode_init(AVCodecContext *avctx)
{
    avctx->pix_fmt= PIX_FMT_YUV420P;

    common_init(avctx);

    return 0;
}

static int decode_frame(AVCodecContext *avctx, void *data, int *data_size, AVPacket *avpkt){
    const uint8_t *buf = avpkt->data;
    int buf_size = avpkt->size;
    SnowContext *s = avctx->priv_data;
    RangeCoder * const c= &s->c;
    int bytes_read;
    AVFrame *picture = data;
    int level, orientation, plane_index;

    ff_init_range_decoder(c, buf, buf_size);
    ff_build_rac_states(c, 0.05*(1LL<<32), 256-8);

    s->current_picture.pict_type= AV_PICTURE_TYPE_I; //FIXME I vs. P
    if(decode_header(s)<0)
        return -1;
    common_init_after_header(avctx);

    // realloc slice buffer for the case that spatial_decomposition_count changed
    ff_slice_buffer_destroy(&s->sb);
    ff_slice_buffer_init(&s->sb, s->plane[0].height, (MB_SIZE >> s->block_max_depth) + s->spatial_decomposition_count * 8 + 1, s->plane[0].width, s->spatial_idwt_buffer);

    for(plane_index=0; plane_index<3; plane_index++){
        Plane *p= &s->plane[plane_index];
        p->fast_mc= p->diag_mc && p->htaps==6 && p->hcoeff[0]==40
                                              && p->hcoeff[1]==-10
                                              && p->hcoeff[2]==2;
    }

    alloc_blocks(s);

    if(frame_start(s) < 0)
        return -1;
    //keyframe flag duplication mess FIXME
    if(avctx->debug&FF_DEBUG_PICT_INFO)
        av_log(avctx, AV_LOG_ERROR, "keyframe:%d qlog:%d\n", s->keyframe, s->qlog);

    decode_blocks(s);

    for(plane_index=0; plane_index<3; plane_index++){
        Plane *p= &s->plane[plane_index];
        int w= p->width;
        int h= p->height;
        int x, y;
        int decode_state[MAX_DECOMPOSITIONS][4][1]; /* Stored state info for unpack_coeffs. 1 variable per instance. */

        if(s->avctx->debug&2048){
            memset(s->spatial_dwt_buffer, 0, sizeof(DWTELEM)*w*h);
            predict_plane(s, s->spatial_idwt_buffer, plane_index, 1);

            for(y=0; y<h; y++){
                for(x=0; x<w; x++){
                    int v= s->current_picture.data[plane_index][y*s->current_picture.linesize[plane_index] + x];
                    s->mconly_picture.data[plane_index][y*s->mconly_picture.linesize[plane_index] + x]= v;
                }
            }
        }

        {
        for(level=0; level<s->spatial_decomposition_count; level++){
            for(orientation=level ? 1 : 0; orientation<4; orientation++){
                SubBand *b= &p->band[level][orientation];
                unpack_coeffs(s, b, b->parent, orientation);
            }
        }
        }

        {
        const int mb_h= s->b_height << s->block_max_depth;
        const int block_size = MB_SIZE >> s->block_max_depth;
        const int block_w    = plane_index ? block_size/2 : block_size;
        int mb_y;
        DWTCompose cs[MAX_DECOMPOSITIONS];
        int yd=0, yq=0;
        int y;
        int end_y;

        ff_spatial_idwt_buffered_init(cs, &s->sb, w, h, 1, s->spatial_decomposition_type, s->spatial_decomposition_count);
        for(mb_y=0; mb_y<=mb_h; mb_y++){

            int slice_starty = block_w*mb_y;
            int slice_h = block_w*(mb_y+1);
            if (!(s->keyframe || s->avctx->debug&512)){
                slice_starty = FFMAX(0, slice_starty - (block_w >> 1));
                slice_h -= (block_w >> 1);
            }

            for(level=0; level<s->spatial_decomposition_count; level++){
                for(orientation=level ? 1 : 0; orientation<4; orientation++){
                    SubBand *b= &p->band[level][orientation];
                    int start_y;
                    int end_y;
                    int our_mb_start = mb_y;
                    int our_mb_end = (mb_y + 1);
                    const int extra= 3;
                    start_y = (mb_y ? ((block_w * our_mb_start) >> (s->spatial_decomposition_count - level)) + s->spatial_decomposition_count - level + extra: 0);
                    end_y = (((block_w * our_mb_end) >> (s->spatial_decomposition_count - level)) + s->spatial_decomposition_count - level + extra);
                    if (!(s->keyframe || s->avctx->debug&512)){
                        start_y = FFMAX(0, start_y - (block_w >> (1+s->spatial_decomposition_count - level)));
                        end_y = FFMAX(0, end_y - (block_w >> (1+s->spatial_decomposition_count - level)));
                    }
                    start_y = FFMIN(b->height, start_y);
                    end_y = FFMIN(b->height, end_y);

                    if (start_y != end_y){
                        if (orientation == 0){
                            SubBand * correlate_band = &p->band[0][0];
                            int correlate_end_y = FFMIN(b->height, end_y + 1);
                            int correlate_start_y = FFMIN(b->height, (start_y ? start_y + 1 : 0));
                            decode_subband_slice_buffered(s, correlate_band, &s->sb, correlate_start_y, correlate_end_y, decode_state[0][0]);
                            correlate_slice_buffered(s, &s->sb, correlate_band, correlate_band->ibuf, correlate_band->stride, 1, 0, correlate_start_y, correlate_end_y);
                            dequantize_slice_buffered(s, &s->sb, correlate_band, correlate_band->ibuf, correlate_band->stride, start_y, end_y);
                        }
                        else
                            decode_subband_slice_buffered(s, b, &s->sb, start_y, end_y, decode_state[level][orientation]);
                    }
                }
            }

            for(; yd<slice_h; yd+=4){
                ff_spatial_idwt_buffered_slice(&s->dwt, cs, &s->sb, w, h, 1, s->spatial_decomposition_type, s->spatial_decomposition_count, yd);
            }

            if(s->qlog == LOSSLESS_QLOG){
                for(; yq<slice_h && yq<h; yq++){
                    IDWTELEM * line = slice_buffer_get_line(&s->sb, yq);
                    for(x=0; x<w; x++){
                        line[x] <<= FRAC_BITS;
                    }
                }
            }

            predict_slice_buffered(s, &s->sb, s->spatial_idwt_buffer, plane_index, 1, mb_y);

            y = FFMIN(p->height, slice_starty);
            end_y = FFMIN(p->height, slice_h);
            while(y < end_y)
                ff_slice_buffer_release(&s->sb, y++);
        }

        ff_slice_buffer_flush(&s->sb);
        }

    }

    emms_c();

    release_buffer(avctx);

    if(!(s->avctx->debug&2048))
        *picture= s->current_picture;
    else
        *picture= s->mconly_picture;

    *data_size = sizeof(AVFrame);

    bytes_read= c->bytestream - c->bytestream_start;
    if(bytes_read ==0) av_log(s->avctx, AV_LOG_ERROR, "error at end of frame\n"); //FIXME

    return bytes_read;
}

static av_cold int decode_end(AVCodecContext *avctx)
{
    SnowContext *s = avctx->priv_data;

    ff_slice_buffer_destroy(&s->sb);

    common_end(s);

    return 0;
}

AVCodec ff_snow_decoder = {
    .name           = "snow",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = CODEC_ID_SNOW,
    .priv_data_size = sizeof(SnowContext),
    .init           = decode_init,
    .close          = decode_end,
    .decode         = decode_frame,
    .capabilities   = CODEC_CAP_DR1 /*| CODEC_CAP_DRAW_HORIZ_BAND*/,
    .long_name = NULL_IF_CONFIG_SMALL("Snow"),
};
