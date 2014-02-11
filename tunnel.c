/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * tunnel.c
 *     - Handles tunnels.
 *
 * Alex Markley; All Rights Reserved
 */

#include "tunnel.h"
#include "main.h"
#include "log.h"
#include "util.h"

#define TUNNEL_MODULE "tunnel %d: "

struct tunnel *tunnel_create(char **argv, char **envp)
	{
	static int nextid = 1;
	struct tunnel *newtun = NULL;
	
	if((newtun = (struct tunnel *)calloc(1, sizeof(struct tunnel))) == NULL)
		{
		logline(LOG_ERROR, TUNNEL_MODULE "out of memory!", nextid);
		return NULL;
		}
	
	//Initialize variables.
	newtun->id = nextid;
	newtun->argv = argv;
	newtun->envp = envp;
	newtun->pid = 0;
	newtun->pid_launched = 0;
	newtun->pipe_stdin[PIPE_READ] = -1;
	newtun->pipe_stdin[PIPE_WRITE] = -1;
	newtun->pipe_stdout[PIPE_READ] = -1;
	newtun->pipe_stdout[PIPE_WRITE] = -1;
	newtun->pipe_stderr[PIPE_READ] = -1;
	newtun->pipe_stderr[PIPE_WRITE] = -1;
	newtun->uptoken = -1;
	newtun->uptoken_sent = 0;
	
	logline(LOG_INFO, TUNNEL_MODULE "Created tunnel object.", newtun->id);
	
	nextid++;
	return newtun;
	}

int tunnel_maintenance(struct tunnel *tun)
	{
	int tunnel_status;
	pid_t waitpid_return;
	char logline_prefix[128];
	time_t now;
	now = time(NULL);
	
	logline(LOG_INFO, TUNNEL_MODULE "Maintenance loop.", tun->id);
	
	//No PID? (yet?)
	if(!tun->pid)
		{
		//Make sure we're not launching too quickly.
		if(now >= (tun->pid_launched + TUNNEL_LAUNCHTHROTTLE))
			{
			if(!tunnel_process_launch(tun))
				{
				logline(LOG_ERROR, TUNNEL_MODULE "tunnel_process_launch() failed!", tun->id);
				return FALSE;
				}
			tun->pid_launched = now;
			}
		}
	
	//Check STDERR for any messages from the child we need to report.
	if(tun->pipe_stdout[PIPE_READ] != -1)
		{
		sprintf(logline_prefix, TUNNEL_MODULE "stdout: ", tun->id);
		tunnel_check_stderr(tun->pipe_stdout[PIPE_READ], logline_prefix);
		}
	if(tun->pipe_stderr[PIPE_READ] != -1)
		{
		sprintf(logline_prefix, TUNNEL_MODULE "stderr: ", tun->id);
		tunnel_check_stderr(tun->pipe_stderr[PIPE_READ], logline_prefix);
		}
	
	//We DO have a PID.
	if(tun->pid)
		{
		//Poll (non-blocking) for this tunnel's child process exit status.
		waitpid_return = waitpid(tun->pid, &tunnel_status, WNOHANG);
		if(waitpid_return < 0)
			{
			logline(LOG_ERROR, TUNNEL_MODULE "waitpid() returned an error!", tun->id);
			return FALSE;
			}
		else if(waitpid_return == tun->pid)
			{
			logline(LOG_INFO, TUNNEL_MODULE "waitpid() says the child exited with status %d", tun->id, WEXITSTATUS(tunnel_status));
			tun->pid = 0;
			if(!stdpipes_close_remaining(tun->pipe_stdin, tun->pipe_stdout, tun->pipe_stderr))
				{
				logline(LOG_ERROR, TUNNEL_MODULE "stdpipes_close_remaining() returned an error!", tun->id);
				return FALSE;
				}
			}
		}
	
	return TRUE;
	}

