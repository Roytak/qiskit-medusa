#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "error.h"
#include "circuit_ir.h"

/// Beginning of an error message.
#define ERROR_TEXT "ERROR: "

void error_exit(const char *error, ...)
{
    va_list args;
    va_start(args, error);

    // create error message format
    unsigned error_len = strlen(error) + strlen(ERROR_TEXT);
    char msg[error_len + 1];
    strcpy(msg, ERROR_TEXT);
    strcat(msg, error);

    vfprintf(stderr, msg, args);
    va_end(args);

    exit(1);
}

void error_set(circuit_ir_t *ir, int line, const char *error, ...)
{
    va_list args;
    va_start(args, error);

    ir->errs = my_realloc(ir->errs, (ir->err_count + 1) * sizeof(medusa_err_info_t));
    ir->errs[ir->err_count].line = line;
    vsnprintf(ir->errs[ir->err_count].msg, sizeof(ir->errs[ir->err_count].msg), error, args);
    ir->err_count++;
}

void error_get_last(circuit_ir_t *ir, const char **msg, int *line)
{
    if (ir->err_count > 0) {
        *msg  = ir->errs[ir->err_count - 1].msg;
        *line = ir->errs[ir->err_count - 1].line;
    } else {
        *msg  = "";
        *line = -1;
    }
}

void error_clear(circuit_ir_t *ir)
{
    free(ir->errs);
    ir->errs = NULL;
    ir->err_count = 0;
}

void* my_malloc(size_t size) {
    void *p = malloc(size);
    if (p == NULL) {
        error_exit("Bad memory allocation.\n");
    }
    return p;
}

void* my_realloc(void *p, size_t size) {
    void *p_new = realloc(p, size);
    if (p_new == NULL) {
        error_exit("Memory reallocation failed.\n");
    }
    return p_new;
}

/* end of "error.c" */