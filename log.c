/*
 * SSHTunnels - A program for generating and maintaining SSH Tunnels
 *
 * log.c
 *     - Logging functions.
 *
 * Alex Markley; All Rights Reserved
 */

#include "main.h"
#include "log.h"

void logline(int type, char *message_format, ...)
	{
	int loopa, loopb, freeable = 1, bufferlen = LOG_BUFFERLEN_STEP, formatting, ret;
	char *buffer = NULL, *buffer_temp = NULL;
	va_list arguments;

	if(type == LOG_INFO && LOG_SUPRESS_ALL_INFO == TRUE) return;
	
	if(message_format == NULL)
		{
		logline(LOG_ERROR, "Internal Program Error! NULL message?");
		return;
		}
	
	//We're going to try and grab some memory so we can do neat things.
	if((buffer = malloc(bufferlen * sizeof(char))) == NULL)
		{
		//We couldn't get any memory, so give up on that.
		freeable = 0;
		buffer = message_format;
		logline(LOG_ERROR, "Out Of Memory!");
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
				bufferlen = bufferlen + LOG_BUFFERLEN_STEP;
				if((buffer_temp = realloc(buffer, bufferlen)) == NULL)
					{
					free(buffer);
					//We couldn't get enough memory, so give up on that.
					freeable = 0;
					buffer = message_format;
					logline(LOG_ERROR, "Out Of Memory!");
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
	
	if(buffer != NULL)
		{
		switch(type)
			{
			case LOG_INFO:
				fprintf(LOG_OUT, APP_SHORT_NAME ": Info: %s\n", buffer);
				break;
			case LOG_WARNING:
				fprintf(LOG_OUT, APP_SHORT_NAME ": Warning: %s\n", buffer);
				break;
			case LOG_ERROR:
				fprintf(LOG_OUT, APP_SHORT_NAME ": Error: %s\n", buffer);
				break;
			default:
				logline(LOG_WARNING, "Internal Program Error: Somebody sent a message without a valid message type! The errant message follows:");
				fprintf(LOG_OUT, APP_SHORT_NAME ": Unknown Notice: %s\n", buffer);
				break;
			}
		}
	
	if(freeable) free(buffer);
	return;
	}

