#include "mcp_forth.h"
#include <assert.h>

static int rt_assert(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 1)) return M4_STACK_UNDERFLOW_ERROR;
    assert(stack->data[-1]);
    stack->data -= 1;
    stack->len -= 1;
    return 0;
}

const m4_runtime_cb_array_t m4_runtime_lib_assert[] = {
    {"assert", {rt_assert}},
    {NULL},
};
