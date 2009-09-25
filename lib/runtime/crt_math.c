/*===-- runtime/crt_math.c ------------------------------------------------===
 *
 * This file is distributed under the MIT license. See LICENSE.txt for details.
 *
 * Copyright (C) 2009, Stephen Wilson
 *
 *===----------------------------------------------------------------------===*/

/*
 * This file holds portable C versions of the runtime math routines.
 */
#include "comma/runtime/commart.h"

#include <stdint.h>

int32_t _comma_pow_i32_i32(int32_t x, int32_t n)
{
    int32_t res = x;

    if (n < 0)
        _comma_raise_exception("Negative exponent!");

    if (n == 0)
        return 0;

    while (--n)
        res *= x;

    return res;
}

int64_t _comma_pow_i64_i32(int64_t x, int32_t n)
{
    int64_t res = x;

    if (n < 0)
        _comma_raise_exception("Negative exponent!");

    if (n == 0)
        return 0;

    while (--n)
        res *= x;

    return res;
}
