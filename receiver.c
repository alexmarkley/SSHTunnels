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

#include <time.h>
#include <sys/types.h>
#include <signal.h>

#define RECEIVER "UpTokenReceiver: "

int main(int argc, char **argv)
	{
	int up = TRUE;
	time_t now, wakeup, last_uptoken = 0;
	ssize_t readret;
	pid_t ppid;
	char buf[UPTOKEN_BUFFER_SIZE];
	
	//Make sure we're not connected to a terminal.
	//If we are, there's a good chance we'll kill the user's shell by accident.
	if(isatty(STDIN_FILENO) || isatty(STDOUT_FILENO))
		{
		logline(LOG_ERROR, RECEIVER "You probably don't want to run this program in a terminal. ;)");
		return 1;
		}
	
	//Make STDIN non-blocking.
	if(!fd_set_nonblock(STDIN_FILENO))
		return 1;
	
	logline(LOG_INFO, RECEIVER "Listening for UpTokens...");
	
	//Run forever. (Ideally)
	while(up)
		{
		now = time(NULL);
		if(last_uptoken == 0)
			last_uptoken = now;
		
		//Echo any bytes from STDIN to STDOUT.
		readret = read_all(STDIN_FILENO, buf, UPTOKEN_BUFFER_SIZE);
		if(readret > 0)
			{
			//Remember the last time we heard from the far end.
			last_uptoken = now;
			
			if(write_all(STDOUT_FILENO, buf, readret) < 0)
				{
				logline(LOG_ERROR, RECEIVER "failed writing to STDOUT! (%s)", strerror(errno));
				up = FALSE;
				}
			}
		else if(readret < 0)
			{
			//Because read() returned an error code, we need to check errno.
			if(errno != EAGAIN && errno != EWOULDBLOCK)
				{
				logline(LOG_ERROR, RECEIVER "failed reading from STDIN! (%s)", strerror(errno));
				up = FALSE;
				}
			//Zero bytes, but not an error.
			}
		
		//Check the time. If it has been too long since we heard from the far end, we'll set up = FALSE;
		if(now > (last_uptoken + UPTOKEN_INTERVAL_DEFAULT))
			{
			logline(LOG_ERROR, RECEIVER "Timeout waiting for UpToken!");
			up = FALSE;
			}
		
		//Only sleep if we are still up and there were no bytes waiting for us in STDIN.
		//(If our buffer was too small, we need to continue reading right away.)
		if(up && readret <= 0)
			{
			wakeup = now + MAIN_SLEEP_SECONDS_DEFAULT;
			while(time(NULL) < wakeup)
				sleep(1);
			}
		}
	
	//In the case of an SSH Tunnel with forwarded ports, a stale sshd process will hold open the necessary ports for a VERY LONG TIME.
	//This mechanism sends SIGTERM to that process, killing it and guaranteeing that the ports are free.
	ppid = getppid();
	logline(LOG_INFO, RECEIVER "Sending SIGTERM to parent process. (%d)", ppid);
	if(kill(ppid, SIGTERM) == -1)
		logline(LOG_ERROR, RECEIVER "kill(%d, SIGTERM) failed! (%s)", ppid, strerror(errno));
	
	return 1; //There is no successful exit condition for this program.
	}

