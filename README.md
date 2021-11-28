# NoSH :: Noah's Shell

The following is my (largely successful) attempt to create a toy shell using standard C and linux system API calls. It does the following:

- Provides the user with a prompt (':') to allow them to run commands (fairly essential).
- Provides basic variable expansion for the string '$$' which is substituted with the PID of the calling process.
- Allows backgrounding of processes with the '&' character
- Implements hand written versions of:
	- exit - kills all background processes and then exits
	- cd - changed directories as expected
	- status - provides the exit status of the previous commands
- Allows the user to execute any other binaries found within the $PATH directory using exec().
- Implements intput and output redirection from scratch using dup().
- Implements custom signal handlers and background/foreground responses for SIGINT and SIGSTP.

NoSH is a work in progress. It is definitely rough around the edges and I continue to work out its bugs as time allows.
