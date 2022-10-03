#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_BN 12
#define INDIRECT_BN 128

#define LAYER_0 DIRECT_BN
#define LAYER_1 DIRECT_BN + INDIRECT_BN
#define LAYER_2 DIRECT_BN + INDIRECT_BN + INDIRECT_BN * INDIRECT_BN

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t direct_blocks[DIRECT_BN];     /* First data sector. */
    block_sector_t indirect_pointer; 
    block_sector_t double_indirect_pointer;

    int indirect_layer;                     /* Default is 0 */
    int inode_type;                         /* necessary */
    off_t length;                           /* File size in bytes. */
    unsigned magic;                         /* Magic number. */
    uint32_t unused[110];                   /* Not used. */
  };

struct indirect_inode_disk
  {
    block_sector_t blocks[INDIRECT_BN];
  };


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  // if (pos < inode->data.length)
  //   {
  //     return inode->data.direct_blocks[0] + pos / BLOCK_SECTOR_SIZE;
  //   }
  // else
  //   return -1;
  if (pos < 0 || pos > inode->data.length)
    {
      return -1;
    }
  off_t index = pos / BLOCK_SECTOR_SIZE;
  
  if (index < LAYER_0)
    {
      // printf ("CK0\n");
      return inode->data.direct_blocks[index];
    }
  else if (index < LAYER_1)
    {
      struct indirect_inode_disk iid;
      buffer_cache_read (fs_device, inode->data.indirect_pointer, &iid);
      // printf ("what we want is %d\n", iid.blocks[index - LAYER_0]);
      return iid.blocks[index-LAYER_0];
    }
  else if (index < LAYER_2)
    {
      struct indirect_inode_disk double_iid;
      buffer_cache_read (fs_device, inode->data.double_indirect_pointer, &double_iid);
      int location = (index - LAYER_1)/INDIRECT_BN;
      int offset = (index - LAYER_1)%INDIRECT_BN;
      struct indirect_inode_disk iid;
      buffer_cache_read (fs_device, double_iid.blocks[location], &iid);
      return iid.blocks[offset];
    }
  else
    {
      return -1;
    }
}
/* note we have to update length info after call allocate function */
bool inode_allocate (struct inode_disk *, off_t);
bool inode_allocate_direct (struct inode_disk *, off_t);
bool inode_allocate_indirect (struct inode_disk *, off_t);
bool inode_allocate_indirect_double (struct inode_disk *, off_t);
bool inode_deallocate (struct inode *);

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, int type)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  // printf ("sector %d of length %d is created\n", sector, length);


  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->inode_type = type;
      for (int i = 0; i < DIRECT_BN; i++)
        {
          disk_inode->direct_blocks[i] = 0;
        }
      disk_inode->indirect_pointer = 0;
      disk_inode->double_indirect_pointer = 0;
      // if (free_map_allocate (sectors, &disk_inode->direct_blocks[0])) 
      //   {
      //     buffer_cache_write (fs_device, sector, disk_inode);
      //     if (sectors > 0) 
      //       {
      //         static char zeros[BLOCK_SECTOR_SIZE];
      //         size_t i;
              
      //         for (i = 0; i < sectors; i++) 
      //           buffer_cache_write (fs_device, disk_inode->direct_blocks[0] + i, zeros);
      //       }
      //     success = true; 
      //   } 
      /* i decide to use sparse file system. */
      if (inode_allocate(disk_inode, length))
        {
          buffer_cache_write (fs_device, sector, disk_inode);
          success = true; 
          // printf ("Cool\n");
        }
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  // printf ("sector %d is opened\n", sector);
  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  buffer_cache_read (fs_device, inode->sector, &inode->data);
  
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      // printf ("the length of inode is %d\n", inode_length (inode));
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          PANIC ("REACH HERE\n");
          free_map_release (inode->sector, 1);
          // free_map_release (inode->data.direct_blocks[0],
          //                   bytes_to_sectors (inode->data.length));
          inode_deallocate (inode);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, direct_blocks[0]ing at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, direct_blocks[0]ing byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          buffer_cache_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          buffer_cache_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, direct_blocks[0]ing at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (byte_to_sector (inode, size+offset) == -1)
    {
      // printf ("current size is %d:realloc size is %d\n",inode->data.length, offset+size);
      inode->data.length = size + offset;
      buffer_cache_write (fs_device, inode->sector, &inode->data);
      if (!inode_allocate(&inode->data, size+offset))
        {
          PANIC ("Realloc failed\n");
        }
      inode->data.length = size + offset;
      // printf ("sector is %d\n", inode->sector);
      buffer_cache_write (fs_device, inode->sector, &inode->data);
      // printf ("the length of the sector is %d", inode_length (inode));
    }

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, direct_blocks[0]ing byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          buffer_cache_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we direct_blocks[0] with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            buffer_cache_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          buffer_cache_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool
inode_allocate (struct inode_disk * disk_inode, off_t length)
{
  size_t sectors = bytes_to_sectors (length);

  // printf ("length of the file is %d\n", disk_inode->length);

  if (sectors < LAYER_0)
    {
      return inode_allocate_direct (disk_inode, sectors);
    }
  else if (sectors < LAYER_1)
    {
      return inode_allocate_direct (disk_inode, LAYER_0) &&\
             inode_allocate_indirect (disk_inode, sectors-LAYER_0);
    }
  else if (sectors < LAYER_2)
    {
      return inode_allocate_direct (disk_inode, LAYER_0) &&\
             inode_allocate_indirect (disk_inode, LAYER_1 - LAYER_0) &&\
             inode_allocate_indirect_double (disk_inode, sectors-LAYER_1);
      // return false;
    }
  else
    {
      return false;
    }
}

