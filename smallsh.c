#define _POSIX_SOURCE
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<readline/readline.h>
#include<readline/history.h>

size_t MAX_ARG = 512;
size_t MAX_STR = 2048;
int STATUS = 0;
int ACTIVE_PID = 0;

// flags for allowing background processes and printing alert message
int BG = 1;
int BG_MSG = 0;

// array of background processes
pid_t BG_PID[20];

void
shell_SIGINT()
{
    // print msg for killing foreground child with ctrl c
    if(ACTIVE_PID){write(STDOUT_FILENO, "FOREGROUND CHILD PROCESS TERMINATED BY SIGNAL 2\n", 48);}
}

void
shell_SIGTSTP()
{
    // function changes both BG flags when triggered by ctrl z
    if(BG){BG = 0;}
    else{BG = 1;}
    BG_MSG = 1;
}

void
shell_BG_alert()
{
    // prints alert message based on BG flag
    // sets BG_MSG flag so alert isnt repeated
    if(BG){fprintf(stdout, "FOREGROUND-ONLY MODE DISABLED\n");}
    else{fprintf(stdout, "FOREGROUND-ONLY MODE ENABLED (& IGNORED)\n");}
    fflush(stdout);
    BG_MSG = 0;
}

void
shell_exit()
{
    // exit shell killing all active BG_PID in a for-loop

    for(int i=0; i<20; i++)
    {
        if(BG_PID[i])
        {
            kill(BG_PID[i], SIGKILL);
        }
    }

    exit(0);
}

void
shell_daycare()
{
    // loop through all saved BG_PID and update status
    // if process is gone, sets index to 0 freeing space
    int ChildStatus = 0;
    pid_t pid;
 
    for(int i=0; i<20; i++)
    {
        if(BG_PID[i])
        {   
            // ENDED WITH EXIT
            pid = waitpid(BG_PID[i], &ChildStatus, WNOHANG | WUNTRACED);
            if(pid == BG_PID[i] && !WTERMSIG(ChildStatus))
            {
                fprintf(stdout, "CHILD PROCESS PID %d COMPLETE - STATUS %i\n", BG_PID[i], WEXITSTATUS(ChildStatus));
                fflush(stdout);
                BG_PID[i] = 0;
            }
            // ENDED WITH A TERMINATING SIGNAL
            else if(pid == BG_PID[i] && WTERMSIG(ChildStatus))
            {
                fprintf(stdout, "CHILD PROCESS PID %d TERMINATED - SIGNAL %i\n", BG_PID[i], WTERMSIG(ChildStatus));
                fflush(stdout);
                BG_PID[i] = 0;
            }
        }
	}
}

int
shell_enroll(pid_t ChildPid)
{
    // find free space in BG_PID
    // if BG_PID is full up the new process is terminated
    for(int i=0; i<20; i++)
    {
        if(!BG_PID[i])
        {
            BG_PID[i] = ChildPid;
            return 0;
        }
    }

    kill(ChildPid, SIGKILL);
    fprintf(stderr, "MAXIMUM BACKGROUND PROCESSES REACHED");
    fflush(stderr);
    return 1;
}

