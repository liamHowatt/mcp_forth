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

const m4_runtime_cb_array_t m4_runtime_lib_string[] = {
    {"search", {rt_search}},
    {NULL},
};
