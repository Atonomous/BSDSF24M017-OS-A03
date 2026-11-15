#include "shell.h"
#include <fcntl.h>

/* Runs in the child: apply < and > for one command. */
static void setup_redirects(command_t* c) {
    if (c->infile) {
        int fd = open(c->infile, O_RDONLY);
        if (fd < 0) { perror(c->infile); exit(1); }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (c->outfile) {
        int fd = open(c->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) { perror(c->outfile); exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

int execute_pipeline(pipeline_t* p) {
    int n = p->ncmds;
    int prev_read = -1;          /* read end of the previous pipe */
    pid_t pids[MAX_CMDS];
    int forked = 0;

    for (int i = 0; i < n; i++) {
        int fd[2] = { -1, -1 };

        if (i < n - 1 && pipe(fd) < 0) {         /* every stage but the last gets a pipe */
            perror("pipe");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (fd[0] != -1) { close(fd[0]); close(fd[1]); }
            break;
        }

        if (pid == 0) {                          /* ---- child ---- */
            if (prev_read != -1) {               /* stdin <- previous pipe */
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (fd[1] != -1) {                   /* stdout -> next pipe */
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }
            setup_redirects(&p->cmds[i]);        /* < and > override the pipe */
            execvp(p->cmds[i].argv[0], p->cmds[i].argv);
            perror(p->cmds[i].argv[0]);
            exit(1);
        }

        /* ---- parent ---- */
        pids[forked++] = pid;
        if (prev_read != -1) close(prev_read);   /* done with the old read end */
        if (fd[1] != -1) close(fd[1]);           /* parent must NOT hold the write end */
        prev_read = fd[0];
    }

    if (prev_read != -1) close(prev_read);       /* only set if we broke out early */

    for (int i = 0; i < forked; i++)
        waitpid(pids[i], NULL, 0);
    return 0;
}

int execute(char* arglist[]) {
    int status;
    int cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0: // Child process
            execvp(arglist[0], arglist);
            perror("Command not found"); // This line runs only if execvp fails
            exit(1);
        default: // Parent process
            waitpid(cpid, &status, 0);
            // printf("Child pid:%d exited with status %d\n", cpid, status >> 8);
            return 0;
    }
}
