/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * main.c
 *     - Daemon can run all the time.
 *     - Starts, monitors, and restarts SSH tunnels as necessary.
 *
 * Alex Markley; All Rights Reserved
 */

#include "main.h"
#include "util.h"
#include "log.h"

//Checks for and reports error lines in a STDERR pipe.
//Returns TRUE if there were errors, FALSE if there were not, and -1 if there was an error.

int checkTunnelErrors(int fd, char *logline_prefix)
	{
	char *buf = NULL, *buf_temp, *buf_sub;
	size_t buf_len = 0, buf_pos = 0;
	ssize_t readret;
	int done = FALSE, loopa = 0, loopb = 0;
	
	while(!done)
		{
		if(buf_pos >= buf_len)
			{
			//We need to increase the size of the buffer.
			buf_len = buf_len + STRING_BUFFER_ALLOCSTEP;
			if((buf = realloc(buf, buf_len)) == NULL)
				{
				logline(LOG_ERROR, "checkTunnelErrors ran out of memory!");
				free(buf);
				return -1;
				}
			}
		buf_temp = buf + buf_pos;
		readret = read(fd, buf_temp, buf_len - buf_pos);
		if(readret > 0)
			{
			buf_pos = buf_pos + readret;
			}
		else if(readret < 0)
			{
			//Because read() returned an error code, we need to check errno.
			if(errno != EAGAIN && errno != EWOULDBLOCK)
				{
				logline(LOG_ERROR, "checkTunnelErrors failed reading pipe! (%s)", strerror(errno));
				free(buf);
				return -1;
				}
			//If read() returned zero bytes, we are done.
			done = TRUE;
			}
		else //readret is 0
			done = TRUE;
		}
	
	//Did we get anything in the buffer at all?
	if(buf_pos == 0)
		{
		free(buf);
		return FALSE;
		}
	
	//We need moar memories!
	if((buf_sub = malloc(buf_len + 1)) == NULL)
		{
		logline(LOG_ERROR, "checkTunnelErrors ran out of memory!");
		free(buf);
		return -1;
		}
	
	//Scan through the buffer, outputting each line.
	for(loopa = 0; loopa < buf_pos; loopa++)
		{
		//Copy the character over.
		buf_sub[loopb] = buf[loopa];
		loopb++;
		
		//Check for a new line character.
		if(buf[loopa] == '\n')
			{
			buf_sub[loopb] = '\0'; //Null-terminate the string.
			logline(LOG_WARNING, "%s%s", logline_prefix, buf_sub);
			loopb = 0;
			}
		}
	if(loopb > 0)
		{
		buf_sub[loopb] = '\0'; //Null-terminate the string.
		logline(LOG_WARNING, "%s%s", logline_prefix, buf_sub);
		}
	
	free(buf);
	free(buf_sub);
	return TRUE;
	}

int main(int argc, char **argv, char **envp)
	{
	int finished = FALSE, tunnel_status, tunnel_stdin[2], tunnel_stdout[2], tunnel_stderr[2];
	pid_t tunnel_pid = 0, waitpid_return;
	ssize_t wrote;
	char *newargv[] = { "/usr/bin/tee", NULL };
	
	//Tunnel requires pipes to be set up for tunnel monitoring.
	if(!stdpipes_create(tunnel_stdin, tunnel_stdout, tunnel_stderr))
		{
		logline(LOG_ERROR, "stdpipes_create() returned an error!");
		exit(1);
		}
	
	//Fork to generate tunnel child process.
	if((tunnel_pid = fork()) < 0)
		{
		logline(LOG_ERROR, "Call to fork() failed!\n");
		exit(1);
		}
	
	if(tunnel_pid == 0) //Child.
		{
		//Close the "far" ends of the pipe between the parent and the child.
		if(!stdpipes_close_far_end_child(tunnel_stdin, tunnel_stdout, tunnel_stderr))
			{
			logline(LOG_ERROR, "stdpipes_close_far_end_child() returned an error!");
			exit(1);
			}
		
		//In the child process we need to replace the standard pipes.
		if(!stdpipes_replace(tunnel_stdin, tunnel_stdout, tunnel_stderr))
			{
			logline(LOG_ERROR, "stdpipes_replace() returned an error!");
			exit(1);
			}
		
		//Exec!
		execve(newargv[0], newargv, envp);
		
		//execve() only returns on error.
		logline(LOG_ERROR, "Call to execve() failed!\n");
		exit(1);
		}
	else //Parent.
		{
		//Close the "far" ends of the pipe between the parent and the child.
		if(!stdpipes_close_far_end_parent(tunnel_stdin, tunnel_stdout, tunnel_stderr))
			{
			logline(LOG_ERROR, "stdpipes_close_far_end_parent() returned an error!");
			exit(1);
			}
		
		//On the parent we must set O_NONBLOCK so we can query the pipes from the child without locking up ourselves.
		if(!fd_set_nonblock(tunnel_stdout[PIPE_READ]) || !fd_set_nonblock(tunnel_stderr[PIPE_READ]))
			{
			logline(LOG_ERROR, "fd_set_nonblock() returned an error!");
			exit(1);
			}
		
		logline(LOG_INFO, "Parent. Checking for child exit status repeatedly.");
		
		//Main loop repeatedly checks child process statuses.
		while(!finished)
			{
			sleep(1);
			
			wrote = write_all(tunnel_stdin[PIPE_WRITE], "7\n", 2);
			if(wrote < 2)
				{
				if(wrote < 0)
					logline(LOG_ERROR, "Couldn't write() to send monitoring packet! (%s)", strerror(errno));
				else
					logline(LOG_ERROR, "Couldn't write() so send monitoring packet! Only %d/2 bytes written.", wrote);
				}
			
			//Check for (and report) errors in the child process STDERR.
			checkTunnelErrors(tunnel_stderr[PIPE_READ], "tunnel stderr: ");
			checkTunnelErrors(tunnel_stdout[PIPE_READ], "tunnel stdout: ");
			
			//Poll (non-blocking) for this tunnel's child process exit status.
			waitpid_return = waitpid(tunnel_pid, &tunnel_status, WNOHANG);
			if(waitpid_return < 0)
				{
				logline(LOG_ERROR, "waitpid() returned an error!");
				exit(1);
				}
			else if(waitpid_return == tunnel_pid)
				{
				logline(LOG_INFO, "waitpid() says the child exited with status %d", WEXITSTATUS(tunnel_status));
				finished = TRUE;
				}
			}
		}
	
	return 0;
	}

