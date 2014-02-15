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
#include <signal.h>
#include <expat.h>

#define TRUE 1
#define FALSE 0

#define MAIN_SLEEP_SECONDS_DEFAULT 5
#define UPTOKEN_ENABLED_DEFAULT TRUE

#define APP_SHORT_NAME "sshtunnels"
#define XMLBUFFERSIZE 512
#define LIST_GROW_STEP 8
#define XMLPARSER "XML Config Parser: "

#define __SSHTUNNELS_MAIN_H
#endif


