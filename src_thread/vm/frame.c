#include <stdio.h>

#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"

#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/malloc.h"
// #include "threads/palloc.h"

/* A global lock, to ensure that only one thread can operate frame */
static struct lock frame_lock;

/* The "core_map" of frame */
static struct hash frame_map;

static unsigned frame_hash_func(const struct hash_elem *elem, void *aux);
static bool frame_less_func(const struct hash_elem *, const struct hash_elem *, void *aux);

/* The frame table list */
static struct list frame_table_list;

void
frame_init ()
{
    lock_init (&frame_lock);
    hash_init (&frame_map, frame_hash_func, frame_less_func, NULL);
    list_init (&frame_table_list);
    // printf ("Initialize done\n");
}

void *
frame_allocate (enum palloc_flags flag)
{
    lock_acquire (&frame_lock);
    void *frame = palloc_get_page (PAL_USER | flag);

    if (frame == NULL) /* If we are not lucky */
      {
        PANIC ("NO ENOUGH MEMORY");
      }
    /* We have solved the triky part */
    struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
    fte->owner = thread_current ();
    fte->frame = frame;
    hash_insert (&frame_map, &fte->helem);
    list_push_back (&frame_table_list, &fte->lelem);

    lock_release (&frame_lock);
    return frame;
}



// Hash Functions required for [frame_map]. Uses 'kpage' as key.
static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame_table_entry *entry = hash_entry(elem, struct frame_table_entry, helem);
  return hash_bytes( &entry->frame, sizeof entry->frame );
}
static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *a_entry = hash_entry(a, struct frame_table_entry, helem);
  struct frame_table_entry *b_entry = hash_entry(b, struct frame_table_entry, helem);
  return a_entry->frame < b_entry->frame;
}
