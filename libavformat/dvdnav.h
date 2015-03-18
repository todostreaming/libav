/*
 * DVD NAV Packet functions
 * Copyright (c) 2015 Luca Barbato
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

#ifndef AVFORMAT_DVDNAV_H
#define AVFORMAT_DVDNAV_H

#include "libavcodec/bytestream.h"

#define NAVPCI_SIZE 980
#define NAVDSI_SIZE 1018

static void ff_print_navpci(const char *label, uint8_t *ps2buf)
{
    GetByteContext gb;
    int nv_pck_lbn, vobu_cat, zero1, uops;
    int hour, minute, second, frame;

    bytestream2_init(&gb, ps2buf, 980);

    nv_pck_lbn = bytestream2_get_be32(&gb);
    vobu_cat   = bytestream2_get_be16(&gb);
    zero1      = bytestream2_get_be16(&gb);

    uops       = bytestream2_get_be32(&gb);

    hour       = bytestream2_get_byte(&gb);
    minute     = bytestream2_get_byte(&gb);
    second     = bytestream2_get_byte(&gb);
    frame      = bytestream2_get_byte(&gb);

    av_log(NULL, AV_LOG_VERBOSE, "%s: pkt_lbn 0x%08x %d:%d:%d.%d\n",
           label, nv_pck_lbn, hour, minute, second, frame);
}

static void ff_print_navdsi(const char *label, uint8_t *ps2buf)

{
    GetByteContext gb;
    int nv_pck_scr, nv_pck_lbn, vobu_ea;
    int vobu_1stref_ea, vobu_2stref_ea, vobu_3stref_ea;
    int vobu_vob_idn, zero1, vobu_c_idn;
    int hour, minute, second, frame;

    bytestream2_init(&gb, ps2buf, 1018);

    nv_pck_scr  = bytestream2_get_be32(&gb);
    nv_pck_lbn  = bytestream2_get_be32(&gb);
    vobu_ea     = bytestream2_get_be32(&gb);

    vobu_1stref_ea = bytestream2_get_be32(&gb);
    vobu_2stref_ea = bytestream2_get_be32(&gb);
    vobu_3stref_ea = bytestream2_get_be32(&gb);

    vobu_vob_idn = bytestream2_get_be16(&gb);
    zero1        = bytestream2_get_byte(&gb);
    vobu_c_idn   = bytestream2_get_byte(&gb);

    hour       = bytestream2_get_byte(&gb);
    minute     = bytestream2_get_byte(&gb);
    second     = bytestream2_get_byte(&gb);
    frame      = bytestream2_get_byte(&gb);


    av_log(NULL, AV_LOG_VERBOSE,
           "%s: nv_pck 0x%08x/0x%08x/0x%08x vob %d cell %d "
           " %d:%d:%d.%d\n",
           label,
           nv_pck_scr, nv_pck_scr, vobu_ea,
           vobu_vob_idn, vobu_c_idn,
           hour, minute, second, frame);
}

#endif /* AVFORMAT_MPEG_H */

