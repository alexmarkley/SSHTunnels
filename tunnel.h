/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * tunnel.h
 *     - Handles tunnels.
 *
 * Alex Markley; All Rights Reserved
 */

//Only process this header once.
#ifndef __SSHTUNNELS_TUNNEL_H

#include <time.h>


/*
	Tunnel (The proposed life cycle.)
		- Tunnel structure allocated. argc and envp populated with reasonable data. (from configuration?) pid set to zero. uptoken and pipes all set to -1.
		- Tunnel maintenance loop:
			- Notice that pid is zero. Launch child process:
				- open pipes, fork, do pipe stuff, and exec
				- retain child pid
				- maybe set a timer??
			- Notice that uptoken is -1. Select uptoken and send to child's stdin. (maybe set a timer???????)
			- Sleep N seconds.
			- Check child's stdout for uptoken.
				- If the uptoken is present, set uptoken to -1
				- If the uptoken is not present ???????????
			- Check child stderr for messages.
			- Check child pid for exit / error status.
				- If child was launched too recently...?
			- Sleep N seconds.
		- Tunnel teardown.
			- If pid, kill child process??
			- Close pipes?
			- Free allocated memory.
*/
struct tunnel
	{
	int id;
	char **argv, **envp;
	pid_t pid;
	int pipe_stdin[2], pipe_stdout[2], pipe_stderr[2];
	char uptoken;
	time_t pid_launched, uptoken_sent;
	};

#define TUNNEL_LAUNCHTHROTTLE 10

struct tunnel *tunnel_create(char **argv, char **envp);
int tunnel_maintenance(struct tunnel *tun);
int tunnel_destroy(struct tunnel *tun);
int tunnel_process_launch(struct tunnel *tun);
int tunnel_check_stderr(int fd, char *logline_prefix);

#define __SSHTUNNELS_TUNNEL_H
#endif

