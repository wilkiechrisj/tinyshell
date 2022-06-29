  ____                  _ _   ____  _     
 / ___| _ __ ___   __ _| | | / ___|| |__  
 \___ \| '_ ` _ \ / _` | | | \___ \| '_ \ 
  ___) | | | | | | (_| | | |  ___) | | | |
 |____/|_| |_| |_|\__,_|_|_| |____/|_| |_|

A basic shell client that uses exec() functions to execute commands.

Christopher Wilkie
wilkiech@oregonstate.edu
CS_344_400

Compiled using GCC.
gcc -std=c99 -g -Wall -Wextra -Wpedantic -Werror -o smallsh smallsh.c


-COMMAND PROMPT-

command [arg1 arg2 ...] [< input_file] [> output_file] [&]

MAX 2048 CHARACTERS
MAX 512 ARGUMENTS

If < is given as an argument the next argument will be used as the input file
if > is given as an argument the next argument will be used as the output file
An & placed as the last argument will execute the command as a background process
$$ in any argument will be expanded to the shell's PID
Comments can be entered using #
Blank command lines are ignored


-BUILT IN COMMANDS-

exit
terminate all background processes and exit the shell

cd
change directory. supports absolute and relative path

status
prints the exit status of the last foreground process


-SIGNALS-

SIGINT (CTRL C)
terminates the foreground process (not the shell) and prints
the PID and terminating signal

SIGTSTP (CTRL Z)
Disables the ability to run commands in the background - & will be ignored
as the last argument