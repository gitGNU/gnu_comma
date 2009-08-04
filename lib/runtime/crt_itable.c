/*===-- runtime/crt_itable.c ----------------------------------------------===
 *
 * This file is distributed under the MIT license. See LICENSE.txt for details.
 *
 * Copyright (C) 2009, Stephen Wilson
 *
 *===----------------------------------------------------------------------===*/

#include "comma/runtime/crt_itable.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define FNV_PRIME           16777619
#define FNV_OFFSET          2166136261
#define LOAD_FACTOR         0.75
#define DEFAULT_BUCKET_SIZE 5
#define MAX_BUCKET_SIZE     16

uint32_t
hash_parameters_fnv(domain_view_t *params, unsigned len)
{
        uint32_t hval = FNV_OFFSET;
        unsigned char *cursor = (unsigned char *)params;
        unsigned char *end    = cursor + sizeof(domain_view_t) * len;

        while (cursor < end) {
                hval *= FNV_PRIME;
                hval ^= (uint32_t)*cursor++;
        }
        return hval;
}

uint32_t
hash_params(domain_view_t *params, unsigned len, unsigned modulus)
{
        uint32_t hash  = hash_parameters_fnv(params, len);
        uint32_t mask  = (1 << modulus) - 1;

        return ((hash >> modulus) ^ hash) & mask;
}

static inline bool hash_resize_p(struct itable *htab)
{
        double lf = ((double)htab->num_entries /
                     (double)(1 << htab->bucket_size));

        return (lf > LOAD_FACTOR && htab->bucket_size < MAX_BUCKET_SIZE);
}

void hash_resize(struct itable *htab, unsigned key_len)
{
        unsigned           old_size   = 1 << htab->bucket_size;
        unsigned           new_size   = old_size * 2;
        domain_instance_t *old_bucket = htab->bucket;
        domain_instance_t *new_bucket = calloc(new_size, sizeof(domain_instance_t));

        domain_instance_t cursor;
        domain_instance_t instance;
        uint32_t          index;
        unsigned          i;

        htab->bucket_size++;
        for (i = 0; i < old_size; ++i) {
                cursor = old_bucket[i];
                while (cursor) {
                        index = hash_params(cursor->params, key_len, htab->bucket_size);
                        instance = cursor;
                        cursor = instance->next;
                        instance->next = new_bucket[index];
                        new_bucket[index] = instance;
                }
        }
        free(old_bucket);
        htab->bucket = new_bucket;
}

bool itable_lookup(struct itable     *htab,
                   domain_info_t      info,
                   domain_view_t     *key,
                   domain_instance_t *instance)
{
        domain_instance_t res;
        unsigned key_len = info->arity;
        uint32_t index   = hash_params(key, key_len, htab->bucket_size);

        if ((res = htab->bucket[index])) {
                /*
                 * We found an entry.  Check each element of the chain for a
                 * match.
                 */
                do {
                        domain_view_t *params = res->params;
                        if (!memcmp(params, key, sizeof(domain_view_t)*key_len))
                                return (*instance = res);
                } while ((res = res->next));
        }

        /*
         * Check if a resize is needed.  Note that we do this before the
         * insertion of our instance since the protocal is to have the caller
         * initialize the instance.
         */
        htab->num_entries++;
        if (hash_resize_p(htab))
                hash_resize(htab, key_len);

        index = hash_params(key, key_len, htab->bucket_size);
        res = alloc_domain_instance(info);
        res->next = htab->bucket[index];
        htab->bucket[index] = res;

        *instance = res;
        return false;
}

struct itable *alloc_itable()
{
        struct itable *res = malloc(sizeof(struct itable));

        res->num_entries = 0;
        res->bucket_size = DEFAULT_BUCKET_SIZE;
        res->bucket = calloc(1 << DEFAULT_BUCKET_SIZE, sizeof(domain_instance_t));

        return res;
}

