/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * log.c
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

#include "main.h"
#include "log.h"

#ifdef SYSLOG
#include <syslog.h>
#endif

FILE *logoutput(int query, FILE *newdest)
	{
	static FILE *outdest;
	static int initialized = FALSE;
	
	if(!initialized)
		{
		outdest = STL_OUTPUT_DEFAULT;
		initialized = TRUE;
		}
	
	if(!query)
		{
		#ifndef SYSLOG
		if(newdest == STL_OUTPUT_SYSLOG)
			newdest = stderr;
		#endif
		outdest = newdest;
		}
	return outdest;
	}

char *logname(char *replace)
	{
	static char *n = NULL;
	if(replace != NULL)
		n = replace;
	return n;
	}

void loginit(char *name)
	{
	static int initialized = FALSE;
	logname(name);
	
	#ifdef SYSLOG
	if(initialized)
		closelog();
	openlog(name, LOG_CONS | LOG_PID | LOG_PERROR, LOG_LOCAL0);
	#endif
	
	initialized = TRUE;
	}

void stl(int type, char *message_format, ...)
	{
	int loopa, loopb, freeable = 1, bufferlen = STL_BUFFERLEN_STEP, formatting, ret;
	char *buffer = NULL, *buffer_temp = NULL;
	FILE *outdest;
	va_list arguments;

	if(type == STL_INFO && STL_SUPRESS_ALL_INFO == TRUE) return;
	
	if(message_format == NULL)
		{
		stl(STL_ERROR, "Internal Program Error! NULL message?");
		return;
		}
	
	//We're going to try and grab some memory so we can do neat things.
	if((buffer = malloc(bufferlen * sizeof(char))) == NULL)
		{
		//We couldn't get any memory, so give up on that.
		freeable = 0;
		buffer = message_format;
		stl(STL_ERROR, "Out Of Memory!");
		}
	else //We got our text buffer. Now we can populate it and perform a format conversion.
		{
		//Perform a format conversion, increasing the size of our buffer until we have enough.
		formatting = 1;
		while(formatting)
			{
			//Format conversion.
			va_start(arguments, message_format);
			ret = vsnprintf(buffer, bufferlen, message_format, arguments);
			va_end(arguments);
			
			//Did the format conversion succeed? Did it fit in the buffer?
			if(ret >= 0 && ret < bufferlen)
				{
				formatting = 0; //All done.
				}
			else //The formatting failed! Need more memory!
				{
				//We need to increase the size of the buffer.
				bufferlen = bufferlen + STL_BUFFERLEN_STEP;
				if((buffer_temp = realloc(buffer, bufferlen)) == NULL)
					{
					free(buffer);
					//We couldn't get enough memory, so give up on that.
					freeable = 0;
					buffer = message_format;
					stl(STL_ERROR, "Out Of Memory!");
					formatting = 0;
					}
				else //We succeeded in reallocating.
					buffer = buffer_temp;
				}
			}
		
		if(freeable) //Are we still working on our own buffer?
			{
			loopb = 0;
			for(loopa = 0; buffer[loopa]; loopa++)
				{
				if(buffer[loopa] >= 32 && buffer[loopa] <= 126) //Strip out ALL non-printable ASCII.
					{
					if(loopa != loopb) buffer[loopb] = buffer[loopa];
					loopb++;
					}
				}
			buffer[loopb] = '\0';
			}
		}
	
	//Check the output destination.
	outdest = logoutput(TRUE, NULL);
	
	//Make sure outdest is not set to syslog if syslog support is not built in.
	#ifndef SYSLOG
	if(outdest == STL_OUTPUT_SYSLOG)
		outdest = stderr;
	#endif
	
	//Actually output the log line.
	if(buffer != NULL)
		{
		switch(type)
			{
			case STL_INFO:
				#ifdef SYSLOG
				if(outdest == STL_OUTPUT_SYSLOG)
					syslog(LOG_INFO, "%s", buffer);
				#endif
				
				if(outdest != STL_OUTPUT_SYSLOG)
					fprintf(outdest, "%s: Info: %s\n", logname(NULL), buffer);
				break;
			case STL_WARNING:
				#ifdef SYSLOG
				if(outdest == STL_OUTPUT_SYSLOG)
					syslog(LOG_WARNING, "%s", buffer);
				#endif
				
				if(outdest != STL_OUTPUT_SYSLOG)
					fprintf(outdest, "%s: Warning: %s\n", logname(NULL), buffer);
				break;
			case STL_ERROR:
				#ifdef SYSLOG
				if(outdest == STL_OUTPUT_SYSLOG)
					syslog(LOG_ERR, "%s", buffer);
				#endif
				
				if(outdest != STL_OUTPUT_SYSLOG)
					fprintf(outdest, "%s: Error: %s\n", logname(NULL), buffer);
				break;
			default:
				stl(STL_WARNING, "Internal Program Error: Somebody sent a message without a valid message type! The errant message follows:");
				
				#ifdef SYSLOG
				if(outdest == STL_OUTPUT_SYSLOG)
					syslog(LOG_WARNING, "%s", buffer);
				#endif
				
				if(outdest != STL_OUTPUT_SYSLOG)
					fprintf(outdest, "%s: Unknown Notice: %s\n", logname(NULL), buffer);
				break;
			}
		if(outdest != STL_OUTPUT_SYSLOG)
			fflush(NULL);
		}
	
	if(freeable) free(buffer);
	return;
	}

