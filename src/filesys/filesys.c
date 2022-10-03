#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  // filesys_remove ("fs.tar");
  fsutil_ls ();
  buffer_cache_close ();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *path, off_t initial_size, int type) 
{
  // printf ("name: %s\n", name);
  
  const char *name = dir_get_filename (path);
  struct dir *dir = dir_get_directory (path);

  // printf ("path is %s, filename is %s, address of dir is %p\n", path, name, dir);

  block_sector_t inode_sector = 0;
  // if (type == DIR && name == NULL)
  //   {
  //     return false;
  //   }
  // struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, type)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  // printf ("%s is added at sector inode sector %d\n", name, inode_sector);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *path)
{
  // printf ("path is %s\n", path);
  // struct dir *dir = dir_open_root ();
  if (strlen (path) == 0)
    {
      return NULL;
    }
  struct dir *dir = dir_get_directory (path);
  const char *name = dir_get_filename (path);
  struct inode *inode = NULL;

  // printf ("name is %s\n", name);

  if (dir == NULL)
    {
      return NULL;
    }

  if (strlen(name) == 0 || strcmp(name, ".") == 0)
    {
      // inode = dir_get_inode (dir);
      return (struct file *) dir;
    }
  else if (strlen(name) != 0)
    {
      dir_lookup (dir, name, &inode);
      // ASSERT (inode != NULL);
    }
  // else
  //   {
  //     PANIC ("NO!!\n");
  //   }

  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *path) 
{
  // printf ("%s is removed\n", name);
  // struct dir *dir = dir_open_root ();
  struct dir *dir = dir_get_directory (path);
  const char *name = dir_get_filename (path);
  struct inode *inode = NULL;

  // printf ("name is %s\n", name);

  if (dir == NULL)
    {
      return NULL;
    }

  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

bool
filesys_chdir (const char * path)
{
  // printf ("path is %s\n", path);
  struct dir *dir = dir_get_directory (path);
  char *name = dir_get_filename (path);
  struct inode *inode;

  // printf ("name is %s\n", name);

  if (dir == NULL)
    {
      free (name);
      return false;
    }
  if (strcmp (name, ".") == 0 || strlen (name) == 0)
    {
      // printf ("here\n");
      thread_current()->cwd = dir;
      free (name);
      return true;
    }
  else if (strcmp (name, "..") == 0)
    {
      PANIC ("NOT FINISHED\n");
    }
  else
    {
      dir_lookup (dir, name, &inode);
    }
  
  dir_close (dir);
  dir = dir_open (inode);
  if (dir == NULL)
    {
      free (name);
      return false;
    }
  dir_close (thread_current()->cwd);
  thread_current()->cwd = dir;
  free (name);
  return true;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
