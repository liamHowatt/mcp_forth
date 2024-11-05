#include "mcp_forth.h"
#include <stdlib.h>

static int rt_bye(void * param, m4_stack_t * stack)
{
    exit(0);
    return 0;
}

const m4_runtime_cb_array_t m4_runtime_lib_process[] = {
    {"bye", {rt_bye}},
    {NULL},
};
