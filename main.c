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
#include "tunnel.h"
#include "log.h"

struct tunnel *globtun;
int finished = FALSE;

void termination_handler(int signum)
	{
	logline(LOG_INFO, "Caught signal %d.", signum);
	finished = TRUE;
	}

void teardown_tunnels(void)
	{
	tunnel_destroy(globtun);
	}

int main(int argc, char **argv, char **envp)
	{
	struct sigaction sigact;
	char *newargv[] = { "/usr/bin/ssh", "elbmin", "tee", NULL };
	
	//Set up signal handling and teardown.
	sigact.sa_handler = termination_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	if(sigaction(SIGINT, &sigact, NULL) != 0)
		logline(LOG_WARNING, "Registering of SIGINT signal handler failed. (%s)", strerror(errno));
	if(sigaction(SIGHUP, &sigact, NULL) != 0)
		logline(LOG_WARNING, "Registering of SIGHUP signal handler failed. (%s)", strerror(errno));
	if(sigaction(SIGTERM, &sigact, NULL) != 0)
		logline(LOG_WARNING, "Registering of SIGTERM signal handler failed. (%s)", strerror(errno));
	if(atexit(teardown_tunnels) != 0)
		logline(LOG_WARNING, "Registering of teardown function failed.");
	
	globtun = tunnel_create(newargv, envp, TRUE);
	
	while(!finished)
		{
		if(!tunnel_maintenance(globtun))
			finished = TRUE;
		else
			sleep(1);
		}
	
	return 0;
	}

