#include "mcp_forth.h"
#include "global.h"
#include <stdarg.h>

#ifndef M4_NO_THREAD
    #include <pthread.h>
    #include <alloca.h>
#endif

#define STACK_CELL_COUNT 100
#define RETURN_STACK_CELL_COUNT 100

#define RASSERT(expr, ecode) do { if(!(expr)) return (ecode); } while(0)

#define POP(var)  do { int ret; if((ret = pop((var), &c->stack))) return ret; } while(0)
#define PUSH(var) do { int ret; if((ret = push((var), &c->stack))) return ret; } while(0)
#define RETURN_POP(var)  do { int ret; if((ret = pop((int *) (var), &c->return_stack))) return ret; } while(0)
#define RETURN_PUSH(var) do { int ret; if((ret = push((int) (var), &c->return_stack))) return ret; } while(0)

union iu {int i; unsigned u;};

typedef struct {
    m4_stack_t stack;
    m4_stack_t return_stack;
    int return_stack_len_base;
    uint8_t * memory_base;
    uint8_t * memory;
    int memory_len;
    const m4_runtime_cb_pair_t ** runtime_cbs;
    const uint8_t * data_start;
    union {int * ip; int i;} * variables_and_constants;
    uint8_t * callback_info;
    const uint8_t ** callback_word_locations;
    const uint8_t * pc;
    int callback_array_offset;
    int total_extra_memory_size;
} ctx_t;

static int run_inner(ctx_t * c);

static int pc_step(const uint8_t ** pc_p) {
    int ret = m4_num_decode(*pc_p, pc_p);
    return ret;
}

static int pop(int * dst, m4_stack_t * stack) {
    if(stack->len <= 0) return M4_STACK_UNDERFLOW_ERROR;
    stack->data -= 1;
    stack->len -= 1;
    *dst = stack->data[0];
    return 0;
}

