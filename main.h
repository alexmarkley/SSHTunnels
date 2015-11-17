/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * main.h
 *     - Daemon can run all the time.
 *     - Starts, monitors, and restarts SSH tunnels as necessary.
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
#ifndef __SSHTUNNELS_MAIN_H

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define TRUE 1
#define FALSE 0

#define MAIN_SLEEP_SECONDS_DEFAULT 5
#define UPTOKEN_ENABLED_DEFAULT TRUE
#define UPTOKEN_INTERVAL_DEFAULT 15
#define UPTOKEN_INTERVAL_GRACEPERIOD 5
#define UPTOKEN_BUFFER_SIZE 8 //Don't change this.
#define UPTOKEN_HEADER_BUFFER_SIZE 128 //Don't change this.
#define UPTOKEN_HEADER_VERSION 1
#define UPTOKEN_HEADER_FORMAT "HeaderVersion: %d; UpToken Interval: %d;\n"

#define XMLBUFFERSIZE 512
#define LIST_GROW_STEP 8
#define XMLPARSER "XML Config Parser: "
#define CONFIG_FILENAME "SSHTunnels_config.xml"

#ifndef PREFIX
#define PREFIX "/usr/local"
#endif

#define __SSHTUNNELS_MAIN_H
#endif


