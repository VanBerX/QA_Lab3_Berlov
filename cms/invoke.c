#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


#include "invoke.h"

int invoke_command(char * const *argv, const char *input, int inputlen)
{
    int res, fd[2], pid, wpid, wc, written, status;
    res = pipe(fd);
    if(res == -1)
        return -1;
    pid = fork();
    if(pid == -1) {
        close(fd[0]);
        close(fd[1]);
        return -1;
    }
    if(pid == 0) { /* child */
        close(fd[1]);
        dup2(fd[0], 0);
        close(fd[0]);
        execvp(*argv, argv);
        _exit(253);
    }
    close(fd[0]);
    written = 0;
    while(written < inputlen) {
        wc = write(fd[1], input + written, inputlen - written);
        if(wc == -1)
            break;
        written += wc;
    }
    close(fd[1]);
    do {
        wpid = wait(&status);
    } while(wpid != pid && wpid != -1 /* paranoia check */);
    if(!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return -2;
    return 0;
}
