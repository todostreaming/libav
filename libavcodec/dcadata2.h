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

#ifndef AVCODEC_DCADATA2_H
#define AVCODEC_DCADATA2_H

#include <stdint.h>

extern const uint8_t ff_dca2_quant_index_sel_nbits[10];
extern const uint8_t ff_dca2_block_code_nbits[7];
extern const uint8_t ff_dca2_dmix_primary_nch[7];
extern const int32_t ff_dca2_quant_levels[32];
extern const int32_t ff_dca2_scale_factor_adj[4];
extern const int32_t ff_dca2_joint_scale_factors[129];
extern const int32_t ff_dca2_band_fir_perfect_fixed[512];
extern const int32_t ff_dca2_band_fir_nonperfect_fixed[512];
extern const int32_t ff_dca2_lfe_fir_64_fixed[256];
extern const int32_t ff_dca2_band_fir_x96_fixed[1024];
extern const uint16_t ff_dca2_xll_refl_coeff[128];

#endif
