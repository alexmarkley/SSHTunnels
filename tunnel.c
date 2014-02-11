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

struct tunnel *tunnel_create(char **argv, char **envp, int uptoken_enabled)
	{
	static int nextid = 1;
	struct tunnel *newtun = NULL;
	
	logline(LOG_INFO, TUNNEL_MODULE "Creating tunnel object...", nextid);
	
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
	newtun->uptoken_enabled = uptoken_enabled;
	newtun->uptoken_condemned = FALSE;
	newtun->uptoken = -1;
	newtun->uptoken_sent = 0;
	newtun->trouble = 0;
	newtun->trouble_launchnext = 0;
	
	nextid++;
	return newtun;
	}

int tunnel_maintenance(struct tunnel *tun)
	{
	static int srand_seeded = FALSE;
	int tunnel_status;
	pid_t waitpid_return;
	char logline_prefix[128], uptoken_string[TUNNEL_UPTOKENBUFFERSIZE];
	time_t now, launchdelay_seconds;
	float rnum;
	ssize_t ioret;
	
	now = time(NULL);
	
	if(!srand_seeded)
		{
		srand((unsigned int)now);
		srand_seeded = TRUE;
		}
	
	//logline(LOG_INFO, TUNNEL_MODULE "Maintenance loop.", tun->id);
	
	//No PID? (yet?)
	if(!tun->pid)
		{
		//Make sure we're not launching too quickly.
		if(now >= (tun->trouble_launchnext))
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
	if(tun->pipe_stderr[PIPE_READ] != -1)
		{
		sprintf(logline_prefix, TUNNEL_MODULE "stderr: ", tun->id);
		tunnel_check_stderr(tun->pipe_stderr[PIPE_READ], logline_prefix);
		}
	
	//We DO have a PID. Child process should be running.
	if(tun->pid)
		{
		//Reset trouble counter if the process has run for at least TUNNEL_TROUBLERESETTIME seconds.
		if(tun->trouble > 0 && now > (tun->pid_launched + TUNNEL_TROUBLERESETTIME))
			{
			logline(LOG_INFO, TUNNEL_MODULE "Resetting trouble counter.", tun->id);
			tun->trouble = 0;
			}
		
		//Uptoken stuff gets handled here too.
		if(tun->uptoken_enabled && !tun->uptoken_condemned && tun->pipe_stdin[PIPE_WRITE] >= 0 && tun->pipe_stdout[PIPE_READ] >= 0)
			{
			if(tun->uptoken < 0) //We have not yet sent an uptoken.
				{
				//Choose an uptoken. (ASCII 33-126)
				rnum = (float)rand() / (float)RAND_MAX;
				rnum = roundf(rnum * 93.0);
				tun->uptoken = (char)rnum + (char)33;
				sprintf(uptoken_string, "%c\n", tun->uptoken);
				if((ioret = write_all(tun->pipe_stdin[PIPE_WRITE], uptoken_string, strlen(uptoken_string))) != strlen(uptoken_string))
					{
					if(ioret == -1)
						logline(LOG_ERROR, TUNNEL_MODULE "uptoken write() failed! (%s)", tun->id, strerror(errno));
					else //Couldn't write enough bytes, but no reported error.
						logline(LOG_ERROR, TUNNEL_MODULE "uptoken write() failed for unknown reason!", tun->id);
					tun->uptoken_condemned = TRUE; //Mark this tunnel process as condemned by the uptoken system.
					}
				else //Uptoken sent!
					{
					//logline(LOG_INFO, TUNNEL_MODULE "uptoken (%c) sent to far end.", tun->id, tun->uptoken);
					tun->uptoken_sent = now;
					}
				}
			else if(now >= (tun->uptoken_sent + TUNNEL_UPTOKENWAITTIME)) //We have previously sent an uptoken. Has the uptoken wait time elapsed?
				{
				//Check the pipe for a reply from the far end. The first byte should exactly match our uptoken.
				memset(uptoken_string, 0, TUNNEL_UPTOKENBUFFERSIZE);
				ioret = read_all(tun->pipe_stdout[PIPE_READ], uptoken_string, (TUNNEL_UPTOKENBUFFERSIZE - 1));
				if(ioret == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
					{
					logline(LOG_ERROR, TUNNEL_MODULE "uptoken read() failed! (%s)", tun->id, strerror(errno));
					tun->uptoken_condemned = TRUE; //Mark this tunnel process as condemned by the uptoken system.
					}
				else if(strlen(uptoken_string) < 2) //No error reported by read(), but still didn't get enough bytes.
					{
					logline(LOG_WARNING, TUNNEL_MODULE "uptoken read() didn't return enough bytes! uptoken did not come back.", tun->id);
					//logline(LOG_INFO, TUNNEL_MODULE "Details: (%d, %d, \"%s\")", tun->id, ioret, strlen(uptoken_string), uptoken_string);
					tun->uptoken_condemned = TRUE; //Mark this tunnel process as condemned by the uptoken system.
					}
				else //We did get enough bytes.
					{
					//Does the uptoken match?
					if(uptoken_string[0] == tun->uptoken)
						{
						//logline(LOG_INFO, TUNNEL_MODULE "uptoken (%c) received from far end.", tun->id, tun->uptoken);
						//Okay! Forget this uptoken so we can pick a new one next round.
						tun->uptoken = -1;
						}
					else
						{
						//Oh dear! We got something unexpected back from the far end.
						logline(LOG_WARNING, TUNNEL_MODULE "uptoken does not match! The far end sent something strange.", tun->id);
						tun->uptoken_condemned = TRUE; //Mark this tunnel process as condemned by the uptoken system.
						}
					}
				}
			
			//Did we run into trouble that would require us to send a signal to the child process?
			if(tun->uptoken_condemned)
				{
				logline(LOG_WARNING, TUNNEL_MODULE "uptoken condemned tunnel process %d. Sending SIGKILL...", tun->id, tun->pid);
				if(kill(tun->pid, SIGKILL) == -1)
					{
					logline(LOG_WARNING, TUNNEL_MODULE "kill(%d, SIGKILL) failed! (%s)", tun->id, tun->pid, strerror(errno));
					}
				}
			}
		
		//Poll (non-blocking) for this tunnel's child process exit status.
		waitpid_return = waitpid(tun->pid, &tunnel_status, WNOHANG);
		if(waitpid_return < 0)
			{
			logline(LOG_ERROR, TUNNEL_MODULE "waitpid() returned an error!", tun->id);
			return FALSE;
			}
		else if(waitpid_return == tun->pid)
			{
			logline(LOG_WARNING, TUNNEL_MODULE "Child process exited with status %d!", tun->id, WEXITSTATUS(tunnel_status));
			tun->pid = 0; //No more PID.
			tun->uptoken = -1; //Clear uptoken too.
			tun->uptoken_condemned = FALSE;
			//If the child process dies for any reason, the trouble level goes up. (Up to TUNNEL_TROUBLEMAX)
			if(tun->trouble < TUNNEL_TROUBLEMAX)
				tun->trouble = tun->trouble + 1;
			//With the calculated trouble level comes a launch delay.
			launchdelay_seconds = (time_t)powf((float)2.0, (float)tun->trouble);
			tun->trouble_launchnext = now + launchdelay_seconds;
			logline(LOG_INFO, TUNNEL_MODULE "Will wait at least %d seconds before relaunching.", tun->id, launchdelay_seconds);
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
	
	logline(LOG_INFO, TUNNEL_MODULE "Destroying tunnel object...", tun->id);
	
	//Let's make sure the child process is dead.
	if(tun->pid > 0)
		{
		logline(LOG_INFO, TUNNEL_MODULE "Process %d still running. Sending SIGKILL...", tun->id, tun->pid);
		if(kill(tun->pid, SIGKILL) == -1)
			{
			logline(LOG_WARNING, TUNNEL_MODULE "kill(%d, SIGKILL) failed! (%s)", tun->id, tun->pid, strerror(errno));
			}
		else
			{
			//Since we successfully sent a signal we now need to waitpid() until the child process dies.
			waitpid(tun->pid, NULL, 0);
			}
		}
	
	//Let's make sure any remaining pipes are closed.
	if(!stdpipes_close_remaining(tun->pipe_stdin, tun->pipe_stdout, tun->pipe_stderr))
		{
		logline(LOG_WARNING, TUNNEL_MODULE "stdpipes_close_remaining() returned an error!", tun->id);
		}
	
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



