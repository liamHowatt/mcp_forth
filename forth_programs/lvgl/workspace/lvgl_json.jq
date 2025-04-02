"#include <mcp/mcp_forth.h>",
"#include \"lvgl/lvgl.h\"",
"#include \"runtime_lvgl.h\"",
"",
"static int m4_fc0(void * param, m4_stack_t * stack)",
"{",
"    lv_color_t (*func)(void) = param;",
"    lv_color_t col = func();",
"    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;",
"    int i_col = 0;",
"    memcpy(&i_col, &col, sizeof(col));",
"    stack->data[0] = i_col;",
"    stack->data += 1;",
"    stack->len += 1;",
"    return 0;",
"}",
"",
"static int m4_fc1(void * param, m4_stack_t * stack)",
"{",
"    if(!(stack->len >= 1)) return M4_STACK_UNDERFLOW_ERROR;",
"    lv_color_t (*func)(int) = param;",
"    stack->data -= 1;",
"    stack->len -= 1;",
"    lv_color_t col = func(stack->data[0]);",
"    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;",
"    int i_col = 0;",
"    memcpy(&i_col, &col, sizeof(col));",
"    stack->data[0] = i_col;",
"    stack->data += 1;",
"    stack->len += 1;",
"    return 0;",
"}",
"",
"static int m4_fc2(void * param, m4_stack_t * stack)",
"{",
"    if(!(stack->len >= 2)) return M4_STACK_UNDERFLOW_ERROR;",
"    lv_color_t (*func)(int, int) = param;",
"    stack->data -= 2;",
"    stack->len -= 2;",
"    lv_color_t col = func(stack->data[0], stack->data[1]);",
"    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;",
"    int i_col = 0;",
"    memcpy(&i_col, &col, sizeof(col));",
"    stack->data[0] = i_col;",
"    stack->data += 1;",
"    stack->len += 1;",
"    return 0;",
"}",
"",
"static int m4_fc3(void * param, m4_stack_t * stack)",
"{",
"    if(!(stack->len >= 3)) return M4_STACK_UNDERFLOW_ERROR;",
"    lv_color_t (*func)(int, int, int) = param;",
"    stack->data -= 3;",
"    stack->len -= 3;",
"    lv_color_t col = func(stack->data[0], stack->data[1], stack->data[2]);",
"    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;",
"    int i_col = 0;",
"    memcpy(&i_col, &col, sizeof(col));",
"    stack->data[0] = i_col;",
"    stack->data += 1;",
"    stack->len += 1;",
"    return 0;",
"}",
"",
"const m4_runtime_cb_array_t runtime_lib_lvgl[] = {",

(.enums[].members[].name |
    "    {\"\(. | ascii_downcase)\", {m4_lit, (void *) \(.)}},"),

(.functions[] |
    "    {\"\(.name | ascii_downcase)\", {m4_f\(.type.type.name |
            if . == "void" then 0
            elif . == "lv_color_t" then "c"
            else 1 end
        )\(.args |
            if .[0].type.name == "void" then 0
            elif .[-1].type.name == "ellipsis" then "x"
            else length end
    ), \(.name)}},"),

(.structures[].name | ltrimstr("_") |
    "    {\"\(. | ascii_downcase)\", {m4_lit, (void *) sizeof(\(.))}},"),

(.macros[] | select((.name | startswith("LV_"))
                    and .params == null
                    and .initializer != null
                    and (.initializer | test("^\\(?([0-9]|LV_)"))
                    and (.initializer | contains("DEFINE") | not)
                    and (.initializer | contains("DECLARE") | not)
                   ) |
    "    {\"\(.name | ascii_downcase)\", {m4_lit, (void *) (\(.initializer))}},"),

"    {NULL}",
"};"
