#include "shell.h"

#define MAX_JOBS 64

typedef struct {
    pid_t pid;
    int   id;
    char  cmd[MAX_LEN];
} job_t;

static job_t jobs[MAX_JOBS];
static int   njobs = 0;
static int   next_id = 1;

void jobs_add(pid_t pid, const char* cmd) {
    if (njobs == MAX_JOBS) {
        fprintf(stderr, "too many background jobs\n");
        return;
    }
    jobs[njobs].pid = pid;
    jobs[njobs].id  = next_id++;
    strncpy(jobs[njobs].cmd, cmd, MAX_LEN - 1);
    jobs[njobs].cmd[MAX_LEN - 1] = '\0';
    printf("[%d] %d\n", jobs[njobs].id, (int)pid);
    njobs++;
}

/* Collect every finished child without blocking. */
void jobs_reap(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < njobs; i++) {
            if (jobs[i].pid == pid) {
                printf("[%d]+ Done    %s\n", jobs[i].id, jobs[i].cmd);
                memmove(&jobs[i], &jobs[i + 1],
                        sizeof(job_t) * (njobs - i - 1));
                njobs--;
                break;
            }
        }
    }
}

void jobs_print(void) {
    if (njobs == 0) {
        printf("No background jobs.\n");
        return;
    }
    for (int i = 0; i < njobs; i++)
        printf("[%d]  Running  PID %d  %s\n",
               jobs[i].id, (int)jobs[i].pid, jobs[i].cmd);
}