bool
inode_allocate_direct (struct inode_disk * disk_inode, off_t sectors)
{
  // printf ("alloc_1\n");

  static char zeros[BLOCK_SECTOR_SIZE];
  for (size_t i = 0; i < sectors; i++)
    {
      if (disk_inode->direct_blocks[i] != 0)
        {
          continue;
        }

      if (!free_map_allocate (1, &disk_inode->direct_blocks[i]))
        {
          return false;
        }
      buffer_cache_write (fs_device, disk_inode->direct_blocks[i], zeros);
    }
  return true;
}

bool
inode_allocate_indirect (struct inode_disk *disk_inode, off_t sectors)
{
  // printf ("sectors is %d\n");
  // printf ("alloc_2\n");
  static char zeros[BLOCK_SECTOR_SIZE] = {0};
  // if (!free_map_allocate (1, &disk_inode->indirect_pointer))
  //   {
  //     PANIC ("inode_allocate_indirect failed\n");
  //     return false;
  //   }

  bool success;
  if (disk_inode->indirect_pointer == 0)
    {
      success = free_map_allocate (1, &disk_inode->indirect_pointer);
      if (!success)
        {
          PANIC ("inode_allocate_indirect failed\n");
          return success;
        }
      buffer_cache_write (fs_device, disk_inode->indirect_pointer, zeros);
    }
  
  struct indirect_inode_disk iid;
  buffer_cache_read (fs_device, disk_inode->indirect_pointer, &iid);



  for (int i = 0; i < sectors; i++)
    {
      // if (!free_map_allocate (1, &iid.blocks[i]))
      //   {
      //     return false;
      //   }
      // buffer_cache_write (fs_device, iid.blocks[i], zeros);

      /* realloc */
      if (iid.blocks[i] != 0)
        {
          continue;
        }
      /* realloc */
      if (!free_map_allocate (1, &iid.blocks[i]))
        {
          return false;
        }
      buffer_cache_write (fs_device, iid.blocks[i], zeros);
    }
  for (size_t i = 0; i < sectors; i++)
    {
      // printf ("%d is initialized\n", iid.blocks[i]);
    }
  buffer_cache_write (fs_device, disk_inode->indirect_pointer, &iid);

  return true;
}

