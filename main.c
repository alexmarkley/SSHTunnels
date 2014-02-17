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

struct sshtunnels_configstate
	{
	int failed;
	int in_sshtunnels, seen_sshtunnels;
	int in_tunnel, seen_tunnel;
	int in_programargument, count_programargument;
	int in_programenvironment, count_programenvironment;
	char **newargv, **newenvp, **defenvp;
	int newargv_len, newargv_pos, newenvp_len, newenvp_pos;
	int uptoken_enabled;
	time_t uptoken_interval;
	};

int main_finished = FALSE;
time_t main_sleep_seconds = MAIN_SLEEP_SECONDS_DEFAULT;
FILE *log_output_file = NULL;
struct tunnel **main_tunnels = NULL;
int main_tunnels_len = 0, main_tunnels_pos = 0;

void termination_handler(int signum);
void brokenpipe_handler(int signum);
int read_configuration(char **defenvp);
void tagstart(void *data, const char *name, const char **attributes);
void tagend(void *data, const char *name);
char *insert_new_environment_variable(char ***newenvp, int *newenvp_len, int *newenvp_pos, char *new);
void destroy_tunnel_argvenvp(struct tunnel *tun);
void destroy_arglist(char **list);
void destroy_alltunnels(void);

int main(int argc, char **argv, char **envp)
	{
	time_t wakeup;
	int error = FALSE;
	int i;
	struct sigaction sigact;
	
	logname("SSHTunnels");
	
	//Read in the configuration or die.
	if(!read_configuration(envp))
		{
		destroy_alltunnels();
		return 1;
		}
	
	//Set up signal handling and teardown.
	sigact.sa_handler = termination_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	if(sigaction(SIGINT, &sigact, NULL) != 0)
		logline(LOG_WARNING, "Registering of SIGINT signal handler failed. (%s)", strerror(errno));
	if(sigaction(SIGHUP, &sigact, NULL) != 0)
		logline(LOG_WARNING, "Registering of SIGHUP signal handler failed. (%s)", strerror(errno));
	if(sigaction(SIGTERM, &sigact, NULL) != 0)
		logline(LOG_WARNING, "Registering of SIGTERM signal handler failed. (%s)", strerror(errno));
	sigact.sa_handler = brokenpipe_handler;
	if(sigaction(SIGPIPE, &sigact, NULL) != 0)
		logline(LOG_WARNING, "Registering of SIGPIPE signal handler failed. (%s)", strerror(errno));
	
	while(!main_finished)
		{
		//Perform maintenance on all of our tunnels.
		for(i = 0; main_tunnels[i] && !main_finished; i++)
			{
			if(!tunnel_maintenance(main_tunnels[i]))
				{
				logline(LOG_ERROR, "FATAL! tunnel_maintenance() returned with an error.");
				main_finished = TRUE;
				error = TRUE;
				}
			}
		
		//Make sure we sleep for at least main_sleep_seconds seconds unless we catch a signal.
		wakeup = time(NULL) + main_sleep_seconds;
		while(!main_finished && time(NULL) < wakeup)
			sleep(1);
		}
	
	//Tear down all of our tunnels.
	destroy_alltunnels();
	
	if(log_output_file) fclose(log_output_file);
	return error;
	}

void termination_handler(int signum)
	{
	logline(LOG_INFO, "Caught signal %d.", signum);
	main_finished = TRUE;
	}

void brokenpipe_handler(int signum)
	{
	logline(LOG_WARNING, "Caught SIGPIPE (%d). Ignoring...", signum);
	}

