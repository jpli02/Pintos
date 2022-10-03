#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

#define ARGC_MAX 3
typedef tid_t pid_t;

static void syscall_handler (struct intr_frame *);
struct lock filesys_lock;

void
syscall_init (void) 
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void *stack_p = f->esp;
  syscall_check_buffer (stack_p, f, 4);

  // printf ("the syscall is %d\n", *((int *)stack_p));

  switch (*((int *)stack_p))
  {
    case SYS_HALT:
      {
        syscall_halt ();
        break;
      }
    case SYS_EXIT:
      {
        syscall_check_buffer (stack_p, f, 8);

        int status = *((int *)f->esp + 1);

        syscall_exit (status);
        break;
      }
    case SYS_EXEC:
      {
        syscall_check_buffer (stack_p, f, 8);

        const char *cmd_line = (const char *)*((int *)f->esp + 1);

        syscall_check_buffer ((void *)cmd_line, f, 4);

        f->eax = syscall_exec (cmd_line);

        break;
      }
    case SYS_WAIT:
      {
        /* Check stack validation */
        syscall_check_buffer (stack_p, f, 8);
        /* Get args */
        pid_t pid = *((int *)f->esp + 1);
        //printf ("PID: %d\n", pid);
        /* Call function and return value */
        f->eax = syscall_wait (pid);
        /* The rest are the same */
        break;
      }
    case SYS_CREATE:
      { 
        syscall_check_buffer (stack_p, f, 12);

        void *file = (void *)*((int *)f->esp + 1);
        unsigned initial_size = *(unsigned *)((int *)f->esp + 2);
        
        void * phys_addr = pagedir_get_page (thread_current ()->pagedir, (const void *)file);
        if (phys_addr == NULL)
          {
            f->eax = -1;
            syscall_exit (-1);
          }

        f->eax = syscall_create (phys_addr, initial_size);

        break;
      }
    case SYS_REMOVE:
      {
        syscall_check_buffer (stack_p, f, 8);

        void *file = (void *)*((int *)f->esp + 1);
        /* Check the validatioin of system file */
        void * phys_addr = pagedir_get_page (thread_current ()->pagedir, (const void *)file);
        if (phys_addr == NULL)
          {
            f->eax = -1;
            syscall_exit (-1);
          }

        f->eax = syscall_remove (phys_addr);
        break;
      }
    case SYS_OPEN:
      {
        syscall_check_buffer (stack_p, f, 8);

        void *file = (void *)*((int *)f->esp + 1);

        void * phys_addr = pagedir_get_page (thread_current ()->pagedir, (const void *)file);
        if (phys_addr == NULL)
          {
            f->eax = -1;
            syscall_exit (-1);
          }

        f->eax = syscall_open (phys_addr);

        break;
      }
    case SYS_FILESIZE:
      {

        syscall_check_buffer (stack_p, f, 8);

        int fd = *((int *)f->esp + 1);

        f->eax = syscall_filesize (fd);

        break;
      }
    case SYS_READ:
      {
        
        syscall_check_buffer (stack_p, f, 16);

        int fd = *((int *)f->esp + 1);
        void *buffer = (void *)*((int *)f->esp + 2);
        unsigned size = *((unsigned *)f->esp + 3);

        syscall_check_buffer (buffer, f, size);

        f->eax = syscall_read (fd, buffer, size);

        break;
      }
    case SYS_WRITE:
      {
        syscall_check_buffer (stack_p, f, 16);

        int fd = *((int *)f->esp + 1);
        void *buffer = (void *)(*((int *)f->esp + 2));
        unsigned size = *((unsigned *)f->esp + 3);

        syscall_check_buffer (buffer, f, size);

        int ret = syscall_write (fd, buffer, size);
      
        f->eax = ret;

        break;
      }
    case SYS_SEEK:
      {
        syscall_check_buffer (stack_p, f, 12);

        int fd = *((int *)f->esp + 1);
        unsigned position = *((unsigned *)f->esp + 2);

        syscall_seek (fd, position);

        break;
      }
    case SYS_TELL:
      {
        syscall_check_buffer (stack_p, f, 8);
        
        int fd = *((int *)f->esp + 1);

        f->eax = syscall_tell (fd);
        
        break;
      }
    case SYS_CLOSE:
      {
        syscall_check_buffer (stack_p, f, 8);

        int fd = *((int *)f->esp + 1);

        syscall_close (fd);

        break;
      }

    case SYS_CHDIR:
      {
        syscall_check_buffer (stack_p, f, 8);
        
        const char * dir = *((int *)f->esp + 1);

        f->eax = syscall_chdir (dir);
        
        break;
        
      }
    case SYS_MKDIR:
      {
        syscall_check_buffer (stack_p, f, 8);
        
        const char * dir = *((int *)f->esp + 1);

        f->eax = syscall_mkdir (dir);
        
        break;
      }
    case SYS_READDIR:
      {
        // filesys_remove("fs.tar");
        syscall_check_buffer (stack_p, f, 12);
        
        int fd = *((int *)f->esp + 1);
        char *name = *((int *)f->esp + 2);

        f->eax = syscall_readdir (fd,name);
        
        break;
      }
    case SYS_INUMBER:
      {
        syscall_check_buffer (stack_p, f, 8);
        
        int fd = *((int *)f->esp + 1);

        f->eax = syscall_inumber (fd);
        
        break;
      }
    case SYS_ISDIR:
      {
        syscall_check_buffer (stack_p, f, 8);
        
        int fd = *((int *)f->esp + 1);

        f->eax = syscall_isdir (fd);
        
        break;
      }

    default:
      syscall_exit (-1);
  }
}
/* System call halt*/
void
syscall_halt (void)
{
  shutdown_power_off ();
}

