#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

void process_activate (void);

void syscall_halt ();
void syscall_exit (int status);
pid syscall_exec (const char *cmd_line);
int syscall_wait (pid pid);
bool syscall_create (const char *file, unsigned initial_size);
bool syscall_remove (const char *file);
int syscall_open (const char *file);
int syscall_filesize (int fd);
int syscall_read (int fd, void *buffer, unsigned size);
int syscall_write (int fd, const void *buffer, unsigned size);
void syscall_seek (int fd, unsigned position);
unsigned syscall_tell (int fd);
void close (int fd);

void syscall_check_addr (void *ptr, struct intr_frame *f UNUSED);
void syscall_check_buffer (void *ptr, struct intr_frame *f UNUSED, unsigned size);

#endif /* userprog/process.h */
