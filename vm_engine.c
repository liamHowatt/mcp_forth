#include "mcp_forth.h"

#define STKSZ 1000
#define RETURN_STKSZ 1000
#define MEMSZ 4000

#define RASSERT(expr, ecode) do { if(!(expr)) { ret=(ecode); goto end_label; } } while(0)

#define POP(var)  if((ret = pop(&var, &stack))) goto end_label
#define PUSH(var) if((ret = push(var, &stack))) goto end_label
#define RETURN_POP(var)  if((ret = pop((int *) &var, &return_stack))) goto end_label
#define RETURN_PUSH(var) if((ret = push((int) var, &return_stack))) goto end_label

union iu {int i; unsigned u;};

static int pc_step(const uint8_t ** pc_p) {
    int ret = num_decode(*pc_p);
    *pc_p += num_encoded_size_from_encoded(*pc_p);
    return ret;
}

static int pop(int * dst, stack_t * stack) {
    if(!stack->len) return STACK_UNDERFLOW_ERROR;
    stack->len -= 1;
    stack->data -= 1;
    *dst = *stack->data;
    return 0;
}

static int push(int src, stack_t * stack) {
    if(stack->len == stack->max) return STACK_OVERFLOW_ERROR;
    *stack->data = src;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

static int run(
    const uint8_t * program_start,
    const uint8_t * data_start,
    runtime_cb * runtime_cbs,
    void * runtime_ctx,
    int variable_count
) {
    int ret = 0;

    int * stack_orig = malloc(sizeof(int) * STKSZ);
    assert(stack_orig);
    stack_t stack = {.data=stack_orig, .max=STKSZ, .len=0};

    int * return_stack_orig = malloc(sizeof(int) * RETURN_STKSZ);
    assert(return_stack_orig);
    stack_t return_stack = {.data=return_stack_orig, .max=RETURN_STKSZ, .len=0};
    int return_stack_len_base = 0;

    uint8_t * memory_orig = calloc(MEMSZ, 1);
    assert(memory_orig);
    uint8_t * memory = memory_orig;

    uint8_t ** variables = malloc(variable_count * sizeof(uint8_t *));
    assert(variables);

    const uint8_t * pc = program_start;
    while(1) {
        const uint8_t * pc_initial = pc;
        int op = pc_step(&pc);

        int builtin_num = -1;
        if(op >= OPCODE_LAST_ && op <= NUM_MAX_ONE_BYTE) {
            builtin_num = op - OPCODE_LAST_;
            op = OPCODE_BUILTIN_WORD;
        }

        switch(op) {
            case OPCODE_BUILTIN_WORD:
                if(builtin_num == -1) {
                    builtin_num = pc_step(&pc);
                }
                switch (builtin_num) {
                    case 0: { /* dup */
                        int x;
                        POP(x);
                        PUSH(x);
                        PUSH(x);
                        break;
                    }
                    case 1: { /* - */
                        int l, r;
                        POP(r);
                        POP(l);
                        PUSH(l - r);
                        break;
                    }
                    case 2: { /* > */
                        int l, r;
                        POP(r);
                        POP(l);
                        PUSH(l > r ? -1 : 0);
                        break;
                    }
                    case 3: { /* + */
                        int l, r;
                        POP(r);
                        POP(l);
                        PUSH(l + r);
                        break;
                    }
                    case 4: { /* = */
                        int l, r;
                        POP(r);
                        POP(l);
                        PUSH(l == r ? -1 : 0);
                        break;
                    }
                    case 5: { /* over */
                        int b, a;
                        POP(a);
                        POP(b);
                        PUSH(b);
                        PUSH(a);
                        PUSH(b);
                        break;
                    }
                    case 6: { /* mod */
                        int l, r;
                        POP(r);
                        POP(l);
                        PUSH(l % r);
                        break;
                    }
                    case 7: { /* drop */
                        int x;
                        POP(x);
                        break;
                    }
                    case 8: { /* swap */
                        int b, a;
                        POP(a);
                        POP(b);
                        PUSH(a);
                        PUSH(b);
                        break;
                    }
                    case 9: { /* * */
                        int l, r;
                        POP(r);
                        POP(l);
                        PUSH(l * r);
                        break;
                    }
                    case 10: { /* allot */
                        int x;
                        POP(x);
                        memory += x;
                        if(memory - memory_orig > MEMSZ) {
                            ret = OUT_OF_MEMORY_ERROR;
                            goto end_label;
                        }
                        break;
                    }
                    case 11: { /* 1+ */
                        int x;
                        POP(x);
                        PUSH(x + 1);
                        break;
                    }
                    case 12: { /* invert */
                        int x;
                        POP(x);
                        PUSH(~x);
                        break;
                    }
                    case 13: { /* 0= */
                        int x;
                        POP(x);
                        PUSH(x == 0 ? -1 : 0);
                        break;
                    }
                    case 14: { /* i */
                        PUSH(return_stack.data[-1]);
                        break;
                    }
                    case 15: { /* j */
                        PUSH(return_stack.data[-3]);
                        break;
                    }
                    case 16: { /* 1- */
                        int x;
                        POP(x);
                        PUSH(x - 1);
                        break;
                    }
                    case 17: { /* rot */
                        int x1, x2, x3;
                        POP(x3);
                        POP(x2);
                        POP(x1);
                        PUSH(x2);
                        PUSH(x3);
                        PUSH(x1);
                        break;
                    }
                    case 18: { /* pick */
                        int n;
                        POP(n);
                        if(stack.len <= n) {
                            ret = STACK_UNDERFLOW_ERROR;
                            goto end_label;
                        }
                        PUSH(stack.data[-n - 1]);
                        break;
                    }
                    case 19: { /* xor */
                        int l, r;
                        POP(r);
                        POP(l);
                        PUSH(l ^ r);
                        break;
                    }
                    case 20: { /* lshift */
                        union iu l, r;
                        POP(r.i);
                        POP(l.i);
                        PUSH(l.u << r.u);
                        break;
                    }
                    case 21: { /* rshift */
                        union iu l, r;
                        POP(r.i);
                        POP(l.i);
                        PUSH(l.u >> r.u);
                        break;
                    }
                    case 22: { /* < */
                        int l, r;
                        POP(r);
                        POP(l);
                        PUSH(l < r ? -1 : 0);
                        break;
                    }
                    case 23: { /* >= */
                        int l, r;
                        POP(r);
                        POP(l);
                        PUSH(l >= r ? -1 : 0);
                        break;
                    }
                    case 24: { /* <= */
                        int l, r;
                        POP(r);
                        POP(l);
                        PUSH(l <= r ? -1 : 0);
                        break;
                    }
                    case 25: { /* or */
                        union iu l, r;
                        POP(r.i);
                        POP(l.i);
                        PUSH(l.u | r.u);
                        break;
                    }
                    case 26: { /* and */
                        union iu l, r;
                        POP(r.i);
                        POP(l.i);
                        PUSH(l.u & r.u);
                        break;
                    }
                    case 27: { /* depth */
                        PUSH(stack.len);
                        break;
                    }
                    case 28: { /* c! */
                        int c, addr;
                        POP(addr);
                        POP(c);
                        *(uint8_t *)addr = c;
                        break;
                    }
                    case 29: { /* c@ */
                        int addr;
                        POP(addr);
                        PUSH(*(uint8_t *)addr);
                        break;
                    }
                    case 30: { /* 0< */
                        int x;
                        POP(x);
                        PUSH(x < 0 ? -1 : 0);
                        break;
                    }
                    case 31: { /* ! */
                        int x, addr;
                        POP(addr);
                        POP(x);
                        *(int *)addr = x;
                        break;
                    }
                    case 32: { /* @ */
                        int addr;
                        POP(addr);
                        PUSH(*(int *)addr);
                        break;
                    }
                    case 33: { /* c, */
                        if(memory - memory_orig >= MEMSZ) {
                            ret = OUT_OF_MEMORY_ERROR;
                            goto end_label;
                        }
                        int c;
                        POP(c);
                        *(memory++) = c;
                        break;
                    }
                    case 34: { /* here */
                        PUSH((int) memory);
                        break;
                    }
                    case 35: { /* max */
                        int a, b;
                        POP(b);
                        POP(a);
                        PUSH(a > b ? a : b);
                        break;
                    }
                    case 36: { /* fill */
                        int addr, len, c;
                        POP(c);
                        POP(len);
                        POP(addr);
                        memset((void *) addr, c, len);
                        break;
                    }
                    case 37: { /* 2drop */
                        int x;
                        POP(x);
                        POP(x);
                        break;
                    }
                    case 38: { /* move */
                        int src, dst, len;
                        POP(len);
                        POP(dst);
                        POP(src);
                        memmove((void *) dst, (const void *) src, len);
                        break;
                    }
                    default:
                        assert(0);
                }
                break;
            case OPCODE_DEFINED_WORD: {
                RETURN_PUSH(return_stack_len_base);
                int target_offset = pc_step(&pc);
                RETURN_PUSH(pc);
                return_stack_len_base = return_stack.len;
                pc = pc_initial + target_offset;
                break;
            }
            case OPCODE_RUNTIME_WORD: {
                int runtime_word_i = pc_step(&pc);
                ret = runtime_cbs[runtime_word_i](runtime_ctx, &stack);
                if(ret) goto end_label;
                break;
            }
            case OPCODE_PUSH_LITERAL:
                PUSH(pc_step(&pc));
                break;
            case OPCODE_EXIT_WORD:
                return_stack.data -= return_stack.len - return_stack_len_base;
                return_stack.len = return_stack_len_base;
                RETURN_POP(pc);
                RETURN_POP(return_stack_len_base);
                break;
            case OPCODE_PUSH_DATA_ADDRESS:
                PUSH((int) (data_start + pc_step(&pc)));
                break;
            case OPCODE_DECLARE_VARIABLE:
                while(((unsigned) memory) % 4u) memory++;
                variables[pc_step(&pc)] = memory;
                memory += 4;
                if(memory - memory_orig > MEMSZ) {
                    ret = OUT_OF_MEMORY_ERROR;
                    goto end_label;
                }
                break;
            case OPCODE_USE_VARIABLE:
                PUSH((int) variables[pc_step(&pc)]);
                break;
            case OPCODE_HALT:
                goto end_label;
            case OPCODE_BRANCH_IF_ZERO: {
                int cond;
                POP(cond);
                int target_offset = pc_step(&pc);
                if(cond == 0) pc = pc_initial + target_offset;
                break;
            }
            case OPCODE_BRANCH:
                pc = pc_initial + pc_step(&pc);
                break;
            case OPCODE_DO: {
                int end, start;
                POP(start);
                POP(end);
                RETURN_PUSH(end);
                RETURN_PUSH(start);
                break;
            }
            case OPCODE_LOOP: {
                int target_offset = pc_step(&pc);
                int end, i;
                RETURN_POP(i);
                RETURN_POP(end);
                i++;
                if(i < end) {
                    pc = pc_initial + target_offset;
                    RETURN_PUSH(end);
                    RETURN_PUSH(i);
                }
                break;
            }
            default:
                assert(0);
        }
    }

end_label:
    free(variables);
    free(memory_orig);
    free(stack_orig);
    free(return_stack_orig);
    return ret;
}

const mcp_forth_engine_t compact_bytecode_vm_engine = {
    .run=run
};
