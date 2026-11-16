#ifndef SHELL_H
#define SHELL_H
#define HISTORY_SIZE 256
#define MAX_CMDS 10

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "FCIT> "

// Function prototypes
char* read_cmd(char* prompt, FILE* fp);
char** tokenize(char* cmdline);
int execute(char** arglist);
int handle_builtin(char** arglist);
void hist_add(const char* cmd);
const char* hist_get(int n);
void hist_print(void);

typedef struct {
    char* argv[MAXARGS + 1];   /* NULL-terminated argument list */
    char* infile;              /* file after '<', or NULL */
    char* outfile;             /* file after '>', or NULL */
} command_t;

typedef struct {
    command_t cmds[MAX_CMDS];  /* one entry per stage of the pipeline */
    int ncmds;
} pipeline_t;

int  parse_pipeline(const char* line, pipeline_t* p);
void free_pipeline(pipeline_t* p);
int  execute_pipeline(pipeline_t* p, int background, const char* cmdtext);
void jobs_add(pid_t pid, const char* cmd);
void jobs_reap(void);
void jobs_print(void);

#endif // SHELL_H
