/*===-- runtime/crt_vstack.c ----------------------------------------------===
 *
 * This file is distributed under the MIT license. See LICENSE.txt for details.
 *
 * Copyright (C) 2009, Stephen Wilson
 *
 *===----------------------------------------------------------------------===*/

#include "comma/runtime/crt_vstack.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * The vstack is represented as a singly linked list of vstack_entry structures.
 */
struct vstack_entry {
    struct vstack_entry *prev;
    char data[];
};
typedef struct vstack_entry *vstack_entry_t;

/*
 * The externally visible stack pointer.  This value is set to the address of a
 * vstack_entry's data member, thus giving Comma code access to the pushed data.
 */
char *_comma_vstack = 0;

/*
 * Returns the top-most element from the vstack.
 */
static inline vstack_entry_t get_vstack_entry()
{
    vstack_entry_t res;
    res = (vstack_entry_t)(_comma_vstack - offsetof(struct vstack_entry, data));
    return res;
}

/*
 * Sets the top-most entry of the vstack.
 */
static inline void set_vstack_entry(vstack_entry_t entry)
{
    _comma_vstack = (char*)entry->data;
}

/*
 * Public routine implementations.
 */
void _comma_vstack_alloc(int32_t size)
{
    vstack_entry_t entry = malloc(sizeof(struct vstack_entry) + size);

    entry->prev = get_vstack_entry();
    set_vstack_entry(entry);
}

void _comma_vstack_push(void *data, int32_t size)
{
    vstack_entry_t entry = malloc(sizeof(struct vstack_entry) + size);

    entry->prev = get_vstack_entry();
    memcpy(entry->data, data, size);
    set_vstack_entry(entry);
}

void _comma_vstack_pop()
{
    vstack_entry_t entry = get_vstack_entry();
    set_vstack_entry(entry->prev);
    free(entry);
}
