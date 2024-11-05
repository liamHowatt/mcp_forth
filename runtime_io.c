#include "mcp_forth.h"
#include <stdio.h>

static int rt_type(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 2)) return M4_STACK_UNDERFLOW_ERROR;
    fwrite((void *)stack->data[-2], 1, stack->data[-1], stdout);
    fflush(stdout);
    stack->data -= 2;
    stack->len -= 2;
    return 0;
}

static int rt_cr(void * param, m4_stack_t * stack)
{
    puts("");
    return 0;
}

static int rt_dot(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 1)) return M4_STACK_UNDERFLOW_ERROR;
    printf("%d ", stack->data[-1]);
    fflush(stdout);
    stack->data -= 1;
    stack->len -= 1;
    return 0;
}

static int rt_key(void * param, m4_stack_t * stack)
{
    if(!(stack->len < stack->max)) return M4_STACK_UNDERFLOW_ERROR;
    int c = getchar();
    if(c == EOF) c = 4; /* ASCII EOT (end of transmission) */
    *stack->data = c;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

static int rt_emit(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 1)) return M4_STACK_UNDERFLOW_ERROR;
    putchar(stack->data[-1]);
    fflush(stdout);
    stack->data -= 1;
    stack->len -= 1;
    return 0;
}

const m4_runtime_cb_array_t m4_runtime_lib_io[] = {
    {"type", {rt_type}},
    {"cr", {rt_cr}},
    {".", {rt_dot}},
    {"key", {rt_key}},
    {"emit", {rt_emit}},
    {NULL},
};
