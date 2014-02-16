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
#include <math.h>
#include <sys/types.h>
#include <signal.h>

struct tunnel
	{
	int id;
	char **argv, **envp;
	pid_t pid;
	int pipe_stdin[2], pipe_stdout[2], pipe_stderr[2];
	int uptoken_enabled, uptoken_condemned;
	char uptoken;
	time_t pid_launched, uptoken_sent, trouble_launchnext;
	int trouble;
	};

#define TUNNEL_TROUBLEMAX 8
#define TUNNEL_TROUBLERESETTIME 300

struct tunnel *tunnel_create(char **argv, char **envp, int uptoken_enabled);
int tunnel_maintenance(struct tunnel *tun);
void tunnel_destroy(struct tunnel *tun);
int tunnel_process_launch(struct tunnel *tun);
int tunnel_check_stderr(int fd, char *logline_prefix);

#define __SSHTUNNELS_TUNNEL_H
#endif

