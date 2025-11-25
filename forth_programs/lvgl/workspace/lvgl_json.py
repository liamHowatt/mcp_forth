import json
import sys
import re

def main():
    assert len(sys.argv) == 2, "expected a json file arg"

    with open(sys.argv[1]) as f:
        lvgl = json.load(f)

    print(start)

    for enum in lvgl["enums"]:
        for member in enum["members"]:
            name = member["name"]

            print(f'    {{"{name.lower()}", {{m4_lit, (void *) {name}}}}},')

    for function in lvgl["functions"]:
        name = function["name"]

        return_type_name = function["type"]["type"].get("name")
        if return_type_name == "void": return_type_code = 0
        elif return_type_name == "lv_color_t": return_type_code = "c"
        else: return_type_code = 1

        args = function["args"]
        if args[0]["type"].get("name") == "void": arg_count_code = 0
        elif args[-1]["type"].get("name") == "ellipsis": arg_count_code = "x"
        else: arg_count_code = len(args)

        print(f'    {{"{name.lower()}", {{m4_f{return_type_code}{arg_count_code}, {name}}}}},')

    for structure in lvgl["structures"]:
        name = structure["name"]
        trimmed_name = name.lstrip("_")

        print(f'    {{"{trimmed_name.lower()}", {{m4_lit, (void *) sizeof({trimmed_name})}}}},')

    initializer_re = re.compile(r"\(?([0-9]|LV_)")

    for macro in lvgl["macros"]:
        name = macro["name"]
        params = macro["params"]
        initializer = macro["initializer"]
        if (
            name.startswith("LV_")
            and params is None
            and initializer is not None
            and initializer_re.match(initializer) is not None
            and "DEFINE" not in initializer
            and "DECLARE" not in initializer
        ):
            print(f'    {{"{name.lower()}", {{m4_lit, (void *) ({initializer})}}}},')

    print(end)


start = """#include <mcp/mcp_forth.h>
#include "lvgl/lvgl.h"
#include "runtime_lvgl.h"

static int m4_fc0(void * param, m4_stack_t * stack)
{
    lv_color_t (*func)(void) = param;
    lv_color_t col = func();
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    int i_col = 0;
    memcpy(&i_col, &col, sizeof(col));
    stack->data[0] = i_col;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

static int m4_fc1(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 1)) return M4_STACK_UNDERFLOW_ERROR;
    lv_color_t (*func)(int) = param;
    stack->data -= 1;
    stack->len -= 1;
    lv_color_t col = func(stack->data[0]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    int i_col = 0;
    memcpy(&i_col, &col, sizeof(col));
    stack->data[0] = i_col;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

static int m4_fc2(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 2)) return M4_STACK_UNDERFLOW_ERROR;
    lv_color_t (*func)(int, int) = param;
    stack->data -= 2;
    stack->len -= 2;
    lv_color_t col = func(stack->data[0], stack->data[1]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    int i_col = 0;
    memcpy(&i_col, &col, sizeof(col));
    stack->data[0] = i_col;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

static int m4_fc3(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 3)) return M4_STACK_UNDERFLOW_ERROR;
    lv_color_t (*func)(int, int, int) = param;
    stack->data -= 3;
    stack->len -= 3;
    lv_color_t col = func(stack->data[0], stack->data[1], stack->data[2]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    int i_col = 0;
    memcpy(&i_col, &col, sizeof(col));
    stack->data[0] = i_col;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

const m4_runtime_cb_array_t runtime_lib_lvgl[] = {"""

end = """    {NULL}
};"""

if __name__ == "__main__":
    main()
