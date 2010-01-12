/*===-- runtime/crt_alloc.c -----------------------------------------------===
 *
 * This file is distributed under the MIT license. See LICENSE.txt for details.
 *
 * Copyright (C) 2010, Stephen Wilson
 *
 *===----------------------------------------------------------------------===*/

/*
 * This file defines the interface to Comma's memory management routines.
 */
#include "comma/runtime/commart.h"

#include <stdlib.h>

void *_comma_alloc(size_t bytes, uint32_t align)
{
    // Ingnore the alignment.
    return malloc(bytes);
}

