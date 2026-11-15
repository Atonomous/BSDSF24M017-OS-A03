#include "shell.h"

static char* hist_buf[HISTORY_SIZE];  /* circular buffer, all NULL at start */
static int   hist_count = 0;          /* total commands ever stored */

/* Store a copy of cmd. When the buffer is full, overwrite the oldest. */
void hist_add(const char* cmd) {
    int slot = hist_count % HISTORY_SIZE;
    free(hist_buf[slot]);             /* free the old entry (free(NULL) is safe) */
    hist_buf[slot] = strdup(cmd);
    hist_count++;
}

/* Return command number n (1-based), or NULL if it doesn't exist
   (never stored, or already overwritten). */
const char* hist_get(int n) {
    if (n < 1 || n > hist_count) return NULL;
    if (n <= hist_count - HISTORY_SIZE) return NULL;   /* overwritten */
    return hist_buf[(n - 1) % HISTORY_SIZE];
}

/* Print the stored commands, oldest first, with their numbers. */
void hist_print(void) {
    int start = hist_count - HISTORY_SIZE + 1;
    if (start < 1) start = 1;
    for (int n = start; n <= hist_count; n++) {
        printf("%4d  %s\n", n, hist_get(n));
    }
}
