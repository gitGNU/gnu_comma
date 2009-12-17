/*===-- runtime/crt_exceptions.c --------------------------------------------===
 *
 * This file is distributed under the MIT license. See LICENSE.txt for details.
 *
 * Copyright (C) 2009, Stephen Wilson
 *
 *===----------------------------------------------------------------------===*/

/*
 * This file defines Comma's exception handling primitives.
 */
#include "comma/runtime/commart.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>


/*
 * Forward declarations.
 */
struct _Unwind_Exception;
struct _Unwind_Context;

/*
 * Reason codes used to communicate the outcomes of several operations.
 */
typedef enum {
    _URC_NO_REASON = 0,
    _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
    _URC_FATAL_PHASE2_ERROR = 2,
    _URC_FATAL_PHASE1_ERROR = 3,
    _URC_NORMAL_STOP = 4,
    _URC_END_OF_STACK = 5,
    _URC_HANDLER_FOUND = 6,
    _URC_INSTALL_CONTEXT = 7,
    _URC_CONTINUE_UNWIND = 8
} _Unwind_Reason_Code;

/*
 * The type of an exception cleanup function.  This is used, for example, when
 * an exception handler needs to free a forgien exception object.
 */
typedef void (*_Unwind_Exception_Cleanup_Fn) (_Unwind_Reason_Code reason,
                                              struct _Unwind_Exception *exc);

/*
 * The system routine which raises an exception.
 */
extern _Unwind_Reason_Code
_Unwind_RaiseException(struct _Unwind_Exception *exception_object);

/*
 * Accessor the the LSDA.
 */
extern uintptr_t
_Unwind_GetLanguageSpecificData(struct _Unwind_Context *context);

/*
 * Accessor to the start of the handler/cleanup code.
 */
extern uintptr_t
_Unwind_GetRegionStart(struct _Unwind_Context *context);

/*
 * Accessor to the instruction pointer.  This pointer is one past the
 * instruction which actually resulted in the raising of an exception.
 */
extern uintptr_t _Unwind_GetIP(struct _Unwind_Context *context);

/*
 * Sets the value of the given general purpose registe.
 */
extern void _Unwind_SetGR(struct _Unwind_Context *context,
                          int reg, uintptr_t value);

/*
 * Sets the instruction pointer to the given value.
 */
extern void _Unwind_SetIP(struct _Unwind_Context *context, uintptr_t IP);

/*
 * The exception header structure which all exception objects must contain.
 */
struct _Unwind_Exception {
    uint64_t                     exception_class;
    _Unwind_Exception_Cleanup_Fn exception_cleanup;
    uint64_t                     private_1;
    uint64_t                     private_2;
};

/*
 * The action argument to the personality routine, and the various action codes.
 */
typedef int _Unwind_Action;
static const _Unwind_Action _UA_SEARCH_PHASE  = 1;
static const _Unwind_Action _UA_CLEANUP_PHASE = 2;
static const _Unwind_Action _UA_HANDLER_FRAME = 4;
static const _Unwind_Action _UA_FORCE_UNWIND  = 8;

/*===----------------------------------------------------------------------===
 * DWARF value parsing.
 *===----------------------------------------------------------------------===*/

/*
 * The following constants are used in the encoding of the DWARF exception
 * headers.  Such an encoding is represented as a single byte.  The lower four
 * bits describes the data format of a DWARF value.  These lower bits are
 * itemized in the Dwaf_Encoding enumeration.  The upper four bits describes how
 * the value is to be interpreted, and are itemized in Dwarf_Application. These
 * values are defined in the LSB-3.0.0 (see http://refspecs.freestandards.org).
 *
 * Apparently GCC defines other encodings not mentioned in the LSB (for example,
 * DW_EH_PE_signed) but they are not needed for our purposes.
 */
