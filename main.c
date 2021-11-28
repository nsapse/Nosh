#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// a global variable to track FG_only mode
int FG_only = 0;

/******************************************************************************
 * Function:         int count()
 * Description:      counts the number of space separable tokens in a string
 * Where:
 *                   char inputstring[] - the input string to parse
 * Return:           int count - the count of tokens
 *****************************************************************************/
// counts the number of space separated tokens in an input string
int count(char inputString[]) {

  // copy the input string into a buffer to tokenize since
  // strtok is destructive and we will want later use of the string
  int len = strlen(inputString) + 1;
  char copyString[len];
  strcpy(copyString, inputString);
  char *token;
  char *savePtr;
  token = strtok_r(copyString, " ", &savePtr);

  // count the number of tokens produced before reaching null
  int count = 0;

  while (token != NULL) {
    count++;
    token = strtok_r(NULL, " \n", &savePtr);
  }
  // printf("number of tokens: %d", count);
  return count;
}

/******************************************************************************
 * Function:         char* expandInput
 * Description:      takes the user input string and expandInputs instances of
 *                   the variable $$ to be the PID of the shell
 * Where:
 *                   char inputString[] - the string to do expansions on
 * Return:			 a string where all instances of $$ have been
 *					 replaced with the current PID.
 *****************************************************************************/
char *expandInput(char inputString[]) {
  // get the len of the PID
  int PIDint = getpid();
  char PIDstr[10];
  sprintf(PIDstr, "%d", PIDint);
  char delim = '$';

  // create a temporary buffer to hold our replacement string
  char tempStr[2000];
  memset(tempStr, '\0', 2000 * sizeof(char));

  // loop through the input one character at a time these indices
  // keep track of our current place in the string and the starting
  // position to copy from
  int index = 0;

  while (index <= strlen(inputString)) {

    // if the next two characters are $$ then insert the PID and move forward
    // the index pointer 2 and the start pointer to match
    if (inputString[index] == delim && inputString[index + 1] == delim) {
      strcat(tempStr, PIDstr);
      index = index + 2;
    }

    // otherwise just copy the character at the pointer advance
    else {
      strncat(tempStr, inputString + index, 1);
      index++;
    }
  }

  // place the expanded string into heap allocated memory so it
  // can be returned without vanishing
  char *replacement_buffer;
  replacement_buffer = malloc(strlen(tempStr) * sizeof(char));
  memset(replacement_buffer, '\0', sizeof(*replacement_buffer));
  strcpy(replacement_buffer, tempStr);

  // return the expanded string for outside use
  return replacement_buffer;
}

/* a struct to hold an array with an accompanying size element
 * Includes:
 *			char **arr		a pointer to to an array of character
 *							arrays (strings)
 *
 *			int size 		the size of the array of string
 *							pointers for use by future functions to iterate
 *							through the array.
 * */

struct sizedArgArr {
  char **arr;
  int size;
};

/******************************************************************************
 * Function:        int parse
 * Description:		parses the input string passed to it and populates the
 *					args array with the args parsed
 * Where:
 *                  char inputString[] - the string we are parsing into
					arguments

 * Return:          struct sizedArgArr *argStruct - the struct containing an
					array of pointers to each argument string and an int 
					representing its size
 *****************************************************************************/
struct sizedArgArr *parse(char inputString[]) {
  int index = 0;
  int size = count(inputString);
  char *token;
  char *savePTR;

  // allocated on the heap for future return
  struct sizedArgArr *argStruct = malloc(sizeof(struct sizedArgArr));

  // create a blank pointer array of the size of tokesn
  // to hold them
  argStruct->arr = calloc(size + 1, sizeof(char *));

  token = strtok_r(inputString, " \n", &savePTR);

  // parse the string into tokens while a string remains
  while (token != NULL) {

    // check if a string needs to be expanded
    if (strstr(token, "$$") != NULL) {
      token = expandInput(token);
    }
    // place the tokens into an args array
    argStruct->arr[index++] = token;

    // must parse both on space and newline
    token = strtok_r(NULL, " \n ", &savePTR);
  }

