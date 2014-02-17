/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * receiver.c
 *     - UpTokenReceiver runs at the far end of the tunnel, echoing UpTokens from STDIN to STDOUT.
 *     - Kills far end of the tunnel if the link goes down.
 *
 * Alex Markley; All Rights Reserved
 */

#include "main.h"
#include "util.h"
#include "log.h"

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>

#define RECEIVER_SLEEP_SECONDS_DEFAULT 2

int main(int argc, char **argv)
	{
	int i, up = TRUE, header_complete = FALSE, header_pos = 0, header_version, header_uptoken_interval;
	time_t now, wakeup, last_uptoken = 0, uptoken_interval = UPTOKEN_INTERVAL_DEFAULT;
	ssize_t readret, buf_pos;
	pid_t ppid;
	char buf[UPTOKEN_BUFFER_SIZE], header[UPTOKEN_HEADER_BUFFER_SIZE];
	
	logname("UpTokenReceiver");
	
	//Zero out header buffer.
	memset(header, 0, UPTOKEN_HEADER_BUFFER_SIZE);
	
	//Make sure we're not connected to a terminal.
	//If we are, there's a good chance we'll kill the user's shell by accident.
	if(isatty(STDIN_FILENO) || isatty(STDOUT_FILENO))
		{
		logline(LOG_ERROR, "You probably don't want to run this program in a terminal. ;)");
		return 1;
		}
	
	//Make STDIN non-blocking.
	if(!fd_set_nonblock(STDIN_FILENO))
		return 1;
	
	//Run forever. (Ideally)
	while(up)
		{
		now = time(NULL);
		if(last_uptoken == 0)
			last_uptoken = now;
		
		//Read from STDIN.
		readret = read_all(STDIN_FILENO, buf, UPTOKEN_BUFFER_SIZE);
		if(readret > 0)
			{
			//Remember how many bytes are in buf.
			buf_pos = readret;
			
			//Remember the last time we heard from the far end.
			last_uptoken = now;
			
			if(!header_complete) //Header is NOT complete. We need to capture and parse it.
				{
				//Copy read bytes into header buffer.
				for(i = 0; i < readret && !header_complete; i++)
					{
					if(header_pos >= (UPTOKEN_HEADER_BUFFER_SIZE - 2))
						{
						logline(LOG_ERROR, "Error receiving header!");
						return -1;
						}
					header[header_pos] = buf[i];
					
					//The first newline completes the header line.
					if(buf[i] == '\n') header_complete = TRUE;
					
					header_pos++;
					}
				
				//Do we still have more in our buffer? We need to be careful to retain it.
				if(i < readret)
					{
					//Move the trailing contents of buf to the beginning of buf.
					buf_pos = buf_pos - i;
					memmove(buf, buf + i, buf_pos);
					}
				else
					{
					buf_pos = 0;
					}
				
				//Is the header buffer full yet?
				if(header_complete)
					{
					//Check header version first.
					if(sscanf(header, "HeaderVersion: %d;", &header_version) < 1)
						{
						logline(LOG_ERROR, "Couldn't parse header version string! Unknown header format! Proceeding with defaults...");
						}
					else //Got a header version number.
						{
						if(header_version == 1)
							{
							if(sscanf(header, UPTOKEN_HEADER_FORMAT, &header_version, &header_uptoken_interval) < 2)
								{
								logline(LOG_ERROR, "Couldn't parse header version string! Should be version 1, but unknown header format! Proceeding with defaults...");
								}
							else //We got everything.
								{
								logline(LOG_INFO, "Received Header Version %d. Uptoken Interval is %d.", header_version, header_uptoken_interval);
								uptoken_interval = (time_t)header_uptoken_interval;
								}
							}
						else
							{
							logline(LOG_ERROR, "Couldn't parse header version string! Unknown header version %d! Proceeding with defaults...", header_version);
							}
						}
					logline(LOG_INFO, "Header parsing finished. Listening for UpTokens...");
					}
				}
			
			//Have we finished parsing the header yet? Is anything left in buf?
			if(header_complete && buf_pos > 0)
				{
				//Echo all bytes from STDIN to STDOUT.
				if(write_all(STDOUT_FILENO, buf, buf_pos) < 0)
					{
					logline(LOG_ERROR, "failed writing to STDOUT! (%s)", strerror(errno));
					up = FALSE;
					}
				}
			}
		else if(readret < 0)
			{
			//Because read() returned an error code, we need to check errno.
			if(errno != EAGAIN && errno != EWOULDBLOCK)
				{
				logline(LOG_ERROR, "failed reading from STDIN! (%s)", strerror(errno));
				up = FALSE;
				}
			//Zero bytes, but not an error.
			}
		
		//Check the time. If it has been too long since we heard from the far end, we'll set up = FALSE;
		if(now > (last_uptoken + uptoken_interval + UPTOKEN_INTERVAL_GRACEPERIOD))
			{
			logline(LOG_ERROR, "Timeout waiting for input!");
			up = FALSE;
			}
		
		//Only sleep if we are still up and there were no bytes waiting for us in STDIN.
		//(If our buffer was too small, we need to continue reading right away.)
		if(up && readret <= 0)
			{
			wakeup = now + RECEIVER_SLEEP_SECONDS_DEFAULT;
			while(time(NULL) < wakeup)
				sleep(1);
			}
		}
	
	//In the case of an SSH Tunnel with forwarded ports, a stale sshd process will hold open the necessary ports for a VERY LONG TIME.
	//This mechanism sends SIGTERM to that process, killing it and guaranteeing that the ports are free.
	ppid = getppid();
	logline(LOG_INFO, "Sending SIGTERM to parent process. (%d)", ppid);
	if(kill(ppid, SIGTERM) == -1)
		logline(LOG_ERROR, "kill(%d, SIGTERM) failed! (%s)", ppid, strerror(errno));
	
	return 1; //There is no successful exit condition for this program.
	}

