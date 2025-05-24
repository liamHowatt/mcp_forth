#define _GNU_SOURCE
#include "mcp_forth.h"
#include <string.h>

static int rt_search(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 4)) return M4_STACK_UNDERFLOW_ERROR;
    void * haystack = (void *) stack->data[-4];
    int haystack_len = stack->data[-3];
    void * result = memmem(haystack, haystack_len, (void *) stack->data[-2], stack->data[-1]);
    if(result) {
        stack->data[-4] = (int) result;
        stack->data[-3] = m4_bytes_remaining(haystack, result, haystack_len);
        stack->data[-2] = -1;
    }
    else {
        stack->data[-2] = 0;
    }
    stack->data -= 1;
    stack->len -= 1;
    return 0;
}

static int rt_compare(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 4)) return M4_STACK_UNDERFLOW_ERROR;
    void *   a1 = (void *) stack->data[-4];
    unsigned u1 =          stack->data[-3];
    void *   a2 = (void *) stack->data[-2];
    unsigned u2 =          stack->data[-1];
    int result = memcmp(a1, a2, u1 < u2 ? u1 : u2);
    if(result == 0) {
        stack->data[-4] = u1 < u2 ? -1 : u1 > u2 ? 1 : 0;
    } else {
        stack->data[-4] = result < 0 ? -1 : 1;
    }
    stack->data -= 3;
    stack->len -= 3;
    return 0;
}

static int rt_move(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 3)) return M4_STACK_UNDERFLOW_ERROR;
    memmove(
        (void *) stack->data[-2],
        (void *) stack->data[-3],
        stack->data[-1]
    );
    stack->data -= 3;
    stack->len -= 3;
    return 0;
}

static int rt_fill(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 3)) return M4_STACK_UNDERFLOW_ERROR;
    memset(
        (void *) stack->data[-3],
        stack->data[-1],
        stack->data[-2]
    );
    stack->data -= 3;
    stack->len -= 3;
    return 0;
}

const m4_runtime_cb_array_t m4_runtime_lib_string[] = {
    {"search", {rt_search}},
    {"compare", {rt_compare}},
    {"move", {rt_move}},
    {"fill", {rt_fill}},
    {NULL},
};