static int push(int src, m4_stack_t * stack) {
    if(stack->len >= stack->max) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = src;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

static int callback_handle(int cb_i, int arg1, va_list ap)
{
    ctx_t * c = m4_global_get_ctx(cb_i);
    cb_i -= c->callback_array_offset;

    uint8_t cb_info = c->callback_info[cb_i];
    int n_params = cb_info >> 1;
    assert(0 == push(arg1, &c->stack));
    for(int i = 1; i < n_params; i++) {
        assert(0 == push(va_arg(ap, int), &c->stack));
    }
    assert(0 == push(c->return_stack_len_base, &c->return_stack));
    assert(0 == push((int) NULL, &c->return_stack));
    c->return_stack_len_base = c->return_stack.len;
    const uint8_t * pc_save = c->pc;
    c->pc = c->callback_word_locations[cb_i];
    assert(0 == run_inner(c));
    c->pc = pc_save;
    bool has_return_val = cb_info & 1;
    int ret;
    if(has_return_val) assert(0 == pop(&ret, &c->stack));
    return ret; /* uninitialized is ok */
}

#define MCP_FORTH_GENERATED_VM_ENGINE
#include "mcp_forth_generated.h"
#undef MCP_FORTH_GENERATED_VM_ENGINE

#ifndef M4_NO_THREAD
static int thread_extra_stack_space_get(m4_stack_t * stack)
{
    ctx_t * root = (ctx_t *) stack;
    
    return sizeof(ctx_t) + STACK_CELL_COUNT * 4 + RETURN_STACK_CELL_COUNT * 4
           + root->total_extra_memory_size;
}

static void * thread_entry(void * arg_void)
{
    int res;

    m4_engine_thread_entry_arg_t * entry_arg = arg_void;

    ctx_t * root = (ctx_t *) entry_arg->stack;
    int arg = entry_arg->arg;
    const uint8_t * target = entry_arg->target;

    res = sem_post(&entry_arg->done_with_arg_sem);
    assert(res == 0);

    int memory_len = thread_extra_stack_space_get((m4_stack_t *) root);

    uint8_t * memory_start = alloca(memory_len);
    uint8_t * memory_p = memory_start;

    ctx_t * c = (ctx_t *) memory_p;
    memory_p += sizeof(ctx_t);

    c->stack.data = (int *) memory_p;
    memory_p += STACK_CELL_COUNT * 4;
    c->stack.max = STACK_CELL_COUNT;
    c->stack.len = 0;

    c->return_stack.data = (int *) memory_p;
    memory_p += RETURN_STACK_CELL_COUNT * 4;
    c->return_stack.max = RETURN_STACK_CELL_COUNT;
    c->return_stack.len = 0;
    c->return_stack_len_base = 2;

    c->memory_base = memory_start;
    c->memory_len = memory_len;

    /* inherit these from the root */
    c->runtime_cbs = root->runtime_cbs;
    c->data_start = root->data_start;
    c->variables_and_constants = root->variables_and_constants;
    c->callback_info = root->callback_info;
    c->callback_word_locations = root->callback_word_locations;
    c->callback_array_offset = root->callback_array_offset;

    assert(root->total_extra_memory_size == m4_bytes_remaining(memory_start, memory_p, memory_len));
    c->total_extra_memory_size = root->total_extra_memory_size;
    c->memory = memory_p;

    c->pc = target;

    global_thread_t global_thread;
    m4_global_thread_set(&global_thread, c);

    assert(0 == push(arg, &c->stack));

    assert(0 == push(0, &c->return_stack));
    assert(0 == push((int) NULL, &c->return_stack));

    res = run_inner(c);
    assert(res == 0);

    int retval;
    res = pop(&retval, &c->stack);
    assert(res == 0);

    return (void *) retval;
}

static const m4_engine_thread_create_param_t engine_thread_create_param = {
    .extra_stack_space_get_cb = thread_extra_stack_space_get,
    .thread_entry = thread_entry,
};
#endif

static int run_inner(ctx_t * c) {
    while(1) {
        const uint8_t * pc_initial = c->pc;
        int op = pc_step(&c->pc);

        int builtin_num = -1;
        if(op >= M4_OPCODE_LAST_ && op <= M4_NUM_MAX_ONE_BYTE) {
            builtin_num = op - M4_OPCODE_LAST_;
            op = M4_OPCODE_BUILTIN_WORD;
        }

        switch(op) {
            case M4_OPCODE_BUILTIN_WORD:
                if(builtin_num == -1) {
                    builtin_num = pc_step(&c->pc);
                }
                switch (builtin_num) {
                    case 0: { /* dup */
                        int x;
                        POP(&x);
                        PUSH(x);
                        PUSH(x);
                        break;
                    }
                    case 1: { /* - */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l - r);
                        break;
                    }
                    case 2: { /* > */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l > r ? -1 : 0);
                        break;
                    }
                    case 3: { /* + */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l + r);
                        break;
                    }
                    case 4: { /* = */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l == r ? -1 : 0);
                        break;
                    }
                    case 5: { /* over */
                        int b, a;
                        POP(&a);
                        POP(&b);
                        PUSH(b);
                        PUSH(a);
                        PUSH(b);
                        break;
                    }
                    case 6: { /* mod */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l % r);
                        break;
                    }
                    case 7: { /* drop */
                        int x;
                        POP(&x);
                        break;
                    }
                    case 8: { /* swap */
                        int b, a;
                        POP(&a);
                        POP(&b);
                        PUSH(a);
                        PUSH(b);
                        break;
                    }
                    case 9: { /* * */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l * r);
                        break;
                    }
                    case 10: { /* allot */
                        int x;
                        POP(&x);
                        c->memory += x;
                        int bytes_remaining = m4_bytes_remaining(c->memory_base, c->memory, c->memory_len);
                        if(bytes_remaining < 0) return M4_OUT_OF_MEMORY_ERROR;
                        if(bytes_remaining > c->total_extra_memory_size) return M4_DATA_SPACE_POINTER_OUT_OF_BOUNDS_ERROR;
                        break;
                    }
                    case 11: { /* 1+ */
                        int x;
                        POP(&x);
                        PUSH(x + 1);
                        break;
                    }
                    case 12: { /* invert */
                        int x;
                        POP(&x);
                        PUSH(~x);
                        break;
                    }
                    case 13: { /* 0= */
                        int x;
                        POP(&x);
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
                        POP(&x);
                        PUSH(x - 1);
                        break;
                    }
                    case 17: { /* rot */
                        int x1, x2, x3;
                        POP(&x3);
                        POP(&x2);
                        POP(&x1);
                        PUSH(x2);
                        PUSH(x3);
                        PUSH(x1);
                        break;
                    }
                    case 18: { /* pick */
                        int n;
                        POP(&n);
                        if(c->stack.len <= n) {
                            return M4_STACK_UNDERFLOW_ERROR;
                        }
                        PUSH(c->stack.data[-n - 1]);
                        break;
                    }
                    case 19: { /* xor */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l ^ r);
                        break;
                    }
                    case 20: { /* lshift */
                        union iu l, r;
                        POP(&r.i);
                        POP(&l.i);
                        PUSH(l.u << r.u);
                        break;
                    }
                    case 21: { /* rshift */
                        union iu l, r;
                        POP(&r.i);
                        POP(&l.i);
                        PUSH(l.u >> r.u);
                        break;
                    }
                    case 22: { /* < */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l < r ? -1 : 0);
                        break;
                    }
                    case 23: { /* >= */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l >= r ? -1 : 0);
                        break;
                    }
                    case 24: { /* <= */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l <= r ? -1 : 0);
                        break;
                    }
                    case 25: { /* or */
                        union iu l, r;
                        POP(&r.i);
                        POP(&l.i);
                        PUSH(l.u | r.u);
                        break;
                    }
                    case 26: { /* and */
                        union iu l, r;
                        POP(&r.i);
                        POP(&l.i);
                        PUSH(l.u & r.u);
                        break;
                    }
                    case 27: { /* depth */
                        PUSH(c->stack.len);
                        break;
                    }
                    case 28: { /* c! */
                        int chr, addr;
                        POP(&addr);
                        POP(&chr);
                        *(uint8_t *)addr = chr;
                        break;
                    }
                    case 29: { /* c@ */
                        int addr;
                        POP(&addr);
                        PUSH(*(uint8_t *)addr);
                        break;
                    }
                    case 30: { /* 0< */
                        int x;
                        POP(&x);
                        PUSH(x < 0 ? -1 : 0);
                        break;
                    }
                    case 31: { /* ! */
                        int x, addr;
                        POP(&addr);
                        POP(&x);
                        *(int *)addr = x;
                        break;
                    }
                    case 32: { /* @ */
                        int addr;
                        POP(&addr);
                        PUSH(*(int *)addr);
                        break;
                    }
                    case 33: { /* c, */
                        if(m4_bytes_remaining(c->memory_base, c->memory, c->memory_len) < 1) {
                            return M4_OUT_OF_MEMORY_ERROR;
                        }
                        int chr;
                        POP(&chr);
                        *(c->memory++) = chr;
                        break;
                    }
                    case 34: { /* here */
                        PUSH((int) c->memory);
                        break;
                    }
                    case 35: { /* max */
                        int a, b;
                        POP(&b);
                        POP(&a);
                        PUSH(a > b ? a : b);
                        break;
                    }
                    case 36: { /* 2* */
                        int x;
                        POP(&x);
                        PUSH(x * 2);
                        break;
                    }
                    case 37: { /* 2drop */
                        int x;
                        POP(&x);
                        POP(&x);
                        break;
                    }
                    case 38: { /* tuck */
                        int x1, x2;
                        POP(&x2);
                        POP(&x1);
                        PUSH(x2);
                        PUSH(x1);
                        PUSH(x2);
                        break;
                    }
                    case 39: { /* cell+ */
                        int x;
                        POP(&x);
                        PUSH(x + 4);
                        break;
                    }
                    case 40: /* align */
                        c->memory = m4_align(c->memory);
                        if(m4_bytes_remaining(c->memory_base, c->memory, c->memory_len) < 0) {
                            return M4_OUT_OF_MEMORY_ERROR;
                        }
                        break;
                    case 41: { /* , */
                        if(m4_bytes_remaining(c->memory_base, c->memory, c->memory_len) < 4) {
                            return M4_OUT_OF_MEMORY_ERROR;
                        }
                        int x;
                        POP(&x);
                        *(int *) c->memory = x;
                        c->memory += 4;
                        break;
                    }
                    case 42: { /* ?dup */
                        int x;
                        POP(&x);
                        PUSH(x);
                        if(x)
                            PUSH(x);
                        break;
                    }
                    case 43: { /* <> */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l != r ? -1 : 0);
                        break;
                    }
                    case 44: { /* >r */
                        int x;
                        POP(&x);
                        RETURN_PUSH(x);
                        break;
                    }
                    case 45: { /* r> */
                        int x;
                        RETURN_POP(&x);
                        PUSH(x);
                        break;
                    }
                    case 46: { /* 2>r */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        RETURN_PUSH(l);
                        RETURN_PUSH(r);
                        break;
                    }
                    case 47: { /* 2r> */
                        int l, r;
                        RETURN_POP(&r);
                        RETURN_POP(&l);
                        PUSH(l);
                        PUSH(r);
                        break;
                    }
                    case 48: { /* 2/ */
                        int x;
                        POP(&x);
                        PUSH(x / 2);
                        break;
                    }
                    case 49: { /* 2dup */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l);
                        PUSH(r);
                        PUSH(l);
                        PUSH(r);
                        break;
                    }
                    case 50: { /* 0<> */
                        int x;
                        POP(&x);
                        PUSH(x != 0 ? -1 : 0);
                        break;
                    }
                    case 51: { /* 0> */
                        int x;
                        POP(&x);
                        PUSH(x > 0 ? -1 : 0);
                        break;
                    }
                    case 52: { /* w@ */
                        int addr;
                        POP(&addr);
                        PUSH(*(uint16_t *)addr);
                        break;
                    }
                    case 53: { /* w! */
                        int chr, addr;
                        POP(&addr);
                        POP(&chr);
                        *(uint16_t *)addr = chr;
                        break;
                    }
                    case 54: { /* +! */
                        int n, addr;
                        POP(&addr);
                        POP(&n);
                        *(int *) addr += n;
                        break;
                    }
                    case 55: { /* / */
                        int l, r;
                        POP(&r);
                        POP(&l);
                        PUSH(l / r);
                        break;
                    }
                    case 56: { /* cells */
                        int x;
                        POP(&x);
                        PUSH(x * 4);
                        break;
                    }
                    case 57: { /* nip */
                        int top, next;
                        POP(&top);
                        POP(&next);
                        PUSH(top);
                        break;
                    }
                    case 58: { /* min */
                        int a, b;
                        POP(&b);
                        POP(&a);
                        PUSH(a < b ? a : b);
                        break;
                    }
                    case 59: { /* abs */
                        int a;
                        POP(&a);
                        PUSH(a >= 0 ? a : -a);
                        break;
                    }
                    case 60: { /* r@ */
                        int a;
                        RETURN_POP(&a);
                        RETURN_PUSH(a);
                        PUSH(a);
                        break;
                    }
                    case 61: { /* w, */
                        if(m4_bytes_remaining(c->memory_base, c->memory, c->memory_len) < 2) {
                            return M4_OUT_OF_MEMORY_ERROR;
                        }
                        int x;
                        POP(&x);
                        *(uint16_t *) c->memory = x;
                        c->memory += 2;
                        break;
                    }
                    case 62: { /* neg */
                        int x;
                        POP(&x);
                        PUSH(-x);
                        break;
                    }
                    default:
                        assert(0);
                }
                break;
            case M4_OPCODE_DEFINED_WORD: {
                RETURN_PUSH(c->return_stack_len_base);
                int target_offset = pc_step(&c->pc);
                RETURN_PUSH(c->pc);
                c->return_stack_len_base = c->return_stack.len;
                c->pc = pc_initial + target_offset;
                break;
            }
            case M4_OPCODE_RUNTIME_WORD: {
                int runtime_word_i = pc_step(&c->pc);
                const m4_runtime_cb_pair_t * cb_pair = c->runtime_cbs[runtime_word_i];
                if(cb_pair->cb == m4_lit) {
                    PUSH((int) cb_pair->param);
                } else {
                    int ret = cb_pair->cb(cb_pair->param, &c->stack);
                    if(ret) return ret;
                }
                break;
            }
            case M4_OPCODE_PUSH_LITERAL:
                PUSH(pc_step(&c->pc));
                break;
            case M4_OPCODE_EXIT_WORD:
                c->return_stack.data -= c->return_stack.len - c->return_stack_len_base;
                c->return_stack.len = c->return_stack_len_base;
                RETURN_POP(&c->pc);
                RETURN_POP(&c->return_stack_len_base);
                if(c->pc == NULL) return 0;
                break;
            case M4_OPCODE_PUSH_DATA_ADDRESS:
                PUSH((int) (c->data_start + pc_step(&c->pc)));
                break;
            case M4_OPCODE_DECLARE_VARIABLE:
                c->memory = m4_align(c->memory);
                c->variables_and_constants[pc_step(&c->pc)].ip = (int *) c->memory;
                c->memory += 4;
                if(m4_bytes_remaining(c->memory_base, c->memory, c->memory_len) < 0) {
                    return M4_OUT_OF_MEMORY_ERROR;
                }
                break;
            case M4_OPCODE_USE_VARIABLE_OR_CONSTANT:
                PUSH(c->variables_and_constants[pc_step(&c->pc)].i);
                break;
            case M4_OPCODE_HALT:
                return 0;
            case M4_OPCODE_BRANCH_IF_ZERO: {
                int cond;
                POP(&cond);
                int target_offset = pc_step(&c->pc);
                if(cond == 0) c->pc = pc_initial + target_offset;
                break;
            }
            case M4_OPCODE_BRANCH:
                c->pc = pc_initial + pc_step(&c->pc);
                break;
            case M4_OPCODE_DO: {
                int end, start;
                POP(&start);
                POP(&end);
                RETURN_PUSH(end);
                RETURN_PUSH(start);
                break;
            }
            case M4_OPCODE_LOOP: {
                int target_offset = pc_step(&c->pc);
                int end, i;
                RETURN_POP(&i);
                RETURN_POP(&end);
                i++;
                if(i < end) {
                    c->pc = pc_initial + target_offset;
                    RETURN_PUSH(end);
                    RETURN_PUSH(i);
                }
                break;
            }
            case M4_OPCODE_PUSH_OFFSET_ADDRESS: {
                int offset = pc_step(&c->pc);
                PUSH((int) pc_initial + offset);
                break;
            }
            case M4_OPCODE_PUSH_CALLBACK:
                PUSH((int) callback_targets[c->callback_array_offset + pc_step(&c->pc)]);
                break;
            case M4_OPCODE_DECLARE_CONSTANT: {
                int x;
                POP(&x);
                c->variables_and_constants[pc_step(&c->pc)].i = x;
                break;
            }
            case M4_OPCODE_EXECUTE: {
                RETURN_PUSH(c->return_stack_len_base);
                RETURN_PUSH(c->pc);
                c->return_stack_len_base = c->return_stack.len;
                int fn;
                POP(&fn);
                c->pc = (const uint8_t *) fn;
                break;
            }
