/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * util.h
 *     - Some useful functions.
 *
 * Copyright (C) 2015 Alex Markley
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */

//Only process this header once.
#ifndef __SSHTUNNELS_UTIL_H

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

#define STRING_BUFFER_ALLOCSTEP 1024
#define LIST_GROW_STEP 8

ssize_t write_all(int fd, const void *buf, size_t count);
ssize_t read_all(int fd, void *buf, size_t count);
int stdpipes_create(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr);
int stdpipes_close_far_end_parent(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr);
int stdpipes_close_far_end_child(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr);
int stdpipes_replace(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr);
int stdpipes_close_remaining(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr);
int fd_set_nonblock(int fd);
void *list_grow_insert(void *ptr, void *new_member, size_t member_size, int *list_len, int *list_pos);

#define __SSHTUNNELS_UTIL_H
#endif


