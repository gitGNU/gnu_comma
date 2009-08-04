/*===-- runtime/crt_types.c -----------------------------------------------===
 *
 * This file is distributed under the MIT license. See LICENSE.txt for details.
 *
 * Copyright (C) 2009, Stephen Wilson
 *
 *===----------------------------------------------------------------------===*/

#include "comma/runtime/crt_types.h"
#include "comma/runtime/crt_itable.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static bool lookup_instance(domain_info_t info,
                            domain_view_t *args, domain_instance_t *instance)
{
        if (!info->instance_table)
                info->instance_table = alloc_itable();

        return itable_lookup(info->instance_table, info, args, instance);
}

/*
 * Get a domain instance.
 */
domain_instance_t _comma_get_domain(domain_info_t info, ...)
{
        domain_instance_t instance;
        unsigned i;
        domain_view_t *args;

        if (info->arity) {
                args = malloc(sizeof(domain_view_t)*info->arity);
                for (i = 0; i < info->arity; ++i) {
                        va_list ap;
                        va_start(ap, info);
                        args[i] = va_arg(ap, domain_view_t);
                }
        }
        else
                args = 0;

        /*
         * Lookup the instance in a hash table and return it if found.
         * Otherwise, this function updates instance to point at an entry which
         * we can fill in (the instance was allocated using
         * alloc_domain_instance, and therefore has the capacity to hold all of
         * the items written to the structure).
         */
        if (lookup_instance(info, args, &instance))
                return instance;

        instance->info = info;
        instance->params = args;

        /*
         * Create all of the views of this instance.
         */
        for (i = 0; i < info->num_signatures; ++i) {
                ptrdiff_t offset = info->sig_offsets[i];
                instance->views[i].instance = instance;
                instance->views[i].index    = i;
        }

        /*
         * If the constructor is non-null, call it.
         */
        if (info->ctor != 0)
                info->ctor(instance);

        return instance;
}

/*
 * Allocates a domain instance which is large enough to hold all of the data
 * required by the given info.
 */
domain_instance_t alloc_domain_instance(domain_info_t info)
{
        domain_instance_t instance;
        instance         = malloc(sizeof(struct domain_instance));
        instance->views  = malloc(sizeof(struct domain_view)
                                  * info->num_signatures);
        return instance;
}
