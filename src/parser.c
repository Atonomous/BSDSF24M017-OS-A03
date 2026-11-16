#include "shell.h"

#define DELIMS " \t<>|"

void free_pipeline(pipeline_t* p) {
    for (int i = 0; i < p->ncmds; i++) {
        for (int j = 0; p->cmds[i].argv[j] != NULL; j++)
            free(p->cmds[i].argv[j]);
        free(p->cmds[i].infile);
        free(p->cmds[i].outfile);
    }
}

/* Returns  1 = parsed OK,  0 = empty line,  -1 = syntax error. */
int parse_pipeline(const char* line, pipeline_t* p) {
    memset(p, 0, sizeof(*p));           /* all pointers start as NULL */
    p->ncmds = 1;
    command_t* cur = &p->cmds[0];
    int argc = 0;
    const char* cp = line;

    while (*cp) {
        if (*cp == ' ' || *cp == '\t') { cp++; continue; }

        if (*cp == '|') {
            if (cur->argv[0] == NULL) {
                fprintf(stderr, "syntax error near '|'\n");
                goto fail;
            }
            if (p->ncmds == MAX_CMDS) {
                fprintf(stderr, "too many commands in pipeline\n");
                goto fail;
            }
            cur = &p->cmds[p->ncmds++];     /* start the next stage */
            argc = 0;
            cp++;
            continue;
        }

        if (*cp == '<' || *cp == '>') {
            char op = *cp++;
            while (*cp == ' ' || *cp == '\t') cp++;
            const char* s = cp;
            while (*cp && strchr(DELIMS, *cp) == NULL) cp++;
            if (cp == s) {
                fprintf(stderr, "syntax error: missing filename after '%c'\n", op);
                goto fail;
            }
            char** slot = (op == '<') ? &cur->infile : &cur->outfile;
            free(*slot);                    /* a later redirect replaces an earlier one */
            *slot = strndup(s, cp - s);
            continue;
        }

                /* ordinary word: quotes group characters and are removed */
        char buf[MAX_LEN];
        size_t bl = 0;
        char q = 0;                              /* current quote char, or 0 */
        while (*cp) {
            if (q) {
                if (*cp == q) { q = 0; cp++; continue; }   /* closing quote */
            } else if (*cp == '"' || *cp == '\'') {
                q = *cp; cp++; continue;                   /* opening quote */
            } else if (strchr(DELIMS, *cp) != NULL) {
                break;                                     /* end of word */
            }
            if (bl < sizeof(buf) - 1) buf[bl++] = *cp;
            cp++;
        }
        if (q) {
            fprintf(stderr, "syntax error: unterminated quote\n");
            goto fail;
        }
        buf[bl] = '\0';
        if (argc >= MAXARGS) {
            fprintf(stderr, "too many arguments\n");
            goto fail;
        }
        cur->argv[argc++] = strdup(buf);
    }

    /* every stage needs a command name */
    for (int i = 0; i < p->ncmds; i++) {
        if (p->cmds[i].argv[0] == NULL) {
            if (p->ncmds == 1 && !p->cmds[0].infile && !p->cmds[0].outfile)
                return 0;                   /* blank line */
            fprintf(stderr, "syntax error: missing command\n");
            goto fail;
        }
    }
    return 1;

fail:
    free_pipeline(p);
    return -1;
}
