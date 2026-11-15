#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>



static const char* builtin_names[] =
    { "exit", "cd", "help", "jobs", "history", NULL };

static char** matches = NULL;
static int nmatch = 0, midx = 0;

static void add_match(const char* name) {
    for (int i = 0; i < nmatch; i++)
        if (strcmp(matches[i], name) == 0) return;      /* no duplicates */
    matches = realloc(matches, sizeof(char*) * (nmatch + 1));
    matches[nmatch++] = strdup(name);
}

static char* command_generator(const char* text, int state) {
    if (state == 0) {                                   /* first call: build list */
        for (int i = 0; i < nmatch; i++) free(matches[i]);
        free(matches);
        matches = NULL; nmatch = 0; midx = 0;

        size_t len = strlen(text);
        for (int i = 0; builtin_names[i]; i++)
            if (strncmp(builtin_names[i], text, len) == 0)
                add_match(builtin_names[i]);

        char* path = getenv("PATH");
        if (path) {
            char* copy = strdup(path);
            for (char* dir = strtok(copy, ":"); dir; dir = strtok(NULL, ":")) {
                DIR* d = opendir(dir);
                if (!d) continue;
                struct dirent* e;
                while ((e = readdir(d)) != NULL) {
                    if (strncmp(e->d_name, text, len) != 0) continue;
                    char full[1024];
                    snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
                    if (access(full, X_OK) == 0) add_match(e->d_name);
                }
                closedir(d);
            }
            free(copy);
        }
    }
    if (midx < nmatch) return strdup(matches[midx++]);  /* one match per call */
    return NULL;                                        /* NULL = no more */
}

static char** shell_completion(const char* text, int start, int end) {
    (void)end;
    if (start == 0 && strchr(text, '/') == NULL)        /* first word, not a path */
        return rl_completion_matches(text, command_generator);
    return NULL;                                        /* fall back to filenames */
}


int main() {
        rl_attempted_completion_function = shell_completion;
    char* cmdline;
    char** arglist;

    while ((cmdline = readline(PROMPT)) != NULL) {

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
	    add_history(cmdline);

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
