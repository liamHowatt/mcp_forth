#include "mcp_forth.h"
#include <time.h>

static int rt_ms(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 1)) return M4_STACK_UNDERFLOW_ERROR;
    stack->data -= 1;
    stack->len -= 1;
    unsigned long long ms = *stack->data;
    unsigned long long sec = ms / 1000ull;
    unsigned long long ms_no_sec = ms % 1000ull;
    unsigned long long ns_no_sec = ms_no_sec * 1000000ull;
    struct timespec tspec = {.tv_sec=sec, .tv_nsec=ns_no_sec};
    int res = nanosleep(&tspec, NULL);
    assert(res == 0);
    return 0;
}

const m4_runtime_cb_array_t m4_runtime_lib_time[] = {
    {"ms", {rt_ms}},
    {NULL},
};