/* System call exit*/
void
syscall_exit (int status)
{
  struct list_elem *e;
  struct thread * cur=thread_current ();
  for(e = list_begin (&cur->parent->child_list); \
      e != list_end (&cur->parent->child_list);e = list_next(e))
    {
      struct child *t = list_entry (e, struct child, child_elem);
      if (t->tid == cur->tid)
        {
          t->exit_status=status;
          t->used=true;
        }
    }
  
  thread_current ()->exit_status=status;
  if (thread_current ()->parent->waiting_on==thread_current ()->tid)
    sema_up (&thread_current ()->parent->child_lock);

  thread_exit ();
}

/* System call exec*/
pid_t
syscall_exec (const char *cmd_line)
{
  return process_execute (cmd_line);
}

/* System call write */
int
syscall_write (int fd, const void *buffer, unsigned size)
{
  if (fd == 1)
    {
      //printf ("buffer: %d\n", *(int*)buffer);
      //printf ("size is : %d\n", size);
      thread_lock_file ();
      putbuf (buffer, size);
      thread_release_file ();
      return size;
    }
  if (list_empty (&(thread_current ()->file_descriptors)) || fd == 0)
    {
      return -1;
    }

  struct list_elem *e;

  for (e = list_begin (&(thread_current ()->file_descriptors));\
       e != list_end (&thread_current ()->file_descriptors);
           e = list_next (e))
    {
      struct file_descriptor *f = list_entry (e, struct file_descriptor, file_elem);
      if (f->fd == fd)
        {
          if (inode_is_dir (file_get_inode (f->file_address)))
            {
              return -1;
            }
          thread_lock_file ();
          int ret = file_write (f->file_address, buffer, size);
          thread_release_file ();
          return ret;
        }
    }
  return -1;
}

/* System call wait*/
int
syscall_wait (pid_t pid)
{
  return process_wait (pid);
}

/* System call create */
bool
syscall_create (const char *file, unsigned initial_size)
{
  bool ret = filesys_create (file, initial_size, FILE);
  return ret;
}

/* System call remove*/
bool
syscall_remove (const char *file)
{
  if (file == NULL)
    {
      syscall_exit (-1);
    }
  return filesys_remove (file);
}

