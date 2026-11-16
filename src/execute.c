#include "shell.h"
#include <fcntl.h>

/* Original single-command executor from the starter code (kept for reference). */
int execute(char* arglist[]) {
    int status;
    int cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0:
            execvp(arglist[0], arglist);
            perror("Command not found");
            exit(1);
        default:
            waitpid(cpid, &status, 0);
            return 0;
    }
}

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

int execute_pipeline(pipeline_t* p, int background, const char* cmdtext) {
    int n = p->ncmds;
    int prev_read = -1;          /* read end of the previous pipe */
    pid_t pids[MAX_CMDS];
    int forked = 0;

    for (int i = 0; i < n; i++) {
        int fd[2] = { -1, -1 };

        if (i < n - 1 && pipe(fd) < 0) {
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
            if (prev_read != -1) {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (background && prev_read == -1 && !p->cmds[i].infile) {
                int dn = open("/dev/null", O_RDONLY);
                dup2(dn, STDIN_FILENO);
                close(dn);
            }
            if (fd[1] != -1) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }
            setup_redirects(&p->cmds[i]);
            execvp(p->cmds[i].argv[0], p->cmds[i].argv);
            perror(p->cmds[i].argv[0]);
            exit(127);
        }

        /* ---- parent ---- */
        pids[forked++] = pid;
        if (prev_read != -1) close(prev_read);
        if (fd[1] != -1) close(fd[1]);
        prev_read = fd[0];
    }

    if (prev_read != -1) close(prev_read);

    if (background) {                            /* do NOT wait: remember the job */
        if (forked > 0) jobs_add(pids[forked - 1], cmdtext);
        return 0;
    }

    int last_status = 1;                         /* nothing forked = failure */
    for (int i = 0; i < forked; i++) {
        int st;
        waitpid(pids[i], &st, 0);
        if (i == forked - 1) {                   /* a pipeline's status is its LAST command's */
            if (WIFEXITED(st))
                last_status = WEXITSTATUS(st);
            else if (WIFSIGNALED(st))
                last_status = 128 + WTERMSIG(st);   /* killed by a signal */
        }
    }
    return last_status;
}
