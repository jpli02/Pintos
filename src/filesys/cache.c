#include "cache.h"

static int capacity;

struct buffer_cache_entry buffer_cache_list[BUFFER_CACHE_SIZE];

struct lock buffer_cache_lock;

static int clock_pin;

void
buffer_cache_init()
{
    lock_init (&buffer_cache_lock);
    printf("lock inited\n");
    clock_pin = 0;

    for (int i = 0; i < BUFFER_CACHE_SIZE; i++)
      {
          buffer_cache_list[i].inuse = false;
      }
    capacity = 0;
}

void
buffer_cache_close(void)
{
    lock_acquire (&buffer_cache_lock);

    for (int i = 0; i < BUFFER_CACHE_SIZE; i++)
      {
          if (buffer_cache_list[i].dirty == true \
              && buffer_cache_list[i].inuse == true)
            {
                buffer_cache_flush (&buffer_cache_list[i]);
            }
      }
    
    lock_release (&buffer_cache_lock);
}

void
buffer_cache_read(struct block *src, block_sector_t block_index, void *buffer)
{
    lock_acquire (&buffer_cache_lock);

    struct buffer_cache_entry *bce = buffer_cache_lookup (block_index);
    if (bce == NULL)
      {
          bce = buffer_cache_evict ();
          bce->inuse = true;
          bce->dirty = false;
          bce->block_index = block_index;
          block_read (src, block_index, bce->buffer);
      }
    bce->pinned = true;
    memcpy (buffer, bce->buffer, BLOCK_SECTOR_SIZE);

    lock_release (&buffer_cache_lock);
}

void
buffer_cache_write(struct block *src, block_sector_t block_index, void *buffer)
{
    lock_acquire (&buffer_cache_lock);

    struct buffer_cache_entry *bce = buffer_cache_lookup (block_index);
    if (bce == NULL)
      {
          bce = buffer_cache_evict ();
          bce->inuse = true;
          bce->block_index = block_index;
          block_read (src, block_index, bce->buffer);
      }
    bce->pinned = true;
    bce->dirty = true;
    memcpy (bce->buffer, buffer, BLOCK_SECTOR_SIZE);

    lock_release (&buffer_cache_lock);
}

struct buffer_cache_entry *
buffer_cache_evict()
{
    ASSERT (lock_held_by_current_thread (&buffer_cache_lock));
    /* Check whether there are any unused cache */
    for (int i = 0; i < BUFFER_CACHE_SIZE; i++)
      {
          if (!buffer_cache_list[clock_pin].inuse)
            {
                return &buffer_cache_list[clock_pin];
            }
      }
    /* Clock algorithm */
    while (true)
      {
          if (!buffer_cache_list[clock_pin].pinned)
            {
                break;
            }
          else
            {
                buffer_cache_list[clock_pin].pinned = false;
            }
          clock_pin = (clock_pin + 1) % BUFFER_CACHE_SIZE;
      }
    if (buffer_cache_list[clock_pin].dirty)
      {
          buffer_cache_flush (&buffer_cache_list[clock_pin]);
      }
    buffer_cache_list[clock_pin].inuse = false;
    return &buffer_cache_list[clock_pin];
}

void
buffer_cache_flush(struct buffer_cache_entry * bce)
{
    ASSERT (bce != NULL);
    ASSERT (bce->dirty);
    ASSERT (lock_held_by_current_thread (&buffer_cache_lock));
    ASSERT (bce->inuse);

    block_write (fs_device, bce->block_index, bce->buffer);
    bce->dirty = false;
}

struct buffer_cache_entry *
buffer_cache_lookup(block_sector_t block_index)
{
    ASSERT (lock_held_by_current_thread (&buffer_cache_lock));
    struct buffer_cache_entry * bce = NULL;
    for (int i = 0; i < BUFFER_CACHE_SIZE; i++)
      {
          if (buffer_cache_list[i].block_index == block_index \
              && buffer_cache_list[i].inuse == true)
            {
                bce = &buffer_cache_list[i];
            }
      }

    return bce;
}