#ifndef M4_NO_THREAD
            case M4_OPCODE_THREAD_CREATE: {
                int res = m4_engine_thread_create((void *) &engine_thread_create_param, (m4_stack_t *) c);
                assert(res == 0);
                break;
            }
            case M4_OPCODE_THREAD_JOIN: {
                int thread_hdl;
                POP(&thread_hdl);
                void * retval;
                int res = pthread_join((pthread_t) thread_hdl, &retval);
                assert(res == 0);
                PUSH((int) retval);
                break;
            }
#else
            case M4_OPCODE_THREAD_CREATE:
            case M4_OPCODE_THREAD_JOIN:
                return M4_THREADS_NOT_ENABLED_ERROR;
#endif
            default:
                assert(0);
        }
    }
}

int m4_vm_engine_run(
    const uint8_t * bin,
    const uint8_t * code,
    uint8_t * memory_start,
    int memory_len,
    const m4_runtime_cb_array_t * const * cb_arrays,
    const char ** missing_runtime_word_dst
) {
    int res;

    int callbacks_used;
    res = m4_global_main_get_callbacks_used(&callbacks_used);
    if(res) return res;

    uint8_t * memory_p = m4_align(memory_start);

    ctx_t * c = (ctx_t *) memory_p;
    memory_p += sizeof(ctx_t);
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) return M4_OUT_OF_MEMORY_ERROR;

    c->stack.data = (int *) memory_p;
    memory_p += STACK_CELL_COUNT * 4;
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) return M4_OUT_OF_MEMORY_ERROR;
    c->stack.max = STACK_CELL_COUNT;
    c->stack.len = 0;

    c->return_stack.data = (int *) memory_p;
    memory_p += RETURN_STACK_CELL_COUNT * 4;
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) return M4_OUT_OF_MEMORY_ERROR;
    c->return_stack.max = RETURN_STACK_CELL_COUNT;
    c->return_stack.len = 0;
    c->return_stack_len_base = 0;

    c->memory_base = memory_start;
    c->memory_len = memory_len;

    c->callback_array_offset = callbacks_used;
    int callback_count;

    res = m4_unpack_binary_header(
        bin,
        memory_p,
        m4_bytes_remaining(memory_start, memory_p, memory_len),
        cb_arrays,
        missing_runtime_word_dst,
        M4_MAX_CALLBACKS - callbacks_used,
        &callback_count,
        (int ***) &c->variables_and_constants,
        &c->runtime_cbs,
        &c->data_start,
        &c->callback_info,
        &c->callback_word_locations,
        NULL,
        &c->pc,
        &c->memory
    );
    if(res) return res;

    if(code) c->pc = code;

    c->total_extra_memory_size = m4_bytes_remaining(memory_start, c->memory, memory_len);

    m4_global_main_callbacks_set_ctx(callback_count, c);

    return run_inner(c);
}
