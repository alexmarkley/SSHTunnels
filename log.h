/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * log.h
 *     - Logging functions.
 *
 * Copyright (C) 2015 Alex Markley
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
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
#define LOG_DEFAULT_OUT stderr

//Message formatting buffer size incrementation.
#define LOG_BUFFERLEN_STEP 128

FILE *logoutput(FILE *replace);
char *logname(char *replace);
void logline(int status, char *message_format, ...); //Now supports printf() format conversion.

#define __SSHTUNNELS_LOG_H
#endif

