/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * log.h
 *     - Logging functions.
 *
 * Alex Markley; All Rights Reserved
 */

//Only process this header once.
#ifndef __SSHTUNNELS_LOG_H

#include <stdio.h>
#include <stdarg.h>

//Valid log levels.
enum
	{
	LOG_ERROR,
	LOG_WARNING,
	LOG_INFO,
	LOG_RESERVED
	};

//Should we even bother dumping INFO messages?
//If this is TRUE, don't generate any info messages regardless of
//the MB setting above.
#define LOG_SUPRESS_ALL_INFO FALSE //(Can be TRUE or FALSE).

//We can do any number of things here. We could write these to a log file, we could open informational dialog boxes, whatever.
#define LOG_OUT stderr

//Message formatting buffer size incrementation.
#define LOG_BUFFERLEN_STEP 128

void logline(int status, char *message_format, ...); //Now supports printf() format conversion.

#define __SSHTUNNELS_LOG_H
#endif

