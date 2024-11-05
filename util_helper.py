for i in range(1, 11):
    print(f"""int m4_f0{i}(void * param, m4_stack_t * stack)
{{
    if(!(stack->len >= {i})) return M4_STACK_UNDERFLOW_ERROR;
    void (*func)({', '.join('int' for _ in range(i))}) = param;
    stack->data -= {i};
    stack->len -= {i};
    func({', '.join(f'stack->data[{j}]' for j in range(i))});
    return 0;
}}
""")

print()

for i in range(1, 11):
    print(f"""int m4_f1{i}(void * param, m4_stack_t * stack)
{{
    if(!(stack->len >= {i})) return M4_STACK_UNDERFLOW_ERROR;
    int (*func)({', '.join('int' for _ in range(i))}) = param;
    stack->data -= {i};
    stack->len -= {i};
    int y = func({', '.join(f'stack->data[{j}]' for j in range(i))});
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = y;
    stack->data += 1;
    stack->len += 1;
    return 0;
}}
""")
