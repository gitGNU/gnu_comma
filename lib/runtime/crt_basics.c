/*===-- runtime/crt_basics.c -----------------------------------------------===
 *
 * This file is distributed under the MIT license. See LICENSE.txt for details.
 *
 * Copyright (C) 2009, Stephen Wilson
 *
 *===----------------------------------------------------------------------===*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 *  This file defines fundamental Comma runtime functions.
 */

/*
 * _comma_assert_fail :  Used to implement "pragma Assert".
 *
 * Prints the given message to stderr then calls abort() to terminate the
 * process.
 *
 * TODO: Scan the message for a terminating new line, possibly followed by white
 * space, and emit a newline ourselves if one is not found.
 */
void _comma_assert_fail(const char *msg)
{
        fputs(msg, stderr);
        abort();
}

