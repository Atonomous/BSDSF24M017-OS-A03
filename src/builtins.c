#include "shell.h"

/* Returns 1 if arglist[0] was a built-in (and has been handled),
   0 if it is not a built-in (caller should fork/exec it). */
int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL)
        return 0;

    /* ---- exit ---- */
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Shell exited.\n");
        exit(0);
    }

    /* ---- cd ---- */
    if (strcmp(arglist[0], "cd") == 0) {
        const char* target = arglist[1];
        if (target == NULL) {
            target = getenv("HOME");
            if (target == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return 1;
            }
        }
        if (chdir(target) != 0) {
            perror("cd");
        }
        return 1;
    }

    /* ---- help ---- */
    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit         Exit the shell\n");
        printf("  cd <dir>     Change the current directory\n");
        printf("  help         Show this help message\n");
        printf("  jobs         List background jobs (not yet implemented)\n");
        return 1;
    }

    /* ---- jobs ---- */
    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    return 0;   /* not a built-in */
}
