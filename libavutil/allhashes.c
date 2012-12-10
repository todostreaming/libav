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
 * Provide registration of all hashes for libavutil.
 */

#include "hash.h"
#include "error.h"
#include "config.h"

#define REGISTER_HASH(X,x) { \
          extern AVHash ff_##x##_hash; \
          if (CONFIG_##X##_HASH)  avhash_register(&ff_##x##_hash); }

static AVHash *first_avhash = NULL;

void av_hash_register(AVHash *hash)
{
    AVHash **p;
    p = &first_avhash;
    while (*p != NULL)
        p = &(*p)->next;
    *p = hash;
    hash->next = NULL;
}

AVHash *av_hash_next(AVHash *hash)
{
    if (hash)
        return hash->next;
    else
        return first_avhash;
}

int av_hash_lookup(AVHashContext **hash, const char *name)
{
    AVHash *p = NULL;
    AVHashContext *hash = av_mallocz(sizeof(*hash));

    if (!hash)
        return AVERROR(ENOMEM);
    while ((p = av_hash_next(p))) {
        if (!av_strncasecmp(p->name, name))
            break;
    }

    if (!p) {
        av_free(hash);
        return AVERROR_HASH_NOT_FOUND;
    }

    hash->hash = p;
    return 0;
}

void av_hash_register_all(void)
{
    static int initialized;

    if (initialized)
        return;
    initialized = 1;

    REGISTER_HASH(MD5, md5)
    REGISTER_HASH(SHA, sha)
    REGISTER_HASH(CRC, crc)
}
