/*
 * Copyright (c) 2012 Luca Barbato
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

/**
 * @file
 * AVHash related functions.
 */

#include "hash.h"
#include "config.h"

/**
 * Wipe the internal state and reset it, optional parameters can be set
 * using the options dictionary.
 */
int av_hash_init(AVHashContext *ctx, AVDictionary **options);

/**
 * Feed the hasher with new data
 */
void av_hash_update(AVHashContext *ctx, const uint8_t *src, const int len);
/**
 * Return the hash value and reset the state.
 */
void av_hash_finalize(AVHashContext *ctx, uint8_t *dst);

/**
 * Destroy the context
 */
void av_hash_free(AVHashContext **ctx);