typedef enum {
    DW_EH_PE_omit    = 0xFF, /* No value is present. */
    DW_EH_PE_uleb128 = 0x01, /* Unsigned LEB128 encoded value. */
    DW_EH_PE_udata2  = 0x02, /* 2 byte unsigned data. */
    DW_EH_PE_udata4  = 0x03, /* 4 byte unsigned data. */
    DW_EH_PE_udata8  = 0x04, /* 8 byte unsigned data. */
    DW_EH_PE_sleb128 = 0x09, /* Signed  LEB128 encoded value. */
    DW_EH_PE_sdata2  = 0x0A, /* 2 byte signed data. */
    DW_EH_PE_sdata4  = 0x0B, /* 4 byte signed data. */
    DW_EH_PE_sdata8  = 0x0C, /* 8 byte signed data. */
} Dwarf_Encoding;

typedef enum {
    DW_EH_PE_absptr  = 0x00, /* Value is used without modification. */
    DW_EH_PE_pcrel   = 0x10, /* Value relative to program counter. */
    DW_EH_PE_datarel = 0x30, /* Value relative to .eh_frame_hdr. */
} Dwarf_Application;

/*
 * The following two subroutines parse signed and unsigned Little-Endian Base
 * 128 (LEB128) encoded values.  This is a compression technique used in DWARF
 * for encoding integers.
 *
 * LEB128 is variable length data.  The following subroutines take two
 * parameters:
 *
 *   - start : a pointer to the start of the data.
 *
 *   - value : an out parameter filled set to the decoded value.
 *
 * A pointer to the byte following the decoded value is returned.
 */
static unsigned char *parse_uleb128(unsigned char *start, uint64_t *value)
{
    uint64_t res = 0;
    unsigned bits = 0;

    /*
     * Read a sequence of bytes so long as the high bit is set.  Form a value
     * using the lower 7 bits in little-endian order.
     */
    while (*start & 0x80) {
        res |= (((uint64_t)*start) & 0x7F) << bits;
        bits += 7;
        ++start;
    }

    /*
     * Bring in the most significant byte.
     */
    res |= (((uint64_t)*start) & 0x7F) << bits;

    *value = res;
    return ++start;
}

static unsigned char *parse_sleb128(unsigned char *start, int64_t *value)
{
    uint64_t res = 0;
    unsigned bits = 0;

    /*
     * Read a sequence of bytes so long as the high bit is set.  Form a value
     * using the lower 7 bits in little-endian order.
     */
    while (*start & 0x80) {
        res |= (((uint64_t)*start) & 0x7F) << bits;
        bits += 7;
        ++start;
    }

    /*
     * Bring in the most significant byte.  If it is signed, extend the result.
     */
    res |= (((uint64_t)*start) & 0x7F) << bits;

    if (*start & 0x40) {
        bits += 7;
        res |= ~((uint64_t)0) << bits;
    }

    *value = (int64_t)res;
    return ++start;
}

/*
 * The following subroutine parses a DWARF encoded value.
 *
 *   - ID    : The type of DWARF value to parse.
 *
 *   - start : A pointer to the first byte of the encoded value.
 *
 *   - value : The parsed result.
 *
 * A pointer to the byte following the decoded value is returned.
 *
 * Note that this implementation uses memcpy to ensure that we do not do
 * unaligned memory accesses.
 */
static unsigned char *
parse_dwarf_value(Dwarf_Encoding ID, unsigned char *start, uint64_t *value)
{
    switch (ID) {

    default:
        assert(0 && "Invalid DWARF encoding!");
        return start;

    case DW_EH_PE_omit:
        return start;

    case DW_EH_PE_uleb128:
        return parse_uleb128(start, value);

    case DW_EH_PE_udata2: {
        uint8_t dst;
        memcpy(&dst, start, 2);
        *value = (uint64_t)dst;
        return start + 2;
    }

    case DW_EH_PE_udata4: {
        uint16_t dst;
        memcpy(&dst, start, 4);
        *value = (uint64_t)dst;
        return start + 4;
    }

    case DW_EH_PE_udata8:
        memcpy(value, start, 8);
        return start + 8;

    case DW_EH_PE_sleb128:
        return parse_sleb128(start, (int64_t*)value);

    case DW_EH_PE_sdata2: {
        int8_t dst;
        memcpy(&dst, start, 2);
        *value = (int64_t)dst;
        return start + 2;
    }

    case DW_EH_PE_sdata4: {
        int16_t dst;
        memcpy(&dst, start, 4);
        *value = (int64_t)dst;
        return start + 4;
    }

    case DW_EH_PE_sdata8:
        memcpy(value, start, 8);
        return start + 8;
    }
}