  argStruct->size = index;
  return argStruct;
}

/*The struct to hold command information*/
struct procObj {
  char *command;
  char **args;
  char *input;
  char *output;
  int background;
  int parentProc;
};

/******************************************************************************
 * Function:        struct procObj *createInputObject
 *
 * Description:		Given an input string creates an output struct
 *					represeting a command, it's arguments, and other relevant information
 *					necessary for execution.
 *
 * Where:			char *input - a pointer to the input string
 *
 * Return:			struct procObj *command - a pointer to a command object representing
 *					the command to be executed by the shell
 *****************************************************************************/
struct procObj *createInputObject(char *input) {
  struct procObj *command = malloc(sizeof(struct procObj));

  // parse out and expand the input
  struct sizedArgArr *argStruct = parse(input);
  char **parsedInput = argStruct->arr;

  // create an array to hold all the arguments
  command->args = calloc(argStruct->size + 1, sizeof(char *));

  // check that the user didn't enter a comment or a null line
  // in that case just return a null command object which
  // will be skipped. This should never reach this point as there is already
  // an initial blank line check when gathering input but you can never be too
  // careful
  if (parsedInput[0] == NULL || strcmp(parsedInput[0], "#") == 0) {
    command->command = NULL;
    return command;
  }

  // initate the command portion of the struct on the heap so that it can be returned
  command->command = calloc(strlen(parsedInput[0]) + 1, sizeof(char));
  strcpy(command->command, parsedInput[0]);

  // loop through the input array and parse it out
  int i = 0;
  int argsIndex = 0;
  while (i < argStruct->size) {

    // take the current token in the array
    char *currTok = parsedInput[i];

    // initiate the input location (if specificed) otherwise leave it as null
    if (strcmp(currTok, "<") == 0) {
      char *inputStr = parsedInput[i + 1];
      command->input = calloc(strlen(inputStr) + 1, sizeof(char));
      strcpy(command->input, inputStr);

      // advance one extra to skip the next element
      i = i + 2;
      continue;
    }

    // initiate the output location (if specificed) otherwise leave it as null
    if (strcmp(currTok, ">") == 0) {
      char *outputStr = parsedInput[i + 1];
      command->output = calloc(strlen(outputStr) + 1, sizeof(char));
      strcpy(command->output, outputStr);

      // advance one extra to skip the next element
      i = i + 2;
      continue;
    }

    // determine whether or not the process is bg-ed by default it will not be
    command->background = 0;

    /*Check the following to determine if a process should be backgrounded:*/
    /*1 - If the FG only flag isn't set (if it is, not backgrounding)*/
    /*2 - Is the last character of the input a &*/

    if (i == argStruct->size - 1 && strcmp(currTok, "&") == 0) {
      if (FG_only == 0) {
        command->background = 1;
      }
      i = i + 1;
      continue;
    }

    // if no special conditions apply just record the arg as an arg in the args
    // array
	
    else {
      command->args[argsIndex++] = currTok;
      i++;
      continue;
    }
  }

  return command;
}

/******************************************************************************
 * Function:		int cd(struct procObj *command)
 *
 * Description:		changes into the directory specified by as the second
 *					arg in the args array of a command
 * Where:			struct procObj *comand - the command object generated
 *					by parsing user input.	
 *
 * Return:			an int indicating whether the change of directories was
 *					successful.
 *****************************************************************************/
int cd(struct procObj *command) {
  char workingDir[2048];
  char *changeDir;

  // allocate memory if the input exists
  if (command->args[1] != NULL) {
	// calloc so it's all null and I don't have to memset it
    changeDir = calloc(strlen(command->args[1]) + 1, sizeof(char));
    strcpy(changeDir, command->args[1]);
  }

  // otherwise set the directory to home
  else {
    changeDir = getenv("HOME");
  }

  // change directories
  int ret = chdir(changeDir);

  // if we fail then we should return an error
  if (ret != 0) {
    printf("directory change failed \n");
    getcwd(workingDir, 100);
    printf("current dir: %s \n", workingDir);
    fflush(stdout);
	return 1;
  }
  
  /*otherwise return 0 to indicated a successful cd*/
  return 0;
}

