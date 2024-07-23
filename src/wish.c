#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

char* PATH = NULL;

// print an error message
int print_error(const char* error_message)
{
	write(STDERR_FILENO, error_message, strlen(error_message));
	return 0;
}

/* Given an arg_string, updates argv and returns argument count. 
 * argv should be uninitialized or NULL and terminated by a NULL pointer.
 * The user should call clean_args after each call to get_args in order to free memory. */
int get_args(char* arg_string, char*** argv)
{
	int argc = 0;

	// Calculate the number of arguments
	char* c = arg_string;
	bool prev_was_char = false;
	while(*c != '\0')
	{
		// if c points to the start of a new argument
		if (!prev_was_char && (*c != ' ' && *c != '\n' && *c != '\t'))  
		{
			argc++;
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
	*argv = malloc((argc + 1) * sizeof(char*));

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
	(*argv)[argc] = NULL;

	return argc;
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
int run_built_in(int argc, char** argv) 
{
	if (argc == 0)
	{
		return 0;
	}

	char* command = argv[0];
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

		int path_len = 0;
		for (int i = 1; i < argc; i ++)
		{
			path_len += strlen(argv[i]) + 1;
		}

		PATH = realloc(PATH, sizeof(char) * path_len);
		PATH[0] = '\0';
		for (int i = 1; i < argc; i++)
		{
			strcat(PATH, argv[i]);
			strcat(PATH, ":");
		}
		PATH[path_len - 1] = '\0';

		return 1;
	}
	// check if exit command was entered
	if (strcmp(command, "exit") == 0)
	{
		exit(0);
	}
	return 0;
}

// given argument list, and execute command
void exec_command(int argc, char** argv)
{
	char* command = argv[0];
	// return if no commands were entered
	if (argc == 0)
	{
		exit(0);
	}
	// look for program in path and execute
	char* path_cpy = malloc(sizeof(char) * (strlen(PATH) + 1));
	strcpy(path_cpy, PATH);
	while(path_cpy != NULL)
	{
		const char* const dir = strsep(&path_cpy, ":");
		// append command to ith directory in pathv
		char* const path = malloc(sizeof(char) * (strlen(dir) + strlen(command) + 1));
		strcpy(path, dir);
		strcat(path, command);

		if (!access(path, X_OK))
		{
			// prepare execv call
			// first element of argv should be changed to full path
			argv[0] = path;
			// ensure argv is terminated with NULL pointer
			if (argv[argc - 1] != NULL)
			{
				argv = realloc(argv, sizeof(char*) * (argc + 1));
				argv[argc] = NULL;
			}
			free(path_cpy);
			execv(path, argv);
			exit(0);
		}
	}	
	free(path_cpy);
	print_error("Unrecognized command\n");
	exit(0);
}

// parses and processes input line
int process_input(char* input)
{
	char** argv = NULL;
	// parse all arguments from input string into a vectory
	const int total_argc = get_args(input, &argv);
	// run built in commands first
	if (run_built_in(total_argc, argv))
	{
		return 0;
	}
	// used to when joining child processes
	int childrenc = 0;
	int pid = -1;
	// used to count arguments for each command including the command itself
	int argc = 0;
	// used to traverse through wish_argv
	char** argv_pos = argv;
	while (*argv_pos != NULL && pid != 0)
	{
		if (strcmp(*argv_pos, "&") == 0)
		{
			pid = fork();
			if (pid == -1)
			{
				print_error("Fork failed\n");
			}
			else if (pid == 0)
			{
				// make copy of argv so that exec_command receives a pointer returned by malloc
				char** argv_cpy = malloc((argc + 1) * sizeof(char*));
				argv_cpy = memcpy(argv_cpy, argv_pos - argc, (argc + 1) * sizeof(char*));
				// NULL terminate array
				argv_cpy[argc] = NULL;
				// execute command
				exec_command(argc, argv_cpy);
				free(argv_cpy);
			}
			argc = 0;
			childrenc++;
		}
		else
		{
			argc++;
		}
		argv_pos++;
	}
	if (argc > 0 && pid != 0)
	{
		pid = fork();
		if (pid == -1)
		{
			print_error("Fork failed\n");
		}
		else if (pid == 0)
		{
			// make copy of argv so that exec_command receives a pointer returned by malloc
			char** argv_cpy = malloc((argc + 1) * sizeof(char*));
			argv_cpy = memcpy(argv_cpy, argv_pos - argc, (argc + 1) * sizeof(char*));
			// NULL terminate array
			argv_cpy[argc] = NULL;
			// execute command
			exec_command(argc, argv_cpy);
			free(argv_cpy);
		}
		childrenc++;
	}
	// join child processes
	for (int i = 0; i < childrenc; i++)
	{
		wait(NULL);
	}
	clean_args(total_argc, &argv);

	return 0;
}

int main(int argc, char** argv) 
{
	// setup PATH
	PATH = malloc(sizeof(char) * (strlen("/bin/") + 1));
	strcpy(PATH, "/bin/");
	// parse commands and arguments
	char* lineptr = NULL;
	size_t n = 0;
	while (1)
	{
		printf("wish> ");	
		if (getline(&lineptr, &n, stdin) == -1)
		{
			exit(0);
		}
		process_input(lineptr);
	}	
	return 0;
}

