#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

char* PATH = NULL;

// print an error message
int print_error(const char* const error_message)
{
	write(STDERR_FILENO, error_message, strlen(error_message));
	return 0;
}

// searches PATH for program and updates fath to program path if found or NULL otherwise
// path should be unitilized or NULL and should be later freed if not null
int find_path(const char* const program, char** path)
{
	char* stringp = PATH;
	while(stringp != NULL)
	{
		// parse out directory
		const char* const dir = strsep(&stringp, ":");
		// assemble full path with directory and program
		*path = malloc(sizeof(char) * (strlen(dir) + strlen(program) + 1));
		*path = strcpy(*path, dir);
		*path = strcat(*path, program);
		// check if file exists and is executable
		if (!access(*path, X_OK))
		{
			return 0;
		}
	}
	// no suitable path was found so set path to NULL and free
	if (*path != NULL)
		free(*path);
	*path = NULL;
	return 0;
}

/* Given an arg_string, updates argv and argc.
 * argv should be uninitialized or NULL and will be terminated by a NULL pointer after return.
 * The user should call clean_args after each call to get_args in order to free memory. */
int get_args(char* arg_string, char*** argv, int* argc)
{
	*argc = 0;
	// Calculate the number of arguments
	char* c = arg_string;
	bool prev_was_char = false;
	while(*c != '\0')
	{
		// if c points to the start of a new argument
		if (!prev_was_char && (*c != ' ' && *c != '\n' && *c != '\t'))  
		{
			(*argc)++;
			prev_was_char = true;
		}
		// else if c points to a space between arguments
		else if (*c == ' ' || *c == '\n' || *c == '\t') 
		{
			prev_was_char = false;
		}
		c++;
	}
	// allocate argv array according to argument count
	*argv = malloc((*argc + 1) * sizeof(char*));

	// copy tokens into array
	int curr_arg = 0;
	while (arg_string != NULL)
	{
		const char* token = strsep(&arg_string, " \n\t");
		if (strcmp(token, "") != 0)
		{
			(*argv)[curr_arg] =  malloc((strlen(token) + 1) * sizeof(char));
			strcpy((*argv)[curr_arg++], token);
		}
	}	
	(*argv)[*argc] = NULL;

	return 0;
}

// Call once per call to get_args to cleanup argv
int clean_args(int argc, char*** argv)
{
	// free memory allocated to each string in argv
	for (int i = 0; i < argc; i++)
	{
		free((*argv)[i]);
	}
	// free memory allocated to array
	free(*argv);
	return 0;
}

// runs built in commands if applicable
// returns 1 if a command was run and 0 otherwise
// expects NULL terminated array
int run_built_in(char** const argv) 
{	
	// count arguments
	int argc = 0;
	while (argv[++argc] != NULL) {}
	const char* const command = argv[0];
	if (command == NULL)
		return 0;
	// check for exit
	if (strcmp(command, "exit") == 0)
		exit(0);
	// check if cd command was enetered
	if (strcmp(command, "cd") == 0)
	{
		// print error if incorrect number of arguments
		if (argc != 2)
		{
			print_error("Usage: cd <dir>\n");
		}
		else if (chdir(argv[1]) == -1)
		{
			print_error("Could not change directory\n");
		}
		return 1;
		
	}
	if (strcmp(command, "path") == 0)
	{
		if (argc < 2)
		{
			PATH = realloc(PATH, sizeof(char));
			PATH[0] = '\0';
			return 1;
		}

		// determine lenght of path
		int path_len = 0;
		for (int i = 1; i < argc; i ++)
		{
			// add one to account for deliminating character
			path_len += strlen(argv[i]) + 1;
		}

		PATH = realloc(PATH, sizeof(char) * path_len);
		PATH[0] = '\0';
		for (int i = 1; i < argc; i++)
		{
			strcat(PATH, argv[i]);
			strcat(PATH, ":");
		}
		// replace final deliminator with null byte
		PATH[path_len - 1] = '\0';

		return 1;
	}
	return 0;
}