/******************************************************************************
 * Function:        executeInput
 * Description:		execute the command represented by a procObj
 * Where:			- procObj* command - the struct containing the information
 *					necessary to complete a command
 *					- int *exitStatus - a pointer to the int representing the exit
 *					status of the shell's last command.
 *					- sigaction INTact - the sigaction struct associated with SIGNINT
 *					- sigaction STPact - the sigaction struct associated with SIGTSP
 *
 * Return:			the PID of the command executed
 *****************************************************************************/
int executeInput(struct procObj *command, int *exitStatus,
                 struct sigaction INTact, struct sigaction STPact) {

  // skip null commands
  if (command->command == NULL) {
    return 0;
  }

  // special path for CD
  if (strcmp(command->command, "cd") == 0) {
    int cmdStatus = cd(command);
    return cmdStatus;
  }

  // otherwise fork then call execvp
  else {

    // int to hold the return status generated by WAITPID
    int childStatus;

    // fork a new process and check that it succeeded
    // attempt to fork the process
    pid_t childPID = fork();

    if (childPID < 0) {
      // Code in this branch will be exected by the parent when fork() fails and
      // the creation of child process fails as well
      perror("fork() failed!");
      exit(1);
    }

    // childPID is 0. This means we are in the child process and the child will
    // execute the code in this branch
    if (childPID == 0) {

      // install a signal handler to allow for SIGINT in FG procs
      if (command->background == 0) {
        INTact.sa_handler = SIG_DFL;
        sigaction(SIGINT, &INTact, NULL);
      }

      // install a signal handler for SIGTSTP to just ignore it
      STPact.sa_handler = SIG_IGN;
      sigaction(SIGTSTP, &STPact, NULL);

      // /*: redirect I/O if non-STDIN/STDOUT specified */
      // if the given input string is not null redirect input
      if (command->input != NULL) {
        int sourceFD = open(command->input, O_RDONLY);
        if (sourceFD == -1) {
          perror("source open()");
          return 1;
        }

        // attempt the redirection and exit if it fails
        int result = dup2(sourceFD, 0);
        if (result == -1) {
          perror("source dup2()");
          return 1;
        }

        fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
      }

      // if the given output string is not null redirect output
      if (command->output != NULL) {
        int targetFD =
            open(command->output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (targetFD == -1) {
          perror("target open()");
          return 1;
        }

        // Redirect stdout to target file
        int result = dup2(targetFD, 1);
        if (result == -1) {
          perror("target dup2()");
          return 1;
        }
        fcntl(targetFD, F_SETFD, FD_CLOEXEC);
      }

      // if I/O file descriptors not set AND BG flag set redirect to /dev/null
      // redirects input to /dev/null if no other input set
      if (command->input == NULL && command->background == 1) {

        int sourceFD = open("/dev/null", O_RDONLY);
        if (sourceFD == -1) {
          perror("source open()");
          return 1;
        }

        // attempt the redirection and exit if it fails
        int result = dup2(sourceFD, 0);
        if (result == -1) {
          perror("source dup2()");
          return 1;
        }
        printf("redirected input to /dev/null");
        fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
      }

      // redirects output to /dev/null if no other output set
      if (command->output == NULL && command->background == 1) {

        int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (targetFD == -1) {
          perror("target open()");
          exit(1);
        }

        // Redirect stdout to target file
        int result = dup2(targetFD, 1);
        if (result == -1) {
          perror("target dup2()");
          exit(2);
        }
        printf("redirected output to /dev/null");
        fflush(stdout);
        fcntl(targetFD, F_SETFD, FD_CLOEXEC);
      }

      // execute the passed comand in its own thread.
      int execReturn;
      execReturn = execvp(command->command, command->args);

      // print error message (and return 1) if execution fails
      if (execReturn == -1) {
        perror("source - exec");
        printf("Execution of %s failed \n", command->command);

        // this line is to make sure failed processes are killed and reaped
        // they previously were remaining despited waitPIDing them and lived
        // as zombies in the backround.
        kill(getpid(), SIGKILL);

        // return that the execution failed
        return childPID;
      }

    }

    // spawnpid is the pid of the child. This means the parent will execute
    // the code in this branch

    else {
      // check the background flag - if set return the PID to track for later
      // termination
      if (command->background == 1) {

        // if backgrounding return the PID
        int termination = waitpid(childPID, &childStatus, WNOHANG);
        printf("backgrounded PID is: %d\n", childPID);
        fflush(stdout);
        return childPID;

      }

      // otherwise wait for the process to terminate then return
      else {

        // point to exitStatus since this is a FG process for status
        waitpid(childPID, exitStatus, 0);

        // check how it exited and set the exit status variable
        if (WIFEXITED(*exitStatus)) {
          *exitStatus = WEXITSTATUS(*exitStatus);
        } else {
          *exitStatus = WTERMSIG(*exitStatus);
          // if killed by a signal we want to alert the user of which
          printf("Process killed by signal: %d \n", *exitStatus);
        }
      }
    }

    // this is to keep the compiler from complaining
    return 0;
  }
}

// struct to track background processes
// has an INT to represent the BGPID and 
// a pointer to the next one
struct bgProc {
  int PID;
  struct bgProc *next;
};

// LL initializer
/******************************************************************************
 * Function:			createLL
 * Description:			initate a LL to track bgProcs	
 * Where:				void
 *
 * Return:				head - a bgProc struct representing the head of the LL
 *****************************************************************************/
struct bgProc *createLL(void) {
  struct bgProc *head = malloc(sizeof(struct bgProc));

  head->PID = -1;
  head->next = NULL;

  return head;
}

// add to LL
/******************************************************************************
 * Function:		addBgProc
 * Description:		adds a background process to the tracking LL
 * Where:			*head - a pointer to the head of the LL
 *					newPID - the PID of the process to add
 * Return:			int representing successful addition
 *****************************************************************************/
int addBgProc(struct bgProc *head, int newPID) {
  // instantiate a new node with the given pid
  struct bgProc *newProc = malloc(sizeof(struct bgProc));
  newProc->PID = newPID;
  newProc->next = NULL;


  // iterate to the end of the list
  while (head->next != NULL) {
    head = head->next;
  }

  // attach the new node to the end of the list
  head->next = newProc;

  // return 1 to indicate success
  return 1;
}

// function to print LL PIDS
/******************************************************************************
 * Function:		showPIDS 
 * Description:		a fancy print function for the backgrounded PIDs
 * Where:			bgProc *head - the head of the bg tracking LL
 * Return:			void
 *****************************************************************************/
void showPIDs(struct bgProc *head) {
  while (head != NULL) {
    printf("bPID: %d \n", head->PID);
    head = head->next;
  }
}

// function to remove dead processes from LL
/******************************************************************************
 * Function:		removeNext
 * Description:		given a node removed the the node following it from the 
 *					proc tracking LL by pointing the current pointer past it
 * Where:			bgProc * node - the node we want to remove the successor to
 * Return:			void
 *****************************************************************************/
void removeNext(struct bgProc *node) { node->next = node->next->next; }

// function to clear dead processes
/******************************************************************************
 * Function:		clearDefunct
 * Description:		clears defunct processes from the background cache
 * Where:			bgProc *head - the head of the LL to clear procs from
 * Return:			void
 *****************************************************************************/
void clearDefunct(struct bgProc *head) {
  // a location to write exit information for waitPID
  int exitStatus;
	
  // we know the head of the list has bunk data
  // we don't want to look at. Set the actual head to the
  // node immediately following it and set the initial prev
  // node to head
  struct bgProc *prev = head;
  head = head->next;
	
  // traverse the list and call waitPID to check if a process has terminated
  while (head != NULL) {
    if (head->PID != -1 && head->PID != 1) {
      int termination = waitpid(head->PID, &exitStatus, WNOHANG);

	  // if a process has terminated alert the user and let the know the 
	  // exit status
      if (termination != 0) {
        int termStatus = WEXITSTATUS(exitStatus);
        printf("Process %d successfully completed and will be cleared. Exit "
               "Status: %d \n",
               head->PID, termStatus);
        removeNext(prev);
      }
    }
	
	// continue on until we reach the end of the loop
    prev = head;
    head = head->next;
  }
}

/******************************************************************************
 * Function:         void exitShell
 * Description:      exits the shell
 * Where:
 *					 struct bgProc *head - the head of the list of background processes 
 * Return:           returns 0 to connote successful exiting 
 *****************************************************************************/
void exitShell(struct bgProc *head) {
  while (head != NULL) {
	// we have to kill all bgprocs before we exit
    if (head->PID != 1 && head->PID != -1) {
      kill(head->PID, SIGKILL);
    }
    head = head->next;
  }

  // once they're all dead we can just call exit
  exit(0);
}

// Function to handle SIGTSTP
/******************************************************************************
 * Function:		handle_SIGTSTP
 * Description:		a handler for the SIGTSTP signal that toggles global foreground
 *					mode.
 * Where:			int signo - the signal being recieved
 *
 * Return:			void
 *****************************************************************************/
void handle_SIGTSTP(int signo) {

	// if the FG_only flag is set to false toggle it to true and let the user know
	// the mode has changed.
  if (FG_only == 0) {
    char *message = "Entering foreground-only mode (& is now ignored)\n";
    write(STDOUT_FILENO, message, 50);
    FG_only = 1;
  } 

	// if the FG_only flag is set to true toggle it to false eand let the user know
	// the mode has changed.
  else {
    char *message = "Exiting foreground-only mode\n";
    write(STDOUT_FILENO, message, 29);
    FG_only = 0;
  }
}

// Main Loop
int main(void) {

  // instantiate the memory for the input and args array
  char userInput[2048];

  // the prompt
  char *prompt = ":";

  // linked list to store background processes
  struct bgProc *bgProcs = createLL();

  // int to store exit status
  int exitStatus = 0;

  // Register a signal handler for SIGINT (ignore for shell and BG
  // but execute for FG procs)
  struct sigaction SIGINT_action = {0};
  SIGINT_action.sa_handler = SIG_IGN;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = SA_RESTART;

  sigaction(SIGINT, &SIGINT_action, NULL);

  // register a signal handler for SIGSTP
  struct sigaction SIGTSTP_action = {0};
  SIGTSTP_action.sa_handler = handle_SIGTSTP;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = SA_RESTART;

  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

  // the main user input loop
  while (1) {

    // waidPID/free dead procs before taking input
    clearDefunct(bgProcs);

    // clear the input buffer and read into it
    memset(userInput, '\0', sizeof(*userInput));
    write(STDOUT_FILENO, prompt, 2);

    // read with fgets to allow spaces
    fgets(userInput, 2048, stdin);

    // check for a comment or blank input
    if (userInput[0] == '#' || userInput[0] == ' ' || userInput[0] == '\n') {
      continue;
    }

    // otherwise parse and execute the input

    else {
      // make the process Object from the input
      struct procObj *command = malloc(sizeof(struct procObj));
      command = createInputObject(userInput);

      // exit command
      if (strcmp(command->command, "exit") == 0) {
        printf("Exiting \n");
        fflush(stdout);
        exitShell(bgProcs);
      }

      // status command
      if (strcmp(command->command, "status") == 0) {
        printf("%d \n", exitStatus);
        continue;
      }

      // execute the command contained in the struct
      int respPID = executeInput(command, &exitStatus, SIGINT_action, SIGTSTP_action);

      // if the PID returned wasn't zero then store it for later termination
      if (respPID != 0) {
        addBgProc(bgProcs, respPID);
      }
    }
  }
  free(bgProcs);
  return 0;
}