int read_configuration(char **defenvp)
	{
	XML_Parser parser;
	int done = 0, i = 0, length;
	char *config_filename[] = { "./" CONFIG_FILENAME, PREFIX "/etc/" CONFIG_FILENAME, "/etc/" CONFIG_FILENAME, NULL };
	char filebuffer[XMLBUFFERSIZE];
	FILE *in = NULL;
	struct sshtunnels_configstate state;
	
	//Initialize state.
	state.failed = FALSE;
	state.in_sshtunnels = FALSE;
	state.seen_sshtunnels = FALSE;
	state.in_tunnel = FALSE;
	state.seen_tunnel = FALSE;
	state.in_programargument = FALSE;
	state.count_programargument = 0;
	state.in_programenvironment = FALSE;
	state.count_programenvironment = 0;
	state.newargv = NULL;
	state.newargv_len = 0;
	state.newargv_pos = 0;
	state.newenvp = NULL;
	state.newenvp_len = 0;
	state.newenvp_pos = 0;
	state.defenvp = defenvp;
	state.uptoken_enabled = UPTOKEN_ENABLED_DEFAULT;
	state.uptoken_interval = UPTOKEN_INTERVAL_DEFAULT;
	
	//Set up XML parser for configuration
	if(!(parser = XML_ParserCreate(NULL)))
		{
		logline(LOG_ERROR, "Failed initializing XML parser!");
		return FALSE;
		}
	XML_SetElementHandler(parser, tagstart, tagend);
	XML_UseParserAsHandlerArg(parser);
	XML_SetUserData(parser, (void *)&state);
	
	//Open config XML file for reading. (Try a couple of different file paths.)
	for(i = 0;config_filename[i] && !in; i++)
		in = fopen(config_filename[i], "rb");
	if(!in)
		{
		logline(LOG_ERROR, "Failed to open " CONFIG_FILENAME);
		for(i = 0; config_filename[i]; i++)
			logline(LOG_ERROR, "Tried: %s", config_filename[i]);
		return FALSE;
		}
	
	//Loop through, reading the file.
	while(!done && !state.failed)
		{
		//Read a chunk.
		length = fread(filebuffer, sizeof(char), XMLBUFFERSIZE, in);
		if(length == 0) done = 1;
		
		//Actually parse the chunk.
		if(XML_Parse(parser, filebuffer, length, done) == 0)
			{
			logline(LOG_ERROR, XMLPARSER "Failed at line %d: %s", (int)XML_GetCurrentLineNumber(parser), XML_ErrorString(XML_GetErrorCode(parser)));
			fclose(in);
			return FALSE;
			}
		}
	
	if(!state.failed && !state.seen_sshtunnels)
		{
		//This shouldn't be possible.
		logline(LOG_ERROR, XMLPARSER "Failed!");
		state.failed = TRUE;
		}
	
	XML_ParserFree(parser);
	fclose(in);
	return state.failed ? FALSE : TRUE;
	}

