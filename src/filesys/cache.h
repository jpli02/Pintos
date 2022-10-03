#ifndef FILESYS_CACHE
#define FILESYS_CACHE

#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "lib/debug.h"
#include <string.h>

#define BUFFER_CACHE_SIZE 64

struct buffer_cache_entry
{
    bool inuse;                         /* whether this block is used */
    bool pinned;                        /* Used for evition */
    bool dirty;                         /* Used for flush */

    block_sector_t block_index;         /* block location */
    uint8_t buffer[BLOCK_SECTOR_SIZE];  /* contents */
};

void buffer_cache_init (void);
void buffer_cache_close (void);
void buffer_cache_read (struct block *block, block_sector_t block_index, void *buffer);
void buffer_cache_write (struct block *block, block_sector_t block_index, void *buffer);

struct buffer_cache_entry *buffer_cache_evict (void);
void buffer_cache_flush (struct buffer_cache_entry *);
struct buffer_cache_entry *buffer_cache_lookup (block_sector_t block_index);

#endif