/*===----------------------------------------------------------------------===
 * LSDA
 *===----------------------------------------------------------------------===*/

/*
 * The Language Specific Data Area is an opaque structure.  The contents provide
 * the data needed by our exception personality to identify the nature of a
 * particular call frame.  The information is organized as a set of DWARF
 * encoded tables.  A sketch of the layout is given below.  Items in the sketch
 * delimited by "===" markers introduce a sub-table of the LSDA and do not
 * represent actual entries.
 *
 *   +==================+          +=============+
 *   | LSDA             |          | TYPES TABLE |
 *   +==================+          +=============+
 *   | @LPStart         |          | Type info N |
 *   +------------------+          +-------------+
 *   | @TTBase format   |                ...
 *   +------------------+          +-------------+
 *   | @TTBase          | -----+   | Type info 2 |
 *   +==================+      |   +-------------+
 *   | CALL-SITE TABLE  |      |   | Type info 1 |
 *   +==================+      |   +-------------+
 *   | format           |      +-> | 0           |
 *   +------------------+          +-------------+
 *   | size             | --+      | Filter 1    |
 *   +------------------+   |      +-------------+
 *   | Call-site record |   |            ...
 *   +------------------+   |      +-------------+
 *           ...            |      | Filter N    |
 *   +==================+   |      +-------------+
 *   | ACTION TABLE     |   |
 *   +==================+   |
 *   | Action record    | <-+
 *   +------------------+   |
 *   | Action record    | <-+
 *   +------------------+
 *           ...
 *
 * The header of the LSDA consists of the following values.
 *
 *    @LPStart : A DWARF encoded value yielding an offset relative to the LSDA
 *      to the start of the landing pad code.  If the encoding is DW_EH_PE_omit,
 *      then there is no value and the offset to use to calculate the location
 *      of the landing pad code is given by a call to _Unwind_GetRegionStart.
 *
 *    @TTBase format : A DWARF encoding byte telling us how to interpret
 *      the entries in the type table.
 *
 *    @TTBase : An unsigned LEB128 value yielding a self relative offset.  This
 *      gives the start of the type table (well, the `middle' of the type table
 *      as seen in the diagram above).
 *
 * The call-site table is organized as follows:
 *
 *    format : This is one of the Dwarf_Encoding values and is used to interpret
 *      the data in the call site records when the precise format of a value is
 *      otherwise undefined.
 *
 *    size : An unsigned LED128 value giving total size (in bytes) of all the
 *       call-site records to follow.
 *
 * Call site records have the following format:
 *
 *    +==================+
 *    | CALL-SITE RECORD |
 *    +==================+
 *    | region start     |
 *    +------------------+
 *    | region length    |
 *    +------------------+
 *    | landing pad      |
 *    +------------------+
 *    | action           |
 *    +------------------+
 *
 *   region start : Offset of the call site relative to the value returned by a
 *     call to _Unwind_GetRegionStart.  This value should be interpreted using
 *     the table encoding.
 *
 *   region length : Length of the call site region.  This value should be
 *     interpreted using the table encoding.
 *
 *   landing pad : Offset of the landing pad for this region relative to
 *     @LPBase.  A landing pad offset value of 0 means there is no landing pad,
 *     and therefore no action, for this particular call site.  This value
 *     should be interpreted using the table encoding.
 *
 *   action : Index into the actions table (which immediately follows the
 *     call-site table).  A action index of 0 means that the landing pad points
 *     to a cleanup rather than a handler.  This value is encoded as an unsigned
 *     LEB128.
 *
 * Immediately following the call-site table is the action table, consisting of
 * a number of action records.  These records have the following form:
 *
 *    +==================+
 *    | ACTION RECORD    |
 *    +==================+
 *    | type index       |
 *    +------------------+
 *    | next action      |
 *    +------------------+
 *
 *   type index : An index into the Types Table, relative to @TTBase.  These are
 *     encoded as a signed LEB128 value.  There are two cases to consider:
 *
 *     For an index I > 0, compute @TTBase - I, yielding a type info object
 *     corresponding to a particular handler.
 *
 *     For an index I < 0, compute @TTBase + I, yielding a null terminated
 *     filter.  These are not used in Comma.  Type filters are the mechanism
 *     used to enforce C++ throw() specifications.
 *
 *   next action : The offset in bytes from the beginning of this action record
 *     to the start of the next (since these entries are variable length).  This
 *     is represented as a signed LEB128.  A value of 0 terminates the chain of
 *     action records.
 */