int tunnel_destroy(struct tunnel *tun)
	{
	if(tun == NULL)
		return FALSE;
	
	//FIXME FIXME FIXME - Let's make sure the child process and pipes are all cleaned up.
	
	free(tun);
	return TRUE;
	}

int tunnel_process_launch(struct tunnel *tun)
	{
	int i = 0, wrote;
	size_t launchstring_len = 0;
	char *launchstring = NULL, *launchstring_tmp;
	
	//For logging purposes, we'll generate a launchstring.
	while(tun->argv[i] != NULL)
		{
		launchstring_len = launchstring_len + strlen(tun->argv[i]) + 2;
		i++;
		}
	if((launchstring = malloc(launchstring_len)) == NULL)
		{
		logline(LOG_ERROR, TUNNEL_MODULE "out of memory!", tun->id);
		return FALSE;
		}
	i = 0;
	launchstring_tmp = launchstring;
	while(tun->argv[i] != NULL)
		{
		wrote = sprintf(launchstring_tmp, " %s", tun->argv[i]);
		launchstring_tmp = launchstring_tmp + wrote;
		i++;
		}
	//Log it!
	logline(LOG_INFO, TUNNEL_MODULE "Launching child process:%s", tun->id, launchstring);
	free(launchstring);
	
	//Tunnel requires pipes to be set up for tunnel monitoring.
	if(!stdpipes_create(tun->pipe_stdin, tun->pipe_stdout, tun->pipe_stderr))
		{
		logline(LOG_ERROR, TUNNEL_MODULE "Couldn't create pipes.", tun->id);
		return FALSE;
		}
	
	//Fork to generate tunnel child process.
	if((tun->pid = fork()) < 0)
		{
		logline(LOG_ERROR, TUNNEL_MODULE "Call to fork() failed!", tun->id);
		return FALSE;
		}
	
	//Child?
	if(tun->pid == 0)
		{
		//Close the "far" ends of the pipe between the parent and the child.
		if(!stdpipes_close_far_end_child(tun->pipe_stdin, tun->pipe_stdout, tun->pipe_stderr))
			{
			logline(LOG_ERROR, TUNNEL_MODULE "stdpipes_close_far_end_child() returned an error!", tun->id);
			return FALSE;
			}
		
		//In the child process we need to replace the standard pipes.
		if(!stdpipes_replace(tun->pipe_stdin, tun->pipe_stdout, tun->pipe_stderr))
			{
			logline(LOG_ERROR, TUNNEL_MODULE "stdpipes_replace() returned an error!", tun->id);
			return FALSE;
			}
		
		//Exec!
		execve(tun->argv[0], tun->argv, tun->envp);
		
		//execve() only returns on error.
		logline(LOG_ERROR, TUNNEL_MODULE "Call to execve() failed!", tun->id);
		exit(1);
		}	
	
	//Close the "far" ends of the pipe between the parent and the child.
	if(!stdpipes_close_far_end_parent(tun->pipe_stdin, tun->pipe_stdout, tun->pipe_stderr))
		{
		logline(LOG_ERROR, TUNNEL_MODULE "stdpipes_close_far_end_parent() returned an error!", tun->id);
		exit(1);
		}
	
	//On the parent we must set O_NONBLOCK so we can query the pipes from the child without locking up ourselves.
	if(!fd_set_nonblock(tun->pipe_stdout[PIPE_READ]) || !fd_set_nonblock(tun->pipe_stderr[PIPE_READ]))
		{
		logline(LOG_ERROR, TUNNEL_MODULE "fd_set_nonblock() returned an error!", tun->id);
		exit(1);
		}
	
	return TRUE;
	}

int tunnel_check_stderr(int fd, char *logline_prefix)
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
				logline(LOG_ERROR, "out of memory!");
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
				logline(LOG_ERROR, "failed reading pipe! (%s)", strerror(errno));
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
		logline(LOG_ERROR, "out of memory!");
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



