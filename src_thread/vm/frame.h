#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <threads/thread.h>
#include <lib/kernel/hash.h>
#include "threads/palloc.h"

struct frame_table_entry
{
    struct thread *owner;           /* The owner of the frame */
    void * frame;                   /* The page the frame table entry points to */
    int unused;                     /* The number of unused hit */

    struct hash_elem helem;         /* Used for hash_map */
    struct list_elem lelem;         /* Used for list element */
};

void   frame_init(void);
void * frame_allocate (enum palloc_flags flag);

#endif