# shell-project
diy unix shell implementing program execution, parallel processes, file redirection, and built in commands: "exit", "cd", and "path"

## Installation
Compile with a c compiler.
For example using gcc: `> gcc -o wish wish.c`

## Built-in functions
- cd: call with one argument specifying directory to move into that directory
- path: call with any number of path arguments to designate directories in path.
A call to path will overwrite the old path variable so `> path` will remove all directories from path variable.
- exit: exits the shell

## Parallel Execution
- Deliminate commands and their arguments with '&' to run them in parallel
- Built-in functions can be called this way for convenience, but will not be run in parallel

## Redirection
- Appending a command with ` > file` will redirect standard output and standard error to file
- If file the file exists, it will be overwritten. If not, it will be created.
- If executing parallel commands, each command can and must have its own redirection if one is desired.
- Redirecting the output of multiple parallel commands to the same file results in undefined behavior
