#include "mcp_forth.h"
#include <assert.h>
#include <stdio.h>

static int rt_assert(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 1)) return M4_STACK_UNDERFLOW_ERROR;
    assert(stack->data[-1]);
    stack->data -= 1;
    stack->len -= 1;
    return 0;
}

static int rt_assert_msg(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 3)) return M4_STACK_UNDERFLOW_ERROR;
    if(!stack->data[-3]) {
        fwrite((void *)stack->data[-2], 1, stack->data[-1], stderr);
        putc('\n', stderr);
        fflush(stderr);
        assert(0);
    }
    stack->data -= 3;
    stack->len -= 3;
    return 0;
}

static int rt_panic(void * param, m4_stack_t * stack)
{
    assert(0);
    return 0;
}

static int rt_panic_msg(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 2)) return M4_STACK_UNDERFLOW_ERROR;
    fwrite((void *)stack->data[-2], 1, stack->data[-1], stderr);
    putc('\n', stderr);
    fflush(stderr);
    assert(0);
    stack->data -= 2;
    stack->len -= 2;
    return 0;
}

const m4_runtime_cb_array_t m4_runtime_lib_assert[] = {
    {"assert", {rt_assert}},
    {"assert_msg", {rt_assert_msg}},
    {"panic", {rt_panic}},
    {"panic_msg", {rt_panic_msg}},
    {NULL},
};
