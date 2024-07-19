#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PATH "./.wish_path"

// updates path_arr and path_arr_size
// path_arr should be uninitialized or NULL
// user is responsible for freeing path_arr if desired
int get_path(size_t* path_arr_size, char*** path_arr)
{
	// allocate array to hold path and add "/bin/" to path by default
	*path_arr = malloc(sizeof(char*));
	*path_arr_size = 1;
	// 1 is added to number of allocated bytes to account for terminating null byte
	(*path_arr)[0] = malloc(sizeof(char) * (strlen("/bin/") + 1));
	strcpy((*path_arr)[0], "/bin/");

	FILE* path_fd = fopen(PATH, "r");
	if (path_fd == NULL)
	{
		return 1;
	}
	char* lineptr = NULL;
	size_t n = 0;
	// count is compared to path_arr_size to determine when a reallocation is needed
	int count = 1;
	while (getline(&lineptr, &n, path_fd) != -1)
	{
		// strip '\n' char from end of line
		const size_t last_char = strlen(lineptr) - 1;
		if (lineptr[last_char] == '\n')
		{
			lineptr[last_char] = '\0';
		}
		// "/bin/" is already in path by default
		if (strcmp(lineptr, "/bin/") != 0)
		{
			// resize path_arr if necassary
			if (++count > *path_arr_size)
			{
				*path_arr_size *= 2;
				*path_arr = realloc(*path_arr, sizeof(char*) * *path_arr_size);
			}
			// allocate new string
			(*path_arr)[count - 1] = malloc(sizeof(char) * (strlen(lineptr) + 1));
			strcpy((*path_arr)[count - 1], lineptr);
		}
	}
	fclose(path_fd);
	
	return 0;
}
/* Given an arg_string, updates argv and returns argument count. 
 * argv should be uninitialized or NULL.
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
	*argv = malloc(argc * sizeof(char*));

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
	char* command = argv[0];
	// check if cd command was enetered
	if (strcmp(command, "cd") == 0)
	{
		// print error if incorrect number of arguments
		if (argc != 2)
		{
			fprintf(stderr, "Usage: cd <directory>\n");
		}
		else if (chdir(argv[1]) == -1)
		{
			fprintf(stderr, "Error changing directory\n");
		}
		return 1;
		
	}
	// check if exit command was entered
	if (strcmp(command, "exit") == 0)
	{
		exit(0);
	}
	return 0;
}

// given argument list execute command
// argv will be modified 
int process_command(int argc, char*** argv, size_t* pathc, char*** pathv)
{
	if (run_built_in(argc, *argv))
	{
		return 0;
	}
	char* command = (*argv)[0];
	// return if no commands were entered
	if (argc == 0)
	{
		return 0;
	}
	// look for program in path and execute
	for (int i = 0; i < *pathc; i++)
	{
		// append command to ith directory in pathv
		char* path = malloc(sizeof(char) * (strlen((*pathv)[i]) + strlen(command) + 1));
		strcpy(path, (*pathv)[i]);
		strcat(path, command);

		if (!access(path, X_OK))
		{
			pid_t child = fork();	
			if (child == 0)
			{
				// prepare execv call
				// first element of argv should be changed to full path
				(*argv)[0] = path;
				// ensure argv is terminated with NULL pointer
				if ((*argv)[argc - 1] != NULL)
				{
					*argv = realloc(*argv, sizeof(char*) * (argc + 1));
					(*argv)[argc] = NULL;
				}

				execv(path, *argv);
				exit(0);
			}
			else if (child != -1)
			{
				wait(NULL);
			}
			else
			{
				fprintf(stderr, "fork error, could not run %s\n", command);
			}
			
			free(path);
			return 0;
		}

	}	

	fprintf(stderr, "%s is unrecognized command\n", (*argv)[0]);
	return 0;
}

int main(int argc, char** argv) 
{
	// get path array and size
	size_t path_arr_size = 0;
	char** path_arr = NULL;
	if (get_path(&path_arr_size, &path_arr) == 1)
	{
		printf("%s does not exist\n", PATH);
	}	

	// parse commands and arguments
	char* lineptr = NULL;
	size_t n = 0;
	char** wish_argv;
	while (1)
	{
		printf("wish> ");	
		if (getline(&lineptr, &n, stdin) == -1)
		{
			free(lineptr);
			exit(0);
		}

		const int wish_argc = get_args(lineptr, &wish_argv);
		const int process_output = process_command(wish_argc, &wish_argv, &path_arr_size, &path_arr);
		clean_args(wish_argc, &wish_argv);

		if (process_output)
		{
			free(lineptr);
			exit(0);
		}
	}	
	return 0;
}

