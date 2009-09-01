/*===-- runtime/crt_itable.h ----------------------------------------------===
 *
 * This file is distributed under the MIT license. See LICENSE.txt for details.
 *
 * Copyright (C) 2009, Stephen Wilson
 *
 *===----------------------------------------------------------------------===*/

/*
 * The following table is used to manage the creation of domain instances.
 */

#ifndef COMMA_RUNTIME_CRT_ITABEL_HDR_GUARD
#define COMMA_RUNTIME_CRT_ITABEL_HDR_GUARD

#include "comma/runtime/crt_types.h"

#include <stdbool.h>

struct itable {
        /*
         * Number of entries in this table.
         */
        unsigned num_entries;

        /*
         * Exponent on 2 yielding the size of the bucket.
         */
        unsigned bucket_size;

        /*
         * An array of instances with bucket_size elements.
         */
        domain_instance_t *bucket;
};

struct itable *alloc_itable();

bool itable_lookup(struct itable     *htab,
                   domain_info_t      info,
                   domain_instance_t *key,
                   domain_instance_t *instance);

#endif