void tagstart(void *data, const char *name, const char **attributes)
	{
	int i, j, seenv;
	char *buf;
	XML_Parser parser = (XML_Parser)data;
	struct sshtunnels_configstate *state = (struct sshtunnels_configstate *)XML_GetUserData(parser);
	
	if(!state->failed)
		{
		if(!state->in_sshtunnels)
			{
			if(strcmp(name, "SSHTunnels") == 0)
				{
				state->in_sshtunnels = TRUE;
				state->seen_sshtunnels = TRUE;
				
				//Scan through all attributes.
				for(i = 0; attributes[i]; i = i + 2)
					{
					if(strcmp(attributes[i], "LogOutput") == 0)
						{
						if(strcasecmp(attributes[i+1], "stdout") == 0)
							logoutput(stdout);
						else if(strcasecmp(attributes[i+1], "stderr") == 0)
							logoutput(stderr);
						else //Literal file name for log output.
							{
							if((log_output_file = fopen(attributes[i+1], "ab")) == NULL)
								{
								logline(LOG_ERROR, XMLPARSER "Could not open specified log file (%s) for writing! Line: %d.", attributes[i+1], (int)XML_GetCurrentLineNumber(parser));
								state->failed = TRUE;
								return;
								}
							logoutput(log_output_file);
							logline(LOG_INFO, "Opened log file (%s).", attributes[i+1]);
							}
						}
					if(strcmp(attributes[i], "SleepTimer") == 0)
						{
						if(sscanf(attributes[i+1], "%d", &j) != 1)
							{
							logline(LOG_ERROR, XMLPARSER "SleepTimer must be an integer! Line: %d", (int)XML_GetCurrentLineNumber(parser));
							state->failed = TRUE;
							return;
							}
						if(j < 1 || j > 60)
							{
							logline(LOG_ERROR, XMLPARSER "SleepTimer must be a positive integer between 1 and 60. Line: %d", (int)XML_GetCurrentLineNumber(parser));
							state->failed = TRUE;
							return;
							}
						main_sleep_seconds = (time_t)j;
						}
					}
				}
			else
				{
				logline(LOG_ERROR, XMLPARSER "Config XML must start with <SSHTunnels> tag. Line: %d.", (int)XML_GetCurrentLineNumber(parser));
				state->failed = TRUE;
				return;
				}
			}
		else //We're in <SSHTunnels>
			{
			if(!state->in_tunnel)
				{
				if(strcmp(name, "Tunnel") == 0)
					{
					state->in_tunnel = TRUE;
					state->seen_tunnel = TRUE;
					state->uptoken_enabled = UPTOKEN_ENABLED_DEFAULT;
					state->uptoken_interval = UPTOKEN_INTERVAL_DEFAULT;
					state->newargv = NULL;
					state->newargv_len = 0;
					state->newargv_pos = 0;
					state->newenvp = NULL;
					state->newenvp_len = 0;
					state->newenvp_pos = 0;
					
					//Initialize envp using values from defenvp.
					for(i = 0; state->defenvp[i]; i++)
						{
						if((insert_new_environment_variable(&state->newenvp, &state->newenvp_len, &state->newenvp_pos, state->defenvp[i])) == NULL)
							{
							state->failed = TRUE;
							return;
							}
						}
					
					//Scan through all attributes.
					for(i = 0; attributes[i]; i = i + 2)
						{
						if(strcmp(attributes[i], "UpTokenEnabled") == 0)
							{
							if(strcasecmp(attributes[i+1], "true") == 0)
								state->uptoken_enabled = TRUE;
							else if(strcasecmp(attributes[i+1], "false") == 0)
								state->uptoken_enabled = FALSE;
							else
								{
								logline(LOG_ERROR, XMLPARSER "UpTokenEnabled must be TRUE or FALSE! Line: %d.", (int)XML_GetCurrentLineNumber(parser));
								state->failed = TRUE;
								return;
								}
							}
						else if(strcmp(attributes[i], "UpTokenInterval") == 0)
							{
							if(sscanf(attributes[i+1], "%d", &j) != 1)
								{
								logline(LOG_ERROR, XMLPARSER "UpTokenInterval must be an integer! Line: %d", (int)XML_GetCurrentLineNumber(parser));
								state->failed = TRUE;
								return;
								}
							if(j < 1 || j > 60)
								{
								logline(LOG_ERROR, XMLPARSER "UpTokenInterval must be a positive integer between 1 and 60. Line: %d", (int)XML_GetCurrentLineNumber(parser));
								state->failed = TRUE;
								return;
								}
							state->uptoken_interval = (time_t)j;
							}
						}
					}
				else
					{
					logline(LOG_ERROR, XMLPARSER "Only <Tunnel> tags allowed within <SSHTunnels> tag. Line: %d.", (int)XML_GetCurrentLineNumber(parser));
					state->failed = TRUE;
					return;
					}
				}
			else //We're in <Tunnel>
				{
				if(!state->in_programargument && !state->in_programenvironment)
					{
					if(strcmp(name, "ProgramArgument") == 0)
						{
						state->in_programargument = TRUE;
						state->count_programargument++;
						
						//Scan through all attributes.
						seenv = FALSE;
						for(i = 0; attributes[i]; i = i + 2)
							{
							if(strcmp(attributes[i], "v") == 0)
								{
								seenv = TRUE;
								if((buf = calloc(strlen(attributes[i+1]) + 1, sizeof(char))) == NULL)
									{
									logline(LOG_ERROR, "Out of memory!");
									state->failed = TRUE;
									return;
									}
								strcpy(buf, attributes[i+1]);
								if((state->newargv = list_grow_insert(state->newargv, &buf, sizeof(char *), &state->newargv_len, &state->newargv_pos)) == NULL)
									{
									logline(LOG_ERROR, "Out of memory!");
									state->failed = TRUE;
									free(buf);
									return;
									}
								}
							}
						if(!seenv)
							{
							logline(LOG_ERROR, XMLPARSER "<ProgramArgument> tag requires \"v\" attribute. Line: %d.", (int)XML_GetCurrentLineNumber(parser));
							state->failed = TRUE;
							return;
							}
						}
					else if(strcmp(name, "ProgramEnvironment") == 0)
						{
						state->in_programenvironment = TRUE;
						state->count_programenvironment++;
						
						//Scan through all attributes.
						seenv = FALSE;
						for(i = 0; attributes[i]; i = i + 2)
							{
							if(strcmp(attributes[i], "v") == 0)
								{
								seenv = TRUE;
								if((insert_new_environment_variable(&state->newenvp, &state->newenvp_len, &state->newenvp_pos, (char *)attributes[i+1])) == NULL)
									{
									state->failed = TRUE;
									return;
									}
								}
							}
						if(!seenv)
							{
							logline(LOG_ERROR, XMLPARSER "<ProgramEnvironment> tag requires \"v\" attribute. Line: %d.", (int)XML_GetCurrentLineNumber(parser));
							state->failed = TRUE;
							return;
							}
						}
					else
						{
						logline(LOG_ERROR, XMLPARSER "Only <ProgramArgument> or <ProgramEnvironment> tags allowed within <Tunnel> tag. Line: %d.", (int)XML_GetCurrentLineNumber(parser));
						state->failed = TRUE;
						return;
						}
					}
				else //We are in either <ProgramArgument> or <ProgramEnvironment>
					{
					logline(LOG_ERROR, XMLPARSER "No tags are allowed inside <ProgramArgument> or <ProgramEnvironment>. Line: %d.", (int)XML_GetCurrentLineNumber(parser));
					state->failed = TRUE;
					return;
					}
				}
			}
		}
	}

