/*===-- runtime/commart.h -------------------------------------------------===
 *
 * This file is distributed under the MIT license. See LICENSE.txt for details.
 *
 * Copyright (C) 2009, Stephen Wilson
 *
 *===----------------------------------------------------------------------===*/

#ifndef COMMA_RUNTIME_COMMART_HDR_GUARD
#define COMMA_RUNTIME_COMMART_HDR_GUARD

/*
 * This file provides the public interface to the Comma runtime library.
 */

/*
 * Allocates and raises a system exception with the given message.
 */
void _comma_raise_exception(const char *message);

#endif
