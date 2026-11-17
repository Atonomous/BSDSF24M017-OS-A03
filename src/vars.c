#include "shell.h"

typedef struct var {
    char*       name;
    char*       value;
    struct var* next;
} var_t;

static var_t* head = NULL;
static var_t* tail = NULL;

/* Create the variable, or overwrite its value if it already exists. */
void var_set(const char* name, const char* value) {
    for (var_t* v = head; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            char* copy = strdup(value);     /* copy first, then free the old one */
            free(v->value);
            v->value = copy;
            return;
        }
    }
    var_t* v = malloc(sizeof(var_t));
    v->name  = strdup(name);
    v->value = strdup(value);
    v->next  = NULL;
    if (tail) tail->next = v; else head = v;   /* append, so 'set' lists in creation order */
    tail = v;
}

/* Returns the value, or NULL if the variable doesn't exist. */
const char* var_get(const char* name) {
    for (var_t* v = head; v != NULL; v = v->next)
        if (strcmp(v->name, name) == 0)
            return v->value;
    return NULL;
}

void var_print(void) {
    for (var_t* v = head; v != NULL; v = v->next)
        printf("%s=%s\n", v->name, v->value);
}

void var_free_all(void) {
    var_t* v = head;
    while (v != NULL) {
        var_t* next = v->next;              /* save it before freeing the node */
        free(v->name);
        free(v->value);
        free(v);
        v = next;
    }
    head = tail = NULL;
}

