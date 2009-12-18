/*===-- runtime/commart.h -------------------------------------------------===
 *
 * This file is distributed under the MIT license. See LICENSE.txt for details.
 *
 * Copyright (C) 2009, Stephen Wilson
 *
 *===----------------------------------------------------------------------===*/

#ifndef COMMA_RUNTIME_COMMART_HDR_GUARD
#define COMMA_RUNTIME_COMMART_HDR_GUARD

#include <stdint.h>

/*
 * This file provides the public interface to the Comma runtime library.
 */

/*
 * Opaque type representing a Comma exeception object.
 */
typedef char *comma_exinfo_t;

/*
 * The following enumeration defines the set of standard system-level exceptions
 * which the runtime might raise.
 */
typedef enum {
    COMMA_CONSTRAINT_ERROR_E,
    COMMA_PROGRAM_ERROR_E
} comma_exception_id;

/*
 * Returns the unique exception object corresponding to the given id.
 */
comma_exinfo_t _comma_get_exception(comma_exception_id id);

/*
 * Allocates and raises an expection with an identity defined by the provided
 * info and associates it with the given message.  The message may be null.
 */
void _comma_raise_exception(comma_exinfo_t info, const char *message);

/*
 * Allocates and raises an expection with an identity defined by the provided
 * info and associates it with the given message.  The message may be a null
 * pointer or a string of the given length.
 */
void _comma_raise_nexception(comma_exinfo_t info, const char *message,
                             int32_t length);

/*
 * Routine to raise a specific system exception.
 */
void _comma_raise_system(uint32_t id, const char *message);


#endif
