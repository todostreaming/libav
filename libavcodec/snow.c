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

#undef NDEBUG
#include <assert.h>


void ff_snow_inner_add_yblock(const uint8_t *obmc, const int obmc_stride, uint8_t * * block, int b_w, int b_h,
                              int src_x, int src_y, int src_stride, slice_buffer * sb, int add, uint8_t * dst8){
    int y, x;
    IDWTELEM * dst;
    for(y=0; y<b_h; y++){
        //FIXME ugly misuse of obmc_stride
        const uint8_t *obmc1= obmc + y*obmc_stride;
        const uint8_t *obmc2= obmc1+ (obmc_stride>>1);
        const uint8_t *obmc3= obmc1+ obmc_stride*(obmc_stride>>1);
        const uint8_t *obmc4= obmc3+ (obmc_stride>>1);
        dst = slice_buffer_get_line(sb, src_y + y);
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
                v += dst[x + src_x];
                v = (v + (1<<(FRAC_BITS-1))) >> FRAC_BITS;
                if(v&(~255)) v= ~(v>>31);
                dst8[x + y*src_stride] = v;
            }else{
                dst[x + src_x] -= v;
            }
        }
    }
}
