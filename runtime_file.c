#include "mcp_forth.h"
#include <stdio.h>
#include <stdlib.h>

static int rt_open_file(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 3)) return M4_STACK_UNDERFLOW_ERROR;
    const char * path = (const char *) stack->data[-3];
    int path_len = stack->data[-2];
    int mode = stack->data[-1];
    assert(mode == 0);
    char * terminated_path = malloc(path_len + 1);
    memcpy(terminated_path, path, path_len);
    terminated_path[path_len] = '\0';
    FILE * f = fopen(terminated_path, "r");
    free(terminated_path);
    stack->data[-3] = (int) f;
    stack->data[-2] = f != NULL ? 0 : 1;
    stack->data -= 1;
    stack->len -= 1;
    return 0;
}

static int rt_read_file(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 3)) return M4_STACK_UNDERFLOW_ERROR;
    void * addr = (void *) stack->data[-3];
    int r_len = stack->data[-2];
    FILE * f = (FILE *) stack->data[-1];
    int br = fread(addr, 1, r_len, f);
    stack->data[-3] = br;
    stack->data[-2] = 0;
    stack->data -= 1;
    stack->len -= 1;
    return 0;
}

static int rt_close_file(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 1)) return M4_STACK_UNDERFLOW_ERROR;
    FILE * f = (FILE *) stack->data[-1];
    int res = fclose(f);
    stack->data[-1] = res;
    return 0;
}

const m4_runtime_cb_array_t m4_runtime_lib_file[] = {
    {"open-file", {rt_open_file}},
    {"read-file", {rt_read_file}},
    {"close-file", {rt_close_file}},
    {"r/o", {m4_lit, (void *) 0}},
    {NULL},
};