void tagend(void *data, const char *name)
	{
	time_t interval;
	XML_Parser parser = (XML_Parser)data;
	struct sshtunnels_configstate *state = (struct sshtunnels_configstate *)XML_GetUserData(parser);
	struct tunnel *mytun;
	
	if(!state->failed)
		{
		if(strcmp(name, "SSHTunnels") == 0)
			{
			state->in_sshtunnels = FALSE;
			if(!state->seen_tunnel)
				{
				logline(LOG_ERROR, XMLPARSER "At least one <Tunnel> required within <SSHTunnels>. Line: %d", (int)XML_GetCurrentLineNumber(parser));
				state->failed = TRUE;
				return;
				}
			}
		else if(strcmp(name, "Tunnel") == 0)
			{
			state->in_tunnel = FALSE;
			if(state->count_programargument < 1)
				{
				logline(LOG_ERROR, XMLPARSER "At least one <ProgramArgument> required within <Tunnel>. Line: %d", (int)XML_GetCurrentLineNumber(parser));
				state->failed = TRUE;
				return;
				}
			
			logline(LOG_INFO, XMLPARSER "Parsed <Tunnel> declaration with %d <ProgramArgument> tag(s) and %d <ProgramEnvironment> tag(s).", state->count_programargument, state->count_programenvironment);
			
			//Normalize interval.
			if(state->uptoken_interval % main_sleep_seconds != 0)
				{
				interval = ((state->uptoken_interval / main_sleep_seconds) + 1) * main_sleep_seconds;
				if(interval > 60)
					interval = main_sleep_seconds;
				logline(LOG_WARNING, "UpToken Interval of %d is not evenly divisible by the main Sleep Timer, which is set to %d. Using UpToken Interval of %d instead.", (int)state->uptoken_interval, (int)main_sleep_seconds, (int)interval);
				state->uptoken_interval = interval;
				}
			
			//Handle tunnel object creation.
			if((mytun = tunnel_create(state->newargv, state->newenvp, state->uptoken_enabled, state->uptoken_interval)) == NULL)
				{
				logline(LOG_ERROR, "Tunnel object creation failed!");
				state->failed = TRUE;
				destroy_arglist(state->newargv);
				destroy_arglist(state->newenvp);
				return;
				}
			if((main_tunnels = list_grow_insert(main_tunnels, &mytun, sizeof(struct tunnel *), &main_tunnels_len, &main_tunnels_pos)) == NULL)
				{
				logline(LOG_ERROR, "Out of memory!");
				state->failed = TRUE;
				destroy_tunnel_argvenvp(mytun);
				tunnel_destroy(mytun);
				return;
				}
			}
		else if(strcmp(name, "ProgramArgument") == 0)
			{
			state->in_programargument = FALSE;
			}
		else if(strcmp(name, "ProgramEnvironment") == 0)
			{
			state->in_programenvironment = FALSE;
			}
		}
	}