void
shell_output(char *OutFile)
{
    if(!OutFile){return;}

    // open file 
    int file = open(OutFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
	if (file == -1){fprintf(stderr, "OPEN OUTPUT FAILED\n"); exit(1);}

    // set to stdout
    int open = dup2(file, 1);
	if (open == -1){fprintf(stderr, "OPEN OUTPUT FAILED\n"); exit(1);}
}

void
shell_input(char *InFile)
{
    if(!InFile){return;}

    // open file
    int file = open(InFile, O_RDONLY);
	if (file == -1){fprintf(stderr, "OPEN INPUT FAILED\n"); exit(1);}

    // set to stdout
    int open = dup2(file, 0);
	if (open == -1){fprintf(stderr, "OPEN INPUT FAILED\n"); exit(1);}
}

char *
shell_expand(char *UserCommand)
{
    // function used to expand the variable "$$" to the shell's pid
    // iterates through all characters in UserCommand and either copies
    // them to ExpandedCommand or concantenates the PID as a string to
    // ExpandedCommand if two '$' are found in a row

    pid_t pid = getpid();
    char *pidstr;
    size_t PidLen = snprintf(NULL, 0, "%d", pid);

    pidstr = malloc((PidLen + 1) * sizeof *pidstr);
    sprintf(pidstr, "%d", pid);

    size_t length = strlen(UserCommand);
    size_t NewLen = length;
    char *ExpandedCommand = malloc(sizeof(char) * length);

    for(size_t i; i<length; i++)
    {
        if(UserCommand[i] == '$' && UserCommand[i+1] == '$')
        {
            i++;
            NewLen = NewLen + PidLen;
            ExpandedCommand = realloc( ExpandedCommand, (NewLen * sizeof(char *)) );
            strcat(ExpandedCommand, pidstr);
        }
        else
        {
            ExpandedCommand = strncat(ExpandedCommand, &UserCommand[i], 1);
        }
    }

    return ExpandedCommand;
}

void
shell_fork(char **ArgArray, char *InFile, char *OutFile, int background, size_t ArrayLen)
{
    // forks into child processes using a switch-case
    // will either halt untill child is complete or continue
    // with the child as a background process

    if(ArrayLen>MAX_ARG)
    {
        fprintf(stderr, "TOO MANY ARGUMENTS\n");
        return;
    }

    pid_t ChildPid = fork();

    switch(ChildPid)
    {   
        //a forking error
        case -1:
            fprintf(stderr, "FORK FAILED\n");

        //child path
        case 0:
            signal(SIGTSTP, SIG_IGN);
            if(!background){signal(SIGINT, SIG_DFL);}

            shell_input(InFile);
            shell_output(OutFile);

            execvp(ArgArray[0], ArgArray);
            fprintf(stderr, "UNABLE TO EXECUTE COMMAND %s\n", ArgArray[0]);
            exit(1);

        //parent path
        default:
            if(!background)
            {
                // foreground child
                ACTIVE_PID = 1;
                waitpid(ChildPid, &STATUS, 0);
                ACTIVE_PID = 0;
            }
            else
            {
                // background child
                if(shell_enroll(ChildPid)){return;}
                fprintf(stdout, "BACKGROUND CHILD PROCESS %d STARTED\n", ChildPid);
                fflush(stdout);
            }
    }
}

void
shell_cd(char **ArgArray, size_t ArrayLen)
{
    // use chdir() to replicate functionality of cd
    if(ArrayLen==1)
    {
        char *home = getenv("HOME");
        chdir(home);
    }
    else if(ArrayLen==2)
    {
        chdir(ArgArray[1]);
    }
    else
    {
        fprintf(stderr, "TOO MANY ARGUMENTS\n");
    }
}

void
shell_status()
{   
    // prints last terminating status code
    fprintf(stdout, "%i\n", WEXITSTATUS(STATUS));
    fflush(stdout);
}

int
shell_parse(char *UserInput)
{   
    // splits UserInput into tokenized strings and sets
    // them to more managable variables
    // BUILT IN: exit, cd, status (see readme)
    // sends all other commands to shell_fork to be run with exec()

    if(!strcmp(UserInput, "")){return 1;}
    if(UserInput[0] == '#'){return 1;}
    
    char **ArgArray = malloc(sizeof(char *));
    char *InFile = NULL;
    char *OutFile = NULL;
    int background = 0;

    char *Token = strtok(UserInput, " ");
    size_t ArrayLen = 0;

    while(Token)
    {   
        if(!strcmp(Token, "&"))
        {
            background = 1;
            Token = strtok(NULL, " ");

            if(Token || !BG){background = 0;}
        }

        else if(!strcmp(Token, "<"))
        {
            Token = strtok(NULL, " ");
            if(Token)
            {
                InFile = Token;
                InFile = shell_expand(InFile);
                Token = strtok(NULL, " ");
            }
        }

        else if(!strcmp(Token, ">"))
        {
            Token = strtok(NULL, " ");
            if(Token)
            {
                OutFile = Token;
                OutFile = shell_expand(OutFile);
                Token = strtok(NULL, " ");
            }
        }

        else
        {
            // no token found, just an argument/command
            ArrayLen++;
            ArgArray = realloc( ArgArray, ((ArrayLen) * sizeof(char *)) );
            
            ArgArray[ArrayLen - 1] = malloc(strlen(Token) + 1);
            strcpy(ArgArray[ArrayLen - 1], Token);

            Token = strtok(NULL, " ");
            ArgArray[ArrayLen - 1] = shell_expand(ArgArray[ArrayLen - 1]);
        }
    }

    //Null terminate
    ArgArray = realloc( ArgArray, ((ArrayLen+1) * sizeof(char *)) );
    ArgArray[ArrayLen] = malloc(sizeof(char *));
    ArgArray[ArrayLen] = '\0';

    //branch to built-ins or fork + exec
    if(!strcmp(ArgArray[0], "exit")){shell_exit();}
    else if(!strcmp(ArgArray[0], "cd")){shell_cd(ArgArray, ArrayLen);}
    else if(!strcmp(ArgArray[0], "status")){shell_status();}
    else{shell_fork(ArgArray, InFile, OutFile, background, ArrayLen);}

    return 1;
}

int main()
{
    char UserInput[MAX_STR];

    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};
	SIGINT_action.sa_handler = shell_SIGINT;
    SIGTSTP_action.sa_handler = shell_SIGTSTP;
	sigfillset(&SIGINT_action.sa_mask);
    sigfillset(&SIGTSTP_action.sa_mask);
	SIGINT_action.sa_flags = 0;
    SIGTSTP_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    while(1)
    {   
        // check if ctrl z was hit - send text alert
        if(BG_MSG){shell_BG_alert();}

        // prompt
        fprintf(stderr, ":");

        // set input to UserInput and get rid of linefeed
        fgets(UserInput, sizeof(UserInput), stdin);
        UserInput[strcspn(UserInput, "\n")] = 0;

        // format UserInput and check on the kids
        shell_parse(UserInput);
        shell_daycare();

        // refresh UserInput
        UserInput[0] = '\0';
    }
}