/*
 * Forward declarations for the structures used to represent the LSDA tables.
 */
struct LSDA_Header;
struct Call_Site;
struct Action_Record;

/*
 * The following structure is used to represent the LSDA Header.
 */
struct LSDA_Header {
    /*
     * Start address of the LSDA.
     */
    unsigned char *lsda_start;

    /*
     * Start of the landing pad code, corresoinding the @LPStart.
     */
    intptr_t lpstart;

    /*
     * DWARF format attribute used for interpreting @TTBase.
     */
    unsigned char type_table_format;

    /*
     * @TTBase table pointer.
     */
    unsigned char *type_table;

    /*
     * Dwarf encoding format to use while interpreting call site records.
     */
    Dwarf_Encoding call_site_format;

    /*
     * Call site table size.
     */
    uint64_t call_table_size;

    /*
     * Pointer to the first call site record in the table.
     */
    unsigned char *call_sites;

    /*
     * Pointer to the actions table.
     */
    unsigned char *actions;
};

/*
 * We parse the variable length call-site entries into the following structure.
 */
struct Call_Site {
    /*
     * Start of the call site (relative to _Unwind_GetRegionStart).
     */
    int64_t region_start;

    /*
     * Length of the call site in bytes.
     */
    uint64_t region_length;

    /*
     * Landing pad offset (relative to @LPStart).
     */
    int64_t landing_pad;

    /*
     * Action index.
     */
    uint64_t action_index;
};

/*
 * We parse the variable length action records into the following structure.
 */
struct Action_Record {
    /*
     * Index into the type table.
     */
    uint64_t info_index;

    /*
     * Offset in bytes to the next action.
     */
    uint64_t next_action;
};

/*===----------------------------------------------------------------------===
 * LSDA Parsers and Comma's Exception Personality.
 *===----------------------------------------------------------------------===*/

/*
 * The following routine aborts the process when a hard error is encountered.
 */
static void fatal_error(const char *message)
{
    fprintf(stderr, "EXCEPTION ERROR : %s\n", message);
    abort();
}

/*
 * This is the exception object thrown by the runtime.  It contains the
 * exceptions exinfo object and a pointer to a null terminated string yielding a
 * message.
 */
struct comma_exception {
    comma_exinfo_t id;
    const char *message;
    struct _Unwind_Exception header;
};

/*
 * Conversions to and from comma_exception's and _Unwind_Exceptions.
 */
struct comma_exception *
to_comma_exception(struct _Unwind_Exception *header)
{
    char *ptr = (char *)header;
    ptr -= offsetof(struct comma_exception, header);
    return (struct comma_exception *)ptr;
}

struct _Unwind_Exception *
to_Unwind_Exception(struct comma_exception *exception)
{
    return &exception->header;
}

/*
 * The following function parses the LSDA header.  Returns 0 in sucess and 1 on
 * error.
 */
