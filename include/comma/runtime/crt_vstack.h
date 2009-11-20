/*===-- runtime/crt_vstack.h ----------------------------------------------===
 *
 * This file is distributed under the MIT license. See LICENSE.txt for details.
 *
 * Copyright (C) 2009, Stephen Wilson
 *
 *===----------------------------------------------------------------------===*/

#ifndef COMMA_RUNTIME_CRT_VSTACK_HDR_GUARD
#define COMMA_RUNTIME_CRT_VSTACK_HDR_GUARD

/**
 * \file
 *
 * \brief Defines routines to access the "vstack", or variable stack.
 *
 * Comma's vstack is a runtime stack used to return objects of variable length
 * from Comma subroutines.  The canonical example is returning a String from a
 * function:
 *
 * \code
 *   function Get_String return String is
 *   begin
 *      return "Hello, world!";
 *   end Get_String;
 * \endcode
 *
 * The compiler generates two calls to _comma_vstack_push() in this case,
 * populating the vstack with a bounds structure and the actual data comprising
 * the string.  Callers of \c Get_String retrieve the data by inspecting the
 * _comma_vstack variable, which is a pointer to the most recently pushed data
 * on the stack.  Once the data has been retrieved (e.g. copied),
 * _comma_vstack_push() is called to remove the top-most item from the stack.
 */

#include <inttypes.h>

/**
 * Pointer to the top of the vstack.
 */
char *_comma_vstack;

/**
 * Allocates a region of \p size bytes on the vstack.
 *
 * The allocated data is available thru the _comma_vstack pointer, and will be
 * disposed of when a matching call to _comma_vstack_pop() is made.
 */
void _comma_vstack_alloc(int32_t size);

/**
 * Copy's \p size bytes from \p data onto the vstack.
 *
 * The allocated data is available thru the _comma_vstack pointer, and will be
 * disposed of when a matching call to _comma_vstack_pop() is made.
 */
void _comma_vstack_push(void *data, int32_t size);

/**
 * Pops the vstack and resets _comma_vstack to the next item.
 */
void _comma_vstack_pop();

#endif