bool
inode_allocate_indirect_double (struct inode_disk * disk_inode, off_t sectors)
{
  // printf ("alloc_3\n");
  static char zeros[BLOCK_SECTOR_SIZE];
  bool success;
  // if (!free_map_allocate (1, &disk_inode->double_indirect_pointer))
  //   {
  //     PANIC ("inode_allocate_indirect failed\n");
  //     return false;
  //   }
  // buffer_cache_write (fs_device, disk_inode->indirect_pointer, zeros);

  /* realloc */
  if (disk_inode->double_indirect_pointer == 0)
    {
      // printf ("reach here when init\n");
      success = free_map_allocate (1, &disk_inode->double_indirect_pointer);
      if (!success)
        {
          PANIC ("inode_allocate_double_indirect failed\n");
          return success;
        }
      buffer_cache_write (fs_device, disk_inode->double_indirect_pointer, zeros);
    }
  /* realloc */

  struct indirect_inode_disk double_iid;
  buffer_cache_read (fs_device, disk_inode->double_indirect_pointer, &double_iid);
  int index = 0;

  while (sectors > 0)
    {
      off_t allocate_size = sectors > INDIRECT_BN ? INDIRECT_BN : sectors;
      // if (!free_map_allocate (1, &double_iid.blocks[index]))
      //   {
      //     return false;
      //   }
      if (double_iid.blocks[index] == 0)
        {
          success = free_map_allocate (1, &double_iid.blocks[index]);
          if (!success)
            {
              PANIC ("inode_allocate_double_indirect failed\n");
              return success;
            }
          buffer_cache_write (fs_device, double_iid.blocks[index], zeros);
        }
      struct indirect_inode_disk iid;
      buffer_cache_read (fs_device, double_iid.blocks[index], &iid);

      // printf ("allocate is %d\n", allocate_size);

      for (int i = 0; i < allocate_size; i++)
        {
          /* realloc */
          if (iid.blocks[i] != 0)
          {
            continue;
          }
          /* realloc */
          if (!free_map_allocate (1, &iid.blocks[i]))
            {
              return false;
            }
          buffer_cache_write (fs_device, iid.blocks[i], zeros);
        }
      buffer_cache_write (fs_device, double_iid.blocks[index], &iid);

      sectors -= allocate_size;
      index++;
    }
  buffer_cache_write (fs_device, disk_inode->double_indirect_pointer, &double_iid);

  // struct indirect_inode_disk iid;

  // for (size_t i = 0; i < sectors; i++)
  //   {
  //     if (!free_map_allocate (1, &iid.blocks[i]))
  //       {
  //         return false;
  //       }
  //     buffer_cache_write (fs_device, iid.blocks[i], zeros);
  //   }
  // for (size_t i = 0; i < sectors; i++)
  //   {
  //     // printf ("%d is initialized\n", iid.blocks[i]);
  //   }
  // buffer_cache_write (fs_device, disk_inode->indirect_pointer, &iid);

  return true;
}

bool
inode_deallocate (struct inode *inode)
{
  if (inode->data.length < 0)
    {
      return false;
    }
  int sectors = bytes_to_sectors (inode->data.length);
  if (sectors <= LAYER_0)
    {
      for (int i = 0; i < sectors; i++)
        {
          free_map_release (inode->data.direct_blocks[i], 1);
        }
    }
  else if (sectors <= LAYER_1)
    {
      for (int i = 0; i < LAYER_0; i++)
        {
          free_map_release (inode->data.direct_blocks[i], 1);
        }
      
      struct indirect_inode_disk iid;
      buffer_cache_read (fs_device, inode->data.indirect_pointer, &iid);
      for (int i = 0; i < sectors-LAYER_1; i++)
        {
          free_map_release (iid.blocks[i], 1);
        }
      free_map_release (inode->data.direct_blocks, 1);
      

    }
  else if (sectors <= LAYER_2)
    {
      for (int i = 0; i < LAYER_0; i++)
        {
          free_map_release (inode->data.direct_blocks[i], 1);
        }
      
      struct indirect_inode_disk iid;
      buffer_cache_read (fs_device, inode->data.indirect_pointer, &iid);
      for (int i = 0; i < sectors-LAYER_1; i++)
        {
          free_map_release (iid.blocks[i], 1);
        }
      free_map_release (inode->data.direct_blocks, 1);

      struct indirect_inode_disk double_iid;
      buffer_cache_read (fs_device, inode->data.double_indirect_pointer, &double_iid);
      int temp = sectors - LAYER_2;
      int index = 0;
      while (temp > 0)
        {
          int deallocate_size = sectors > INDIRECT_BN? INDIRECT_BN: sectors;
          buffer_cache_read (fs_device, double_iid.blocks[index], &iid);
          for (int i = 0;i< deallocate_size; i++)
            {
              free_map_release (iid.blocks[i], 1);
            }
          free_map_release (double_iid.blocks[index], 1);
          
          temp -= deallocate_size;
          index++;
        }
      free_map_release (inode->data.double_indirect_pointer, 1);
    }
  else
    {
      printf ("Something went wrong\n");
      return false;
    }
  return true;
}

bool
inode_is_dir (const struct inode *inode)
{
  // printf ("inode is %p, azhe is %d\n", inode, inode->data.inode_type);
  return inode->data.inode_type == DIR;
}