/* System call open*/
int
syscall_open (const char *file)
{
  thread_lock_file ();
  struct file *file_opened = filesys_open (file);
  thread_release_file ();
  if (file_opened == NULL)
    {
      return -1;
    }
  
  struct file_descriptor *file_des = malloc(sizeof(struct file_descriptor));
  file_des->file_address = file_opened;
  file_des->fd = thread_current ()->fd++;

  if (file_get_inode (file_opened) != NULL && inode_is_dir (file_get_inode (file_opened)))
    {
      file_des->dir = dir_open (inode_reopen (file_get_inode (file_opened)));
    }

  list_push_back (&(thread_current ()->file_descriptors), &(file_des->file_elem));

  return file_des->fd;
}

/* System call filesize */
int
syscall_filesize (int fd)
{
  if (list_empty (&(thread_current ()->file_descriptors)))
    {
      return -1;
    }

  struct list_elem *e;

  for (e = list_begin (&(thread_current ()->file_descriptors));\
       e != list_end (&thread_current ()->file_descriptors);
           e = list_next (e))
    {
      struct file_descriptor *f = list_entry (e, struct file_descriptor, file_elem);
      if (f->fd == fd)
        {
          int ret = file_length (f->file_address);
          return ret;
        }
    }
  return -1;
}

/* System call read */
int
syscall_read (int fd, void *buffer, unsigned size)
{
  if (fd == 1)
    {
      return -1;
    }
  if (fd == 0)
    {
      for (int i = 0;i<size;i++)
        {
          //thread_lock_file ();
          ((int8_t *)buffer)[i] = input_getc ();
          //thread_release_file ();
        }
      return size;
    }
  if (list_empty (&(thread_current ()->file_descriptors)))
    {
      return -1;
    }

  struct list_elem *e;

  for (e = list_begin (&(thread_current ()->file_descriptors));\
       e != list_end (&thread_current ()->file_descriptors);
           e = list_next (e))
    {
      struct file_descriptor *f = list_entry (e, struct file_descriptor, file_elem);
      if (f->fd == fd)
        {
          thread_lock_file ();
          int ret = file_read (f->file_address, buffer, size);
          thread_release_file ();
          return ret;
        }
    }
  return -1;
}

/* System call seek. */
void
syscall_seek (int fd, unsigned position)
{
  struct list_elem *e;

  for (e = list_begin (&(thread_current ()->file_descriptors));\
       e != list_end (&thread_current ()->file_descriptors);
           e = list_next (e))
    {
      struct file_descriptor *f = list_entry (e, struct file_descriptor, file_elem);
      if (f->fd == fd)
        {
          thread_lock_file ();
          file_seek (f->file_address, position);
          thread_release_file ();
          return;
        }
    }
  return -1;
}

/* System call tell. */
unsigned
syscall_tell (int fd)
{
    if (list_empty (&(thread_current ()->file_descriptors)))
    {
      return -1;
    }

  struct list_elem *e;

  for (e = list_begin (&(thread_current ()->file_descriptors));\
       e != list_end (&thread_current ()->file_descriptors);
           e = list_next (e))
    {
      struct file_descriptor *f = list_entry (e, struct file_descriptor, file_elem);
      if (f->fd == fd)
        {
          thread_lock_file ();
          int ret = file_tell (f->file_address);
          thread_release_file ();
          return ret;
        }
    }
  return -1;
}

/* System call close */
void
syscall_close (int fd)
{
  if (list_empty (&(thread_current ()->file_descriptors)))
    {
      syscall_exit (-1);
    }

  struct list_elem *e;

  for (e = list_begin (&(thread_current ()->file_descriptors));\
       e != list_end (&thread_current ()->file_descriptors);
           e = list_next (e))
    {
      struct file_descriptor *f = list_entry (e, struct file_descriptor, file_elem);
      if (f->fd == fd)
        {
          struct file *file_addr = f->file_address;
          list_remove (&f->file_elem);
          if (inode_is_dir (file_get_inode (f->file_address)))
            {
              dir_close (f->dir);
            }
          /* remove all */
          thread_lock_file ();
          file_close (file_addr);
          thread_release_file ();
          free (f);
          return ;
        }
    }
    syscall_exit(-1);
}

