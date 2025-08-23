#include "mcp_forth.h"
#include "global.h"

#ifndef M4_NO_THREAD
    #include <pthread.h>
    #include <alloca.h>
#endif

#define STACK_CELL_COUNT 100

typedef int (*callback_target_t)(int arg1, ...);

typedef struct {
    m4_stack_t stack;
    const m4_runtime_cb_pair_t ** runtime_cbs;
    const m4_runtime_cb_pair_t * const * special_runtime_cbs;
    uint8_t * memory;
    const uint8_t * data;
    uint8_t * callback_info;
    const uint8_t ** callback_word_locations;
    int * stack_base;
    int ** variables;
    const callback_target_t * callback_targets;
    const int * literals;
    int * return_stack;
    const uint8_t * bin_start;
    int total_extra_memory_size;
} ctx_t;

void m4_esp32s3_engine_run_asm(ctx_t * asm_struct, const uint8_t * program_start);
int m4_esp32s3_engine_callback_target_0(int arg1, ...);
int m4_esp32s3_engine_callback_target_1(int arg1, ...);
int m4_esp32s3_engine_callback_target_2(int arg1, ...);
int m4_esp32s3_engine_callback_target_3(int arg1, ...);
int m4_esp32s3_engine_callback_target_4(int arg1, ...);
int m4_esp32s3_engine_callback_target_5(int arg1, ...);
int m4_esp32s3_engine_callback_target_6(int arg1, ...);
int m4_esp32s3_engine_callback_target_7(int arg1, ...);

static const callback_target_t callback_targets[MAX_CALLBACKS] = {
    m4_esp32s3_engine_callback_target_0,
    m4_esp32s3_engine_callback_target_1,
    m4_esp32s3_engine_callback_target_2,
    m4_esp32s3_engine_callback_target_3,
    m4_esp32s3_engine_callback_target_4,
    m4_esp32s3_engine_callback_target_5,
    m4_esp32s3_engine_callback_target_6,
    m4_esp32s3_engine_callback_target_7,
};

#ifndef M4_NO_THREAD
static int thread_extra_stack_space_get(m4_stack_t * stack)
{
    ctx_t * root = (ctx_t *) stack;

    return sizeof(ctx_t) + STACK_CELL_COUNT * 4 * 2 + root->total_extra_memory_size;
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
    c->stack_base = (int *) memory_p;
    memory_p += STACK_CELL_COUNT * 4;
    c->stack.max = STACK_CELL_COUNT;
    c->stack.len = 0;

    c->return_stack = (int *) memory_p;
    memory_p += STACK_CELL_COUNT * 4;

    /* inherit these from the root */
    c->runtime_cbs = root->runtime_cbs;
    c->special_runtime_cbs = root->special_runtime_cbs;
    c->data = root->data;
    c->callback_info = root->callback_info;
    c->callback_word_locations = root->callback_word_locations;
    c->variables = root->variables;
    c->callback_targets = root->callback_targets;
    c->literals = root->literals;
    c->bin_start = root->bin_start;
    assert(root->total_extra_memory_size == m4_bytes_remaining(memory_start, memory_p, memory_len));
    c->total_extra_memory_size = root->total_extra_memory_size;

    c->memory = memory_p;

    global_thread_t global_thread;
    m4_global_thread_set(&global_thread, c);

    c->stack.data[0] = arg;
    c->stack.data += 1;
    c->stack.len += 1;

    m4_esp32s3_engine_run_asm(c, target);

    assert(c->stack.len >= 1);
    return (void *) c->stack.data[-1];
}

static const m4_engine_thread_create_param_t engine_thread_create_param = {
    .extra_stack_space_get_cb = thread_extra_stack_space_get,
    .thread_entry = thread_entry,
};

#else /*M4_NO_THREAD*/

static int m4_engine_thread_create(void * param, m4_stack_t * stack)
{
    assert(0);
}
#endif

static int thread_join(void * param, m4_stack_t * stack)
{
#ifndef M4_NO_THREAD
    assert(stack->len >= 1);
    void * retval;
    int res = pthread_join((pthread_t) stack->data[-1], &retval);
    assert(res == 0);
    stack->data[-1] = (int) retval;
#else
    assert(0);
#endif
    return 0;
}

#ifndef M4_NO_THREAD
    static const m4_runtime_cb_pair_t thread_create_cb_pair = {m4_engine_thread_create, (void *) &engine_thread_create_param};
#else
    static const m4_runtime_cb_pair_t thread_create_cb_pair = {m4_engine_thread_create, NULL};
#endif
static const m4_runtime_cb_pair_t thread_join_cb_pair = {thread_join, NULL};
static const m4_runtime_cb_pair_t * const special_runtime_cbs[] = {
    &thread_create_cb_pair,
    &thread_join_cb_pair,
};

int m4_esp32s3_engine_run(
    const uint8_t * bin,
    const uint8_t * code,
    uint8_t * memory_start,
    int memory_len,
    const m4_runtime_cb_array_t ** cb_arrays,
    const char ** missing_runtime_word_dst
)
{
    int res;

    int callbacks_used;
    res = m4_global_main_get_callbacks_used(&callbacks_used);
    if(res) return res;

    uint8_t * memory_p = m4_align(memory_start);

    ctx_t * c = (ctx_t *) memory_p;
    memory_p += sizeof(ctx_t);
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) return M4_OUT_OF_MEMORY_ERROR;

    c->stack.data = (int *) memory_p;
    c->stack_base = (int *) memory_p;
    memory_p += STACK_CELL_COUNT * 4;
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) return M4_OUT_OF_MEMORY_ERROR;
    c->stack.max = STACK_CELL_COUNT;
    c->stack.len = 0;

    c->return_stack = (int *) memory_p;
    memory_p += STACK_CELL_COUNT * 4;
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) return M4_OUT_OF_MEMORY_ERROR;

    int callback_count;

    res = m4_unpack_binary_header(
        bin,
        memory_p,
        m4_bytes_remaining(memory_start, memory_p, memory_len),
        cb_arrays,
        missing_runtime_word_dst,
        MAX_CALLBACKS - callbacks_used,
        &callback_count,
        &c->variables,
        &c->runtime_cbs,
        &c->data,
        &c->callback_info,
        &c->callback_word_locations,
        &c->literals,
        &c->bin_start,
        &c->memory
    );
    if(res) return res;

    if(code) c->bin_start = code;

    c->callback_targets = callback_targets + callbacks_used;
    c->special_runtime_cbs = special_runtime_cbs;
    c->total_extra_memory_size = m4_bytes_remaining(memory_start, c->memory, memory_len);

    m4_global_main_callbacks_set_ctx(callback_count, c);

    m4_esp32s3_engine_run_asm(c, c->bin_start);

    return 0;
}
