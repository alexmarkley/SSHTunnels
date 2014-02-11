/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * util.h
 *     - Some useful functions.
 *
 * Alex Markley; All Rights Reserved
 */

#include "util.h"
#include "main.h"
#include "log.h"

//Behavior identical to the write() function, except that it will try very hard to write count bytes.
ssize_t write_all(int fd, const void *buf, size_t count)
	{
	const void *buftmp;
	size_t bufpos = 0;
	ssize_t wrote;
	
	while((count - bufpos) > 0)
		{
		buftmp = buf + bufpos;
		wrote = write(fd, buftmp, count - bufpos);
		if(wrote == 0)
			{
			logline(LOG_ERROR, "write_all: write() apparently wrote zero bytes. errno probably was not set. Setting errno to EAGAIN and returning failure.");
			errno = EAGAIN;
			return -1;
			}
		else if(wrote < 0)
			{
			//Just return errors.
			return wrote;
			}
		//Wrote at least one byte. Proceed.
		bufpos = bufpos + wrote;
		}
	
	return bufpos;
	}

//Behavior identical to the read() function, except that it will try very hard to read count bytes.
ssize_t read_all(int fd, void *buf, size_t count)
	{
	void *buftmp;
	size_t bufpos = 0;
	ssize_t readret;
	
	while((count - bufpos) > 0)
		{
		buftmp = buf + bufpos;
		readret = read(fd, buftmp, count - bufpos);
		if(readret < 1) //read() returns -1 on errors and 0 on things like EOF.
			return readret;
		//Read at least one byte. Proceed.
		bufpos = bufpos + readret;
		}
	
	return bufpos;
	}

//Sets up a total of three pipes for STDIN, STDOUT, and STDERR.
//This is so that a new child process's standard I/O can be captured by the parent process.
//Returns TRUE on success or FALSE on error.
int stdpipes_create(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr)
	{
	if(pipe(pipe_stdin) < 0)
		{
		logline(LOG_ERROR, "stdpipes_create: Call to pipe() for stdin failed! (%s)", strerror(errno));
		return FALSE;
		}
	if(pipe(pipe_stdout) < 0)
		{
		logline(LOG_ERROR, "stdpipes_create: Call to pipe() for stdout failed! (%s)", strerror(errno));
		return FALSE;
		}
	if(pipe(pipe_stderr) < 0)
		{
		logline(LOG_ERROR, "stdpipes_create: Call to pipe() for stderr failed! (%s)", strerror(errno));
		return FALSE;
		}
	return TRUE;
	}

//Closes the far ends of the standard pipes. For parent process.
//Returns TRUE on success or FALSE on error.
int stdpipes_close_far_end_parent(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr)
	{
	if(close(pipe_stdin[PIPE_READ]) < 0)
		{
		logline(LOG_ERROR, "stdpipes_close_far_end_parent: Call to close() for stdin read failed! (%s)", strerror(errno));
		return FALSE;
		}
	pipe_stdin[PIPE_READ] = -1;
	if(close(pipe_stdout[PIPE_WRITE]) < 0)
		{
		logline(LOG_ERROR, "stdpipes_close_far_end_parent: Call to close() for stdout write failed! (%s)", strerror(errno));
		return FALSE;
		}
	pipe_stdout[PIPE_WRITE] = -1;
	if(close(pipe_stderr[PIPE_WRITE]) < 0)
		{
		logline(LOG_ERROR, "stdpipes_close_far_end_parent: Call to close() for stderr write failed! (%s)", strerror(errno));
		return FALSE;
		}
	pipe_stderr[PIPE_WRITE] = -1;
	return TRUE;
	}

//Closes the far ends of the standard pipes. For child process.
//Returns TRUE on success or FALSE on error.
int stdpipes_close_far_end_child(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr)
	{
	if(close(pipe_stdin[PIPE_WRITE]) < 0)
		{
		logline(LOG_ERROR, "stdpipes_close_far_end_child: Call to close() for stdin write failed! (%s)", strerror(errno));
		return FALSE;
		}
	pipe_stdin[PIPE_WRITE] = -1;
	if(close(pipe_stdout[PIPE_READ]) < 0)
		{
		logline(LOG_ERROR, "stdpipes_close_far_end_child: Call to close() for stdout read failed! (%s)", strerror(errno));
		return FALSE;
		}
	pipe_stdout[PIPE_READ] = -1;
	if(close(pipe_stderr[PIPE_READ]) < 0)
		{
		logline(LOG_ERROR, "stdpipes_close_far_end_child: Call to close() for stderr read failed! (%s)", strerror(errno));
		return FALSE;
		}
	pipe_stderr[PIPE_READ] = -1;
	return TRUE;
	}

//Closes original STDIN, STDOUT, and STDERR and replaces them with the previously opened pipes.
//It might go without saying, but this should only be called in the child process, usually before a call to execve()
//Returns TRUE on success or FALSE on error.
int stdpipes_replace(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr)
	{
	if(dup2(pipe_stdin[PIPE_READ], STDIN_FILENO) < 0)
		{
		logline(LOG_ERROR, "stdpipes_replace: Call to dup2() for stdin failed! (%s)", strerror(errno));
		return FALSE;
		}
	if(dup2(pipe_stdout[PIPE_WRITE], STDOUT_FILENO) < 0)
		{
		logline(LOG_ERROR, "stdpipes_replace: Call to dup2() for stdout failed! (%s)", strerror(errno));
		return FALSE;
		}
	if(dup2(pipe_stderr[PIPE_WRITE], STDERR_FILENO) < 0)
		{
		logline(LOG_ERROR, "stdpipes_replace: Call to dup2() for stderr failed! (%s)", strerror(errno));
		return FALSE;
		}
	return TRUE;
	}

//Closes any remaining open pipes from the standard pipes.
//Returns TRUE on success or FALSE on error.
int stdpipes_close_remaining(int *pipe_stdin, int *pipe_stdout, int *pipe_stderr)
	{
	int *pipes[6], i;
	pipes[0] = &pipe_stdin[PIPE_READ];
	pipes[1] = &pipe_stdin[PIPE_WRITE];
	pipes[2] = &pipe_stdout[PIPE_READ];
	pipes[3] = &pipe_stdout[PIPE_WRITE];
	pipes[4] = &pipe_stderr[PIPE_READ];
	pipes[5] = &pipe_stderr[PIPE_WRITE];
	
	for(i = 0; i < 6; i++)
		{
		if(*pipes[i] != -1)
			{
			if(close(*pipes[i]) < 0)
				{
				logline(LOG_ERROR, "stdpipes_close_remaining: close() failed! (%s)", strerror(errno));
				return FALSE;
				}
			*pipes[i] = -1;
			}
		}
	
	return TRUE;
	}

//Sets a file descriptor's non-blocking flag.
//Returns TRUE on success or FALSE on error.
int fd_set_nonblock(int fd)
	{
	int pipe_flags;
	pipe_flags = fcntl(fd, F_GETFL, 0);
	if(fcntl(fd, F_SETFL, pipe_flags | O_NONBLOCK) < 0)
		{
		logline(LOG_ERROR, "fd_set_nonblock: Call to fcntl() failed! (%s)", strerror(errno));
		return FALSE;
		}
	return TRUE;
	}