//Envp should be populated with non-duplicate entries. So we'll check for dupes before inserting each entry.
//Will return a pointer a freshly-allocated, \0-terminated string copy of "new", or NULL on failure.
char *insert_new_environment_variable(char ***newenvp, int *newenvp_len, int *newenvp_pos, char *new)
	{
	int i, equalspos;
	char *mynew;
	char **myenvp = *newenvp; //I'm not gonna lie. This is pretty Inceptionesque.
	
	if((mynew = calloc(strlen(new) + 1, sizeof(char))) == NULL)
		{
		logline(LOG_ERROR, "Out of memory!");
		return NULL;
		}
	strcpy(mynew, new);
	
	//Find the first equals sign within the new variable.
	equalspos = strlen(mynew) - 1; //Default to last byte of the string.
	for(i = 0; mynew[i]; i++)
		{
		if(mynew[i] == '=')
			{
			equalspos = i;
			break;
			}
		}
	
	//Scan through envp, looking for dupes.
	if(myenvp != NULL)
		{
		for(i = 0; myenvp[i]; i++)
			{
			//Does the portion before the equals sign match?
			if(strncmp(myenvp[i], mynew, equalspos + 1) == 0)
				{
				//We are replacing this variable, instead of adding one to the end of the list.
				free(myenvp[i]);
				myenvp[i] = mynew;
				return mynew;
				}
			}
		}
	
	//No duplicates. mynew needs to go onto the end of the list.
	if((*newenvp = list_grow_insert(myenvp, &mynew, sizeof(char *), newenvp_len, newenvp_pos)) == NULL)
		{
		logline(LOG_ERROR, "Out of memory!");
		free(mynew);
		return NULL;
		}
	
	return mynew;
	}

//Tunnel module doesn't allocate or populate argv and envp, we do. So tunnel_destroy() isn't responsible for tearing them down either.
void destroy_tunnel_argvenvp(struct tunnel *tun)
	{
	if(tun == NULL)
		return;
	
	destroy_arglist(tun->argv);
	tun->argv = NULL;
	destroy_arglist(tun->envp);
	tun->envp = NULL;
	}

void destroy_arglist(char **list)
	{
	int i;
	if(list == NULL)
		return;
	
	for(i = 0; list[i]; i++)
		free(list[i]);
	free(list);
	}

void destroy_alltunnels(void)
	{
	int i;
	if(main_tunnels)
		{
		for(i = 0; main_tunnels[i]; i++)
			{
			destroy_tunnel_argvenvp(main_tunnels[i]);
			tunnel_destroy(main_tunnels[i]);
			}
		free(main_tunnels);
		main_tunnels = NULL;
		}
	}