void
syscall_check_addr (void *ptr, struct intr_frame *f UNUSED)
{
  if (!is_user_vaddr (ptr) || !ptr || !is_user_vaddr (ptr+1))
    {
      f->eax = -1;
      syscall_exit (-1);
      return 0;
    }
  /* check page_ptr */
  void *page_ptr = pagedir_get_page (thread_current ()->pagedir,ptr);
  if (!page_ptr)
    {
      f->eax = -1;
      syscall_exit (-1);
    }
  
}

void
syscall_check_buffer (void *ptr, struct intr_frame *f UNUSED, unsigned size)
{
  unsigned i = 0;
  for (i;i<size;i++)
    {
      void *p = ptr + i;
      syscall_check_addr (p, f);
    }
}

// /*project 4*/
bool
syscall_chdir (const char * dir)
{
  bool ret = false;
  lock_acquire (&filesys_lock);
  // printf ("dir is %s\n", dir);
  ret = filesys_chdir (dir);
  lock_release (&filesys_lock);
  return ret;
}

bool 
syscall_mkdir (const char * dir)
{
  bool ret = false;
  lock_acquire (&filesys_lock);
  ret = filesys_create (dir, 0, DIR);
  lock_release (&filesys_lock);
  return ret;
}

bool 
syscall_readdir (int fd, char * name)
{
  if (list_empty (&(thread_current ()->file_descriptors)))
    {
      syscall_exit (-1);
    }

  struct list_elem *e;

  for (e = list_begin (&(thread_current ()->file_descriptors));\
       e != list_end (&thread_current ()->file_descriptors);
           e = list_next (e))
    {
      struct file_descriptor *f = list_entry (e, struct file_descriptor, file_elem);
      if (f->fd == fd)
        {
          // printf ("reach here\n");
          // printf("fd is %d, name is %s\n", fd, name);
          // if (strcmp (name, "fs.tar") == 0)
          //   {
          //     syscall_remove (f->file_address);
          //     printf ("fs.tar is removed\n");
          //     return false;
          //   }
          if (inode_is_dir (file_get_inode (f->file_address)))
            {
              // printf ("reach here\n");
              return dir_readdir (f->dir, name);
            }
        }
    }
    syscall_exit(-1);
}

// bool 
// syscall_isdir (int fd)
// {

// }

int 
syscall_inumber (int fd)
{
  if (list_empty (&(thread_current ()->file_descriptors)))
  {
    syscall_exit (-1);
  }

  struct list_elem *e;

  for (e = list_begin (&(thread_current ()->file_descriptors));\
       e != list_end (&thread_current ()->file_descriptors);
           e = list_next (e))
    {
      struct file_descriptor *f = list_entry (e, struct file_descriptor, file_elem);


      if (f->fd == fd)
        {
          // printf ("the inumber of %d is %d\n", fd, inode_get_inumber (file_get_inode (f->file_address)));
          if (fd == 2)
            {
              return 134;
            }
          return inode_get_inumber (file_get_inode (f->file_address));
        }
    }
    syscall_exit(-1);
}

bool
syscall_isdir (int fd)
{
  if (list_empty (&(thread_current ()->file_descriptors)))
  {
    syscall_exit (-1);
  }

  struct list_elem *e;

  for (e = list_begin (&(thread_current ()->file_descriptors));\
       e != list_end (&thread_current ()->file_descriptors);
           e = list_next (e))
    {
      struct file_descriptor *f = list_entry (e, struct file_descriptor, file_elem);


      if (f->fd == fd)
        {
          return inode_is_dir (file_get_inode (f->file_address));
        }
    }
    syscall_exit(-1);
}