static int parse_LSDA(struct _Unwind_Context *context, struct LSDA_Header *lsda)
{
    unsigned char *ptr;
    uint64_t tmp;

    lsda->lsda_start =
        (unsigned char *)_Unwind_GetLanguageSpecificData(context);

    if (!lsda->lsda_start)
        return 1;
    else
        ptr = lsda->lsda_start;

    /*
     * Parse @LPStart.  This is given by _Unwind_GetRegionStart, possibly
     * augmented by a DWARF value.
     *
     * FIXME: We do not parse the DWARF value currently.  LLVM never emits such
     * code at this time.
     */
    if (*ptr == DW_EH_PE_omit) {
        lsda->lpstart = _Unwind_GetRegionStart(context);
        ++ptr;
    }
    else
        fatal_error("Unexpected DWARF value for @LPStart!");

    /*
     * FIXME: Currently, we do not make use of the @TTBase format.  We assume it
     * is DW_EH_PE_absptr and interpet the type table entries as such.  See
     * match_exception() for a few more notes on the issue.
     */
    lsda->type_table_format = *ptr;
    ++ptr;
    if (lsda->type_table_format != DW_EH_PE_absptr)
        fatal_error("Unexpected type table format!");

    /*
     * Take @TTBase as a self relative offset.
     */
    ptr = parse_uleb128(ptr, &tmp);
    lsda->type_table = ptr + tmp;

    /*
     * Read in the call site encoding.  Unlike @TTBase format, we can respect
     * this.
     */
    lsda->call_site_format = *ptr;
    ++ptr;

    /*
     * Read the call table size as an unsigned LEB128.
     */
    ptr = parse_uleb128(ptr, &lsda->call_table_size);

    /*
     * The current pointer indicates the start of the call site table.
     */
    lsda->call_sites = ptr;

    /*
     * The action table immediately follows the call site table.
     */
    lsda->actions = lsda->call_sites + lsda->call_table_size;

    return 0;
}

/*
 * Debug routine to spill the contents of an LSDA_Header, conditional on the
 * setting of COMMA_EH_DEBUG.
 */
static void dump_LSDA(struct LSDA_Header *lsda)
{
#ifdef COMMA_EH_DEBUG
    fputs("LSDA_Header  :\n", stderr);
    fprintf(stderr, " lsda_start  : %" PRIXPTR "\n",
            (uintptr_t)lsda->lsda_start);
    fprintf(stderr, " lpstart     : %" PRIXPTR "\n",
            lsda->lpstart);
    fprintf(stderr, " tt format   : %u\n",
            (unsigned)lsda->type_table_format);
    fprintf(stderr, " type_table  : %" PRIXPTR "\n",
            (uintptr_t)lsda->type_table);
    fprintf(stderr, " call format : %u\n",
            (unsigned)lsda->call_site_format);
    fprintf(stderr, " call size   : %" PRIu64 "\n",
            lsda->call_table_size);
    fprintf(stderr, " call sites  : %" PRIXPTR "\n",
            (uintptr_t)lsda->call_sites);
    fprintf(stderr, " actions     : %" PRIXPTR "\n",
            (uintptr_t)lsda->actions);
#endif
}

/*
 * The following function parses a call-site filling dst with the result.
 * Returns a pointer to the next site on success or null if there are no more
 * call sites to parse.
 */
static unsigned char *
parse_Call_Site(struct LSDA_Header *lsda,
                unsigned char *ptr, struct Call_Site *dst)
{
    /*
     * First, check that the given pointer is within the bounds of the
     * call-site table.
     */
    if (ptr >= (lsda->call_sites + lsda->call_table_size))
        return 0;

    /*
     * Using the DWARF format control for the call size table, parse the call
     * size region, length, and landing pad info.
     */
    ptr = parse_dwarf_value(lsda->call_site_format,
                            ptr, (uint64_t*)&dst->region_start);
    ptr = parse_dwarf_value(lsda->call_site_format,
                            ptr, &dst->region_length);
    ptr = parse_dwarf_value(lsda->call_site_format,
                            ptr, (uint64_t*)&dst->landing_pad);

    /*
     * The action is always an unsigned LEB128 value.
     */
    ptr = parse_uleb128(ptr, &dst->action_index);
    return ptr;
}

