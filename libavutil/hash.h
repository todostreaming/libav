/*
 * copyright (c) 2012 Luca Barbato
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

#ifndef AVUTIL_HASH_H
#define AVUTIL_HASH_H

#include <stdint.h>

#include "version.h"
#include "dict.h"

/**
 * @defgroup lavu_hash AVHash
 * @ingroup lavu_crypto
 * @{
 */

typedef struct AVHashContext {
    const AVClass *av_class;
    const struct AVHash *hash;
    int digest_length;
    void *context;
    void *opaque;
}

typedef struct AVHash {
    const AVClass *av_class;
    const char *name;

    int  (*init)  (AVHashContext *ctx);
    void (*update)(AVHashContext *ctx, const uint8_t *src, const int len);
    void (*close) (AVHashContext *ctx, uint8_t *dst);

    int   context_size;
    struct AVHash *next;
} AVHash;


/**
 * Iterate over the available hash.
 */
AVHash *av_hash_next(AVHash *hash)


/**
 * Given a name describing the hash return an yet to be
 * initialized context.
 */
int av_hash_lookup(AVHashContext **hash, const char *name);

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


/**
 * Shortcut for the previous two functions
 */
void av_hash_sum(AVHashContext *ctx, uint8_t *dst, const uint8_t *src,
                 const int len);


/**
 * @}
 */

#endif /* AVUTIL_HASH_H */
