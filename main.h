/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * main.h
 *     - Daemon can run all the time.
 *     - Starts, monitors, and restarts SSH tunnels as necessary.
 *
 * Alex Markley; All Rights Reserved
 */

//Only process this header once.
#ifndef __SSHTUNNELS_MAIN_H

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define TRUE 1
#define FALSE 0

#define APP_SHORT_NAME "sshtunnels"

#define __SSHTUNNELS_MAIN_H
#endif