/*
 * Debug routine to spill the contents of a Call_Site, conditional on the
 * setting of COMMA_EH_DEBUG.
 */
static void dump_Call_Site(struct Call_Site *site)
{
#ifdef COMMA_EH_DEBUG
    fputs("Call_Site      :\n", stderr);
    fprintf(stderr, " region_start  : %" PRIx64 "\n", site->region_start);
    fprintf(stderr, " region_length : %" PRIu64 "\n", site->region_length);
    fprintf(stderr, " landing_pad   : %" PRIx64 "\n", site->landing_pad);
    fprintf(stderr, " action_index  : %" PRIu64 "\n", site->action_index);
#endif
}

/*
 * Parses an action record starting at the given address.  Returns a pointer to
 * the next action record if one follows, else null.
 */
static unsigned char *
parse_Action_Record(unsigned char *ptr, struct Action_Record *dst)
{
    /*
     * Retain the address of the current action record so that we can produce a
     * pointer to the next entry.  It seems almost guaranteed that the action
     * table is contiguous and packed, but there is no harm here in doing it
     * `right'.
     */
    unsigned char *start = ptr;

    /*
     * Both fields of an action record are signed LEB128 encoded values.
     */
    ptr = parse_sleb128(ptr, (int64_t*)&dst->info_index);
    parse_sleb128(ptr, (int64_t*)&dst->next_action);

    /*
     * A next value of zero terminates the action list.  Build a pointer to the
     * next entry or signal completion.
     */
    if (dst->next_action)
        return start + dst->next_action;
    else
        return 0;
}

/*
 * The following routine walks the list of call site records provided by the
 * LSDA and determines which one is applicable using the given _Unwind_Context.
 * Fills the Call_Site structure dst with the relevant info and returns 0 on
 * success.  Otherwise, returns 1 with the contents of dst undefined.
 */
static int
find_applicable_call_site(struct LSDA_Header *lsda,
                          struct _Unwind_Context *context,
                          struct Call_Site *dst)
{
    /*
     * Get the current instruction pointer.  This value is one past the location
     * of the actual instruction that raised, hense the -1.
     */
    unsigned char *IP = (unsigned char *)_Unwind_GetIP(context) - 1;

    /*
     * Get a pointer to the initial call site record.
     */
    unsigned char *ptr = lsda->call_sites;

    /*
     * Get a pointer to the region of code this exception header covers.  The
     * call site regions start relative to this position.
     */
    unsigned char *region =
        (unsigned char *)_Unwind_GetRegionStart(context);

    /*
     * Walk the list of call sites.
     */
    while ((ptr = parse_Call_Site(lsda, ptr, dst))) {

        /*
         * Since the call sites are sorted, we stop when the IP is less than the
         * start of the current site records region.
         */
        unsigned char *region_start = region + dst->region_start;
        if (IP < region_start)
            return 1;

        /*
         * We know the IP is greater than or equal to the start of the call site
         * region.  Check if it is in bounds.  If so, we have a match.
         */
        if (IP < region_start + dst->region_length)
            return 0;
    }

    /*
     * We have scanned all of the call sites.  Not only did we not find a match
     * but the IP was out of bounds for all of them.  This should never happen.
     */
    fatal_error("IP out of bounds for call site table!");
    return 1;
}

/*
 * The following routine takes an LSDA, an action index, and a generic exception
 * object as arguments.  Returns 0 if the exception object matches the set of
 * exception ID's starting at the given action index and 1 otherwise.  When a
 * match was found, the matching action index is placed in dst.
 */
