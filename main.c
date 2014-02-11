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

int main(int argc, char **argv, char **envp)
	{
	int finished = FALSE;
	struct tunnel *tun;
	char *newargv[] = { "/usr/bin/tee", "--version", NULL };
	
	tun = tunnel_create(newargv, envp);
	
	while(!finished)
		{
		if(!tunnel_maintenance(tun))
			finished = TRUE;
		else
			sleep(1);
		}
	
	return 0;
	}

