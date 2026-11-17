#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include <ctype.h>


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


static int has_text(const char* s) {
    for (; *s; s++)
        if (!isspace((unsigned char)*s)) return 1;
    return 0;
}

/* Look in shell variables first, then the environment ($HOME, $PATH...). */
static const char* lookup_var(const char* name) {
    const char* v = var_get(name);
    return v ? v : getenv(name);
}

/* If s looks like NAME=..., return the length of NAME, otherwise 0. */
static size_t assign_name_len(const char* s) {
    if (!(isalpha((unsigned char)*s) || *s == '_')) return 0;
    size_t i = 1;
    while (isalnum((unsigned char)s[i]) || s[i] == '_') i++;
    return (s[i] == '=') ? i : 0;
}

/* Replace every word of the form $NAME with its value (empty if unset). */
static void expand_variables(pipeline_t* p) {
    for (int i = 0; i < p->ncmds; i++) {
        for (int j = 0; p->cmds[i].argv[j] != NULL; j++) {
            char* w = p->cmds[i].argv[j];
            if (w[0] == '$' && w[1] != '\0') {
                const char* val = lookup_var(w + 1);
                char* repl = strdup(val ? val : "");   /* new memory for the result */
                free(w);                               /* free the old word */
                p->cmds[i].argv[j] = repl;
            }
        }
    }
}

/* Run one ';'-separated piece; returns its exit status. */
static int run_segment(char* seg) {
    int bg = 0, status = 0;

    while (isspace((unsigned char)*seg)) seg++;
    size_t len = strlen(seg);
    while (len > 0 && isspace((unsigned char)seg[len - 1]))
        seg[--len] = '\0';

    if (len > 0 && seg[len - 1] == '&') {
        bg = 1;
        seg[--len] = '\0';
        while (len > 0 && isspace((unsigned char)seg[len - 1]))
            seg[--len] = '\0';
    }

    pipeline_t pl;
    int rc = parse_pipeline(seg, &pl);
    if (rc > 0) {
        expand_variables(&pl);                   /* $NAME -> value, before anything runs */

        /* a single word like NAME=value is an assignment, not a command */
        size_t nl = (pl.ncmds == 1 && pl.cmds[0].argv[1] == NULL)
                        ? assign_name_len(pl.cmds[0].argv[0]) : 0;

        if (nl > 0) {
            const char* word  = pl.cmds[0].argv[0];
            const char* value = word + nl + 1;   /* text after the '=' */
            char name[MAX_LEN];
            memcpy(name, word, nl);
            name[nl] = '\0';

            if (value[0] == '$' && value[1] != '\0') {   /* B=$A */
                const char* v = lookup_var(value + 1);
                value = v ? v : "";
            }
            var_set(name, value);
        } else if (bg || !(pl.ncmds == 1 && handle_builtin(pl.cmds[0].argv))) {
            status = execute_pipeline(&pl, bg, seg);
        }
        free_pipeline(&pl);
    } else if (rc < 0) {
        status = 2;
    }
    return status;
}

/* Split a line on ';' and run each piece. Returns the LAST piece's status. */
static int run_line(const char* line) {
    int status = 0;
    char* copy = strdup(line);
    char* save = NULL;
    for (char* seg = strtok_r(copy, ";\n", &save); seg;
         seg = strtok_r(NULL, ";\n", &save))
        status = run_segment(seg);
    free(copy);
    return status;
}

/* If line begins with the whole word kw, return a pointer just past it. */
static const char* keyword(const char* line, const char* kw) {
    while (isspace((unsigned char)*line)) line++;
    size_t n = strlen(kw);
    if (strncmp(line, kw, n) == 0 &&
        (line[n] == '\0' || isspace((unsigned char)line[n])))
        return line + n;
    return NULL;
}

#define MAX_BLOCK 32
enum { COND, THEN, ELSE };

static int add_line(char** list, int* n, const char* s) {
    if (!has_text(s)) return 1;                  /* ignore blank text */
    if (*n == MAX_BLOCK) {
        fprintf(stderr, "if block too long\n");
        return 0;
    }
    list[(*n)++] = strdup(s);
    return 1;
}

/* Read the rest of an if-block from the keyboard, then run it. */
static void run_if(const char* first) {
    char* blk[3][MAX_BLOCK];
    int   cnt[3] = { 0, 0, 0 };
    int   state = COND, ok = 1, done = 0;

    /* text after the 'if' keyword on the first line is part of the condition */
    if (!add_line(blk[COND], &cnt[COND], keyword(first, "if")))
        ok = 0;

    while (ok && !done) {
        char* line = readline("> ");             /* continuation prompt */
        if (line == NULL) {                      /* Ctrl+D */
            fprintf(stderr, "syntax error: missing 'fi'\n");
            ok = 0;
            break;
        }
        const char* r;
        if ((r = keyword(line, "then")) != NULL) {
            if (state != COND || cnt[COND] == 0) {
                fprintf(stderr, "syntax error: unexpected 'then'\n");
                ok = 0;
            } else {
                state = THEN;
                if (!add_line(blk[THEN], &cnt[THEN], r)) ok = 0;
            }
        } else if ((r = keyword(line, "else")) != NULL) {
            if (state != THEN) {
                fprintf(stderr, "syntax error: unexpected 'else'\n");
                ok = 0;
            } else {
                state = ELSE;
                if (!add_line(blk[ELSE], &cnt[ELSE], r)) ok = 0;
            }
        } else if (keyword(line, "fi") != NULL) {
            if (state == COND) {
                fprintf(stderr, "syntax error: 'fi' before 'then'\n");
                ok = 0;
            } else {
                done = 1;
            }
        } else {
            if (!add_line(blk[state], &cnt[state], line)) ok = 0;
        }
        free(line);
    }

    if (ok && done) {
        int status = 0;
        for (int i = 0; i < cnt[COND]; i++)      /* run the condition */
            status = run_line(blk[COND][i]);

        int which = (status == 0) ? THEN : ELSE; /* 0 = success = then */
        for (int i = 0; i < cnt[which]; i++)
            run_line(blk[which][i]);
    }

    for (int s = 0; s < 3; s++)
        for (int i = 0; i < cnt[s]; i++)
            free(blk[s][i]);
}

int main() {
    char* cmdline;
    rl_attempted_completion_function = shell_completion;

    while (1) {
        jobs_reap();                          /* collect finished bg jobs BEFORE the prompt */
        cmdline = readline(PROMPT);
        if (cmdline == NULL) break;           /* Ctrl+D */

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
            printf("%s\n", cmdline);
        }

        if (has_text(cmdline)) {              /* store the WHOLE line once */
            hist_add(cmdline);
            add_history(cmdline);
        }

        if (keyword(cmdline, "if") != NULL)
            run_if(cmdline);                     /* multi-line if/then/else/fi */
        else
            run_line(cmdline);

        free(cmdline);
    }

    var_free_all();
    printf("\nShell exited.\n");
    return 0;
}   

