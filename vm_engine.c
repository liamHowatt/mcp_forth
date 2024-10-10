#include "mcp_forth.h"

#define STKSZ 1000
#define RETURN_STKSZ 1000
#define MEMSZ 4000

#define RASSERT(expr, ecode) do { if(!(expr)) return (ecode); } while(0)

#define POP(var)  do { int ret; if((ret = pop(&(var), &c->stack))) return ret; } while(0)
#define PUSH(var) do { int ret; if((ret = push((var), &c->stack))) return ret; } while(0)
#define RETURN_POP(var)  do { int ret; if((ret = pop((int *) &(var), &c->return_stack))) return ret; } while(0)
#define RETURN_PUSH(var) do { int ret; if((ret = push((int) (var), &c->return_stack))) return ret; } while(0)

typedef struct {
    stack_t stack; /* must be first */
    stack_t return_stack;
    int return_stack_len_base;
    uint8_t * memory_base;
    uint8_t * memory;
    uint8_t ** variables;
    const uint8_t * pc;
    const uint8_t * data_start;
    runtime_cb * runtime_cbs;
    void * runtime_ctx;
} ectx_t;

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

static int run_inner(
    ectx_t * c
) {
    while(1) {
        const uint8_t * pc_initial = c->pc;
        int op = pc_step(&c->pc);

        int builtin_num = -1;
        if(op >= OPCODE_LAST_ && op <= NUM_MAX_ONE_BYTE) {
            builtin_num = op - OPCODE_LAST_;
            op = OPCODE_BUILTIN_WORD;
        }

        switch(op) {
            case OPCODE_BUILTIN_WORD:
                if(builtin_num == -1) {
                    builtin_num = pc_step(&c->pc);
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
                        c->memory += x;
                        if(c->memory - c->memory_base > MEMSZ) {
                            return OUT_OF_MEMORY_ERROR;
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
                        PUSH(c->return_stack.data[-1]);
                        break;
                    }
                    case 15: { /* j */
                        PUSH(c->return_stack.data[-3]);
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
                        if(c->stack.len <= n) {
                            return STACK_UNDERFLOW_ERROR;
                        }
                        PUSH(c->stack.data[-n - 1]);
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
                        PUSH(c->stack.len);
                        break;
                    }
                    case 28: { /* c! */
                        int chr, addr;
                        POP(addr);
                        POP(chr);
                        *(uint8_t *)addr = chr;
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
                        if(c->memory - c->memory_base >= MEMSZ) {
                            return OUT_OF_MEMORY_ERROR;
                        }
                        int chr;
                        POP(chr);
                        *(c->memory++) = chr;
                        break;
                    }
                    case 34: { /* here */
                        PUSH((int) c->memory);
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
                        int addr, len, chr;
                        POP(chr);
                        POP(len);
                        POP(addr);
                        memset((void *) addr, chr, len);
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
                    case 39: { /* execute */
                        RETURN_PUSH(c->return_stack_len_base);
                        RETURN_PUSH(c->pc);
                        c->return_stack_len_base = c->return_stack.len;
                        int fn;
                        POP(fn);
                        c->pc = (const uint8_t *) fn;
                        break;
                    }
                    default:
                        assert(0);
                }
                break;
            case OPCODE_DEFINED_WORD: {
                RETURN_PUSH(c->return_stack_len_base);
                int target_offset = pc_step(&c->pc);
                RETURN_PUSH(c->pc);
                c->return_stack_len_base = c->return_stack.len;
                c->pc = pc_initial + target_offset;
                break;
            }
            case OPCODE_RUNTIME_WORD: {
                int runtime_word_i = pc_step(&c->pc);
                int ret = c->runtime_cbs[runtime_word_i](c->runtime_ctx, &c->stack);
                if(ret) return ret;
                break;
            }
            case OPCODE_PUSH_LITERAL:
                PUSH(pc_step(&c->pc));
                break;
            case OPCODE_EXIT_WORD:
                c->return_stack.data -= c->return_stack.len - c->return_stack_len_base;
                c->return_stack.len = c->return_stack_len_base;
                RETURN_POP(c->pc);
                RETURN_POP(c->return_stack_len_base);
                if(c->pc == NULL) return 0;
                break;
            case OPCODE_PUSH_DATA_ADDRESS:
                PUSH((int) (c->data_start + pc_step(&c->pc)));
                break;
            case OPCODE_DECLARE_VARIABLE:
                while(((unsigned) c->memory) % 4u) c->memory++;
                c->variables[pc_step(&c->pc)] = c->memory;
                c->memory += 4;
                if(c->memory - c->memory_base > MEMSZ) {
                    return OUT_OF_MEMORY_ERROR;
                }
                break;
            case OPCODE_USE_VARIABLE:
                PUSH((int) c->variables[pc_step(&c->pc)]);
                break;
            case OPCODE_HALT:
                return 0;
            case OPCODE_BRANCH_IF_ZERO: {
                int cond;
                POP(cond);
                int target_offset = pc_step(&c->pc);
                if(cond == 0) c->pc = pc_initial + target_offset;
                break;
            }
            case OPCODE_BRANCH:
                c->pc = pc_initial + pc_step(&c->pc);
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
                int target_offset = pc_step(&c->pc);
                int end, i;
                RETURN_POP(i);
                RETURN_POP(end);
                i++;
                if(i < end) {
                    c->pc = pc_initial + target_offset;
                    RETURN_PUSH(end);
                    RETURN_PUSH(i);
                }
                break;
            }
            case OPCODE_PUSH_OFFSET_ADDRESS: {
                int offset = pc_step(&c->pc);
                PUSH((int) pc_initial + offset);
                break;
            }
            default:
                assert(0);
        }
    }
}

static int run(
    const uint8_t * program_start,
    const uint8_t * data_start,
    runtime_cb * runtime_cbs,
    void * runtime_ctx,
    int variable_count
) {
    ectx_t c;

    int * stack_orig = malloc(sizeof(int) * STKSZ);
    assert(stack_orig);
    c.stack.data = stack_orig;
    c.stack.max = STKSZ;
    c.stack.len = 0;

    int * return_stack_orig = malloc(sizeof(int) * RETURN_STKSZ);
    assert(return_stack_orig);
    c.return_stack.data = return_stack_orig;
    c.return_stack.max = RETURN_STKSZ;
    c.return_stack.len = 0;
    c.return_stack_len_base = 0;

    uint8_t * memory_orig = calloc(MEMSZ, 1);
    assert(memory_orig);
    c.memory_base = memory_orig;
    c.memory = memory_orig;

    c.variables = malloc(variable_count * sizeof(uint8_t *));
    assert(c.variables);

    c.pc = program_start;
    c.data_start = data_start;
    c.runtime_cbs = runtime_cbs;
    c.runtime_ctx = runtime_ctx;

    int ret = run_inner(&c);

    free(c.variables);
    free(memory_orig);
    free(stack_orig);
    free(return_stack_orig);
    return ret;
}

static int call_defined_word(const uint8_t * defined_word_ptr, stack_t * stack)
{
    int ret = 0;
    ectx_t * c = (ectx_t *) stack;
    RETURN_PUSH(c->return_stack_len_base);
    RETURN_PUSH(NULL);
    c->return_stack_len_base = c->return_stack.len;
    const uint8_t * pc_save = c->pc;
    c->pc = defined_word_ptr;
    ret = run_inner(c);
    c->pc = pc_save;
    return ret;
}

const mcp_forth_engine_t compact_bytecode_vm_engine = {
    .run=run,
    .call_defined_word=call_defined_word
};