static int
match_exception(struct LSDA_Header *lsda,
                struct _Unwind_Exception *exceptionObject,
                uint64_t action_index, uint64_t *dst)
{
    struct Action_Record action;
    unsigned char *ptr;

    /*
     * Convert the generic exception into a Comma exception.
     */
    struct comma_exception *exception = to_comma_exception(exceptionObject);

    /*
     * We should never have a zero action index, as that indicates no action at
     * all.  Assert this fact and remove the bias.
     */
    if (action_index == 0) {
        fatal_error("Invalid action index!");
        return 1;
    }
    else {
        action_index -= 1;
        ptr = lsda->actions + action_index;
    }

    /*
     * Parse each action record in search of a match.
     */
    do {
        ptr = parse_Action_Record(ptr, &action);

        if (action.info_index > 0) {
            /*
             * FIXME: We should be using the type table format here to interpret
             * both the size of the entries themselves and how to interpret the
             * values.  Here we assume a format of DW_EH_PE_absptr.  This works
             * for static code, but there are other cases (for PIC code the
             * references will be indirect).  In short, we should:
             *
             *  - Use the ttable format to get a scale factor for the action
             *    index.
             *
             *  - Use the ttable format to fetch the required base.  For
             *    example, a format of DW_EH_PE_funcrel would indicate that we
             *    interpret the entries as offsets relative to the value of
             *    _Unwind_GetRegionStart().
             */
            comma_exinfo_t **info;
            info = (comma_exinfo_t **)lsda->type_table;
            info -= action.info_index;

            /*
             * If the info pointer is 0, then this is a catch-all.  Otherwise,
             * the exeption objects id must match the handlers associated
             * exinfo.
             */
            if ((*info == 0) || (exception->id == **info)) {
                *dst = action.info_index;
                return 0;
            }

        }
        else {
            /*
             * Negative indices in an action record correspond to filters.  Such
             * things are only used in C++ and should never appear in Comma.
             */
            if (action.info_index < 0) {
                fatal_error("Filter action detected!");
                return 1;
            }
        }
    } while (ptr);

    /*
     * No match was found.
     */
    return 1;
}

static void
install_handler(struct _Unwind_Context *context,
                struct _Unwind_Exception *exceptionObject,
                int64_t landing_pad, uintptr_t id)
{
    /*
     * Handler code expects the exception object to be in the native Comma
     * format, rather than the generic format used by the system unwinder.
     */
    struct comma_exception *exception = to_comma_exception(exceptionObject);

    /*
     * Load the register to contain the exception pointer.
     */
    _Unwind_SetGR(context, __builtin_eh_return_data_regno(0),
                  (uintptr_t)exception);

    /*
     * Return the matching exception identity to that the landing pad knows
     * which action to take.
     */
    _Unwind_SetGR(context, __builtin_eh_return_data_regno(1), id);

    /*
     * Set the IP to the location of the landing pad.
     */
    _Unwind_SetIP(context, (uintptr_t)landing_pad);
}

/*
 * The following magic number identifies Comma exceptions.  It reads as the
 * string "SMW\0CMA\0", where the first four bytes denotes the "vendor" (my
 * initials, in this case), and the last four bytes identifies the language.
 */
static const uint64_t Comma_Exception_Class_ID = 0x534D570434D410Ull;

/*
 * Cleanup routine for Comma exceptions.
 */
static void _comma_cleanup_exception(_Unwind_Reason_Code reason,
                                     struct _Unwind_Exception *exc)
{
    struct comma_exception *exception = to_comma_exception(exc);
    free(exception);
}

/*
 * The following function is called to allocate and raise a Comma exception.
 */
void _comma_raise_exception(comma_exinfo_t info, const char *message)
{
    struct comma_exception *exception;
    struct _Unwind_Exception *exception_object;

    /*
     * Allocate the exception and fill in the Comma specific bits.
     */
    exception = malloc(sizeof(struct comma_exception));
    exception->id = info;
    exception->message = message;

    /*
     * Fill in the ABI bits.
     *
     * The magic number reads as "SMW\0CMA\0"
     */
    exception->header.exception_class = Comma_Exception_Class_ID;
    exception->header.exception_cleanup = _comma_cleanup_exception;

    /*
     * Fire it off.
     */
    exception_object = to_Unwind_Exception(exception);
    _Unwind_RaiseException(exception_object);
}

/*
 * Raises a specific system exception.
 */
