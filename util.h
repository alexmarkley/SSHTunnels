/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * util.h
 *     - Some useful functions.
 *
 * Alex Markley; All Rights Reserved
 */

//Only process this header once.
#ifndef __SSHTUNNELS_UTIL_H

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

#define STRING_BUFFER_ALLOCSTEP 1024

ssize_t write_all(int fd, const void *buf, size_t count);
ssize_t read_all(int fd, void *buf, size_t count);
int stdpipes_create(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr);
int stdpipes_close_far_end_parent(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr);
int stdpipes_close_far_end_child(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr);
int stdpipes_replace(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr);
int stdpipes_close_remaining(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr);
int fd_set_nonblock(int fd);

#define __SSHTUNNELS_UTIL_H
#endif