/* populates vector of vector of strings pointed to by commandv with the commands to be run
* and their respective arguments.
* Each vector in commmandv is NULL terminated
* *commandv should be passed in unitialized or NULL. 
* The char pointers pointed to by argv should not be modified or freed until commandv is no longer needed. */
int parse_args(int argc, char* const argv[], int* const commandc, char**** const commandv)
{
	// init cv with space for one char** vector and commandc to 0
	*commandc = 0;
	*commandv = NULL;
	// parse argv into seperate commands deliminated by '&'
	int curr_argc = 0;
	for (int i = 0; i <= argc; i++)
	{
		// if i is within range set curr_arg, otherwise set to NULL
		const char* curr_arg = NULL;
		if (i < argc)
			curr_arg = argv[i];
		else
			curr_arg = NULL;	
		if (i >= argc || strcmp(curr_arg, "&") == 0)
		{
			// increment commandc
			(*commandc)++;
			// add 1 to accomodate terminatig NULL pointer
			char** new_argv = malloc(sizeof(char*) * (curr_argc + 1));
			// copy commmand and respective arguments
			new_argv = memcpy(new_argv, argv + (i - curr_argc), sizeof(char*) * curr_argc);
			// terminate with NULL pointer
			new_argv[curr_argc] = NULL;
			// resize cv
			*commandv = realloc(*commandv, sizeof(char**) * *commandc);
			// point next pointer in vector to new_argv
			(*commandv)[*commandc - 1] = new_argv;

			curr_argc = 0;
			// increment i so that '&' is not included in arguments
		}
		else
		{
			curr_argc++;
		}
	}
	// NULL terminate *commandv
	*commandv = realloc(*commandv, sizeof(char**) * (*commandc + 1));
	(*commandv)[*commandc] = NULL;

	return 0;
}

// creates child processes and execute commands
// built_in_commands can be passed in parallel but will not run in parallel
int exec_commands(const int commandc, char*** const commandv)
{
	// counts children to be joined
	int childc = 0;
	// iterate through commands
	for (int i = 0; i < commandc; i++)
	{
		// if a built in command runs skip forking process and continue
		if (run_built_in(commandv[i]))
			continue;
		// create child process
		int pid = fork();
		if (pid == -1)
		{
			print_error("Failed to create child process\n");
			continue;
		}
		// if parent increment child counter
		else if (pid != 0)
		{
			childc++;
		}
		// else execute command
		else
		{
			// attempt to run built in command first
			if (run_built_in(commandv[i]))
				exit(0);
			// otherwise find program in path
			// find program
			char* path = NULL;
			find_path(commandv[i][0], &path);
			// check if program was found
			if (path == NULL)
			{
				print_error("Unrecognized command\n");
			}
			else
			{
				// if pointer to command is not NULL free and replace with full path
				if (commandv[i][0])
				{
					free(commandv[i][0]);
					commandv[i][0] = path;
					execv(path, commandv[i]);
				}
			}
			// ensure child has exited
			exit(0);
		}
	}
	// join child processes
	for (int i = 0; i < childc; i++)
	{
		wait(NULL);
	}
	return 0;
}

int main(int argc, char** argv) 
{
	// setup PATH
	PATH = malloc(sizeof(char) * (strlen("/bin/") + 1));
	strcpy(PATH, "/bin/");

	char* lineptr = NULL;
	size_t n = 0;
	while (1)
	{
		printf("wish> ");	
		// get input line
		if (getline(&lineptr, &n, stdin) == -1)
		{
			exit(0);
		}
		// split line into arguments
		char** wargv = NULL;
		int wargc = 0;
		get_args(lineptr, &wargv, &wargc);
		// parse arguments into vector of commands
		char*** commandv = NULL;
		int commandc = 0;
		parse_args(wargc, wargv, &commandc, &commandv);
		// execute commands
		exec_commands(commandc, commandv);
		// cleanup
		clean_args(wargc, &wargv);
		// TODO: clean_commands(commandc, &commandv);
	}	
	return 0;
}