void _comma_raise_system(uint32_t id, const char *message)
{
    comma_exinfo_t info = _comma_get_exception(id);
    _comma_raise_exception(info, message);
}

/*
 * Comma's exception personality.
 */
_Unwind_Reason_Code
_comma_eh_personality(int version,
                      _Unwind_Action actions,
                      uint64_t exceptionClass,
                      struct _Unwind_Exception *exceptionObject,
                      struct _Unwind_Context *context)
{
    struct LSDA_Header lsda;
    struct Call_Site site;
    uint64_t id;
    intptr_t handler;

    /*
     * The C++ Itanium ABI specifies a version number of 1.  This is the only
     * ABI we cope with.
     */
    if (version != 1) {
        return _URC_FATAL_PHASE1_ERROR;
    }

    /*
     * Check that the exception class is a Comma exception.  Currently, this is
     * a fatal error.  In the future, we may support the catching of foreign
     * exceptions in a catch-all context.
     */
    if (exceptionClass != Comma_Exception_Class_ID) {
        return _URC_FATAL_PHASE1_ERROR;
    }

    /*
     * If we could not parse the lsda, continue to unwind.
     */
    if (parse_LSDA(context, &lsda))
        return _URC_CONTINUE_UNWIND;
    dump_LSDA(&lsda);

    /*
     * Find the applicable call site.  If none was found, continue unwinding.
     */
    if (find_applicable_call_site(&lsda, context, &site))
        return _URC_CONTINUE_UNWIND;
    dump_Call_Site(&site);

    /*
     * If this site does not define a landing pad there is nothing to do.
     * Continue to unwind.
     */
    if (!site.landing_pad)
        return _URC_CONTINUE_UNWIND;

    /*
     * If there is no action defined for this site, then this is a cleanup.
     * Comma does not support cleanups ATM, so this is a hard error for now.
     */
    if (!site.action_index) {
        fatal_error("Cleanups are not supported!");
        return _URC_FATAL_PHASE1_ERROR;
    }

    /*
     * We have an action index.  If the thrown exception matches any of the
     * excption ids acceptable to the landing pad we have found a handler.  If
     * not, continue to unwind.
     */
    if (match_exception(&lsda, exceptionObject, site.action_index, &id))
        return _URC_CONTINUE_UNWIND;

    /*
     * A handler was found.  Report the fact when in phase 1.
     */
    if (actions & _UA_SEARCH_PHASE)
        return _URC_HANDLER_FOUND;

    /*
     * If the _UA_HANDLER_FRAME bit is not set then continue to unwind.
     */
    if (!(actions & _UA_HANDLER_FRAME))
        return _URC_CONTINUE_UNWIND;

    /*
     * We are in phase 2. Compute the landing pad address relative to @LPStart
     * and install into the context.
     */
    handler = lsda.lpstart + site.landing_pad;
    install_handler(context, exceptionObject, handler, id);
    return _URC_INSTALL_CONTEXT;
}

/*
 * When the main Comma procedure catches an uncaught exception, the following
 * function is invoked to report the incident.
 */
void _comma_unhandled_exception(struct comma_exception *exception)
{
    fprintf(stderr, "Unhandled exception: %s: %s\n",
            exception->id,
            exception->message);
    abort();
}

/*
 * The following items are the built in comma_exinfo's.  These are exported
 * symbols as the compiler generates direct references to them.
 */
comma_exinfo_t _comma_exinfo_program_error = "PROGRAM_ERROR";
comma_exinfo_t _comma_exinfo_constraint_error = "CONSTRAINT_ERROR";

/*
 * API to access the system-level exinfo's.
 */
comma_exinfo_t _comma_get_exception(comma_exception_id id)
{
    comma_exinfo_t info = 0;
    switch (id) {
    default:
        fatal_error("Invalid exception ID!");
        break;

    case COMMA_CONSTRAINT_ERROR_E:
        info = _comma_exinfo_constraint_error;
        break;

    case COMMA_PROGRAM_ERROR_E:
        info = _comma_exinfo_program_error;
        break;
    }
    return info;
}
