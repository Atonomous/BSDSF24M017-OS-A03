#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {

        /* !n : replace the line with the n-th history command */
        if (cmdline[0] == '!') {
            char* end;
            long n = strtol(cmdline + 1, &end, 10);
            const char* found = NULL;

            if (end != cmdline + 1 && *end == '\0')
                found = hist_get((int)n);

            if (found == NULL) {
                fprintf(stderr, "%s: event not found\n", cmdline);
                free(cmdline);
                continue;
            }
            free(cmdline);
            cmdline = strdup(found);
            printf("%s\n", cmdline);      /* show what is being re-run */
        }

        if ((arglist = tokenize(cmdline)) != NULL) {
            hist_add(cmdline);            /* store only real commands */

            if (!handle_builtin(arglist)) {
                execute(arglist);
            }

            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
