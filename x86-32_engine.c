#include "mcp_forth.h"
#include "global.h"

#ifndef M4_NO_THREAD
    #include <pthread.h>
    #include <semaphore.h>
    #include <alloca.h>
    _Static_assert(sizeof(pthread_t) == 4);
#endif

#define STACK_CELL_COUNT 100

typedef int (*callback_target_t)();

typedef struct {
    m4_stack_t stack;
    uint8_t * memory;
    const uint8_t * data;
    uint8_t * callback_info;
    const uint8_t ** callback_word_locations;
    int * stack_base;
    void * edi_table;
    void (*call_runtime_word)(void);
    const callback_target_t * callback_targets;
    int callback_array_offset;
    const m4_runtime_cb_pair_t * special_runtime_cbs;
    int total_extra_memory_size;
} ctx_t;

#ifndef M4_NO_THREAD
    typedef struct {
        sem_t done_with_arg_sem;
        ctx_t * root;
        int arg;
        const uint8_t * target;
        int memory_len;
    } thread_entry_arg_t;
#endif

void m4_x86_32_engine_run_asm(ctx_t * asm_struct, const uint8_t * program_start);
void m4_x86_32_engine_call_runtime_word(void);
int m4_x86_32_engine_callback_target_0();
int m4_x86_32_engine_callback_target_1();
int m4_x86_32_engine_callback_target_2();
int m4_x86_32_engine_callback_target_3();
int m4_x86_32_engine_callback_target_4();
int m4_x86_32_engine_callback_target_5();
int m4_x86_32_engine_callback_target_6();
int m4_x86_32_engine_callback_target_7();

static const callback_target_t callback_targets[MAX_CALLBACKS] = {
    m4_x86_32_engine_callback_target_0,
    m4_x86_32_engine_callback_target_1,
    m4_x86_32_engine_callback_target_2,
    m4_x86_32_engine_callback_target_3,
    m4_x86_32_engine_callback_target_4,
    m4_x86_32_engine_callback_target_5,
    m4_x86_32_engine_callback_target_6,
    m4_x86_32_engine_callback_target_7,
};

#ifndef M4_NO_THREAD
static void * thread_entry(void * arg_void)
{
    int res;

    thread_entry_arg_t * entry_arg = arg_void;

    ctx_t * root = entry_arg->root;
    int arg = entry_arg->arg;
    const uint8_t * target = entry_arg->target;
    int memory_len = entry_arg->memory_len;
    
    res = sem_post(&entry_arg->done_with_arg_sem);
    assert(res == 0);

    uint8_t * memory_start = alloca(memory_len);
    uint8_t * memory_p = memory_start;

    ctx_t * c = (ctx_t *) memory_p;
    memory_p += sizeof(ctx_t);

    c->stack.data = (int *) memory_p;
    c->stack_base = (int *) memory_p;
    memory_p += STACK_CELL_COUNT * 4;
    c->stack.max = STACK_CELL_COUNT;
    c->stack.len = 0;

    /* inherit these from the root */
    c->data = root->data;
    c->callback_info = root->callback_info;
    c->callback_word_locations = root->callback_word_locations;
    c->edi_table = root->edi_table;
    c->call_runtime_word = root->call_runtime_word;
    c->callback_targets = root->callback_targets;
    c->callback_array_offset = root->callback_array_offset;
    c->special_runtime_cbs = root->special_runtime_cbs;

    assert(root->total_extra_memory_size == m4_bytes_remaining(memory_start, memory_p, memory_len));
    c->total_extra_memory_size = root->total_extra_memory_size;
    c->memory = memory_p;

    global_thread_t global_thread;
    m4_global_thread_set(&global_thread, c);

    c->stack.data[0] = arg;
    c->stack.data += 1;
    c->stack.len += 1;

    m4_x86_32_engine_run_asm(c, target);

    assert(c->stack.len >= 1);
    return (void *) c->stack.data[-1];
}
#endif

static int thread_create(void * param, m4_stack_t * stack)
{
#ifndef M4_NO_THREAD
    int res;

    assert(stack->len >= 3);

    ctx_t * root = (ctx_t *) stack;
    int arg = stack->data[-3];
    int prio_change = stack->data[-2];
    const uint8_t * target = (const uint8_t *) stack->data[-1];

    pthread_attr_t attr;
    res = pthread_attr_init(&attr);
    assert(res == 0);

    size_t default_stack_size;
    res = pthread_attr_getstacksize(&attr, &default_stack_size);
    assert(res == 0);
    int memory_len = sizeof(ctx_t) + STACK_CELL_COUNT * 4 + root->total_extra_memory_size;
    res = pthread_attr_setstacksize(&attr, default_stack_size + memory_len);
    assert(res == 0);

    res = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    assert(res == 0);

    int this_policy;
    struct sched_param this_sched_param;
    res = pthread_getschedparam(pthread_self(), &this_policy, &this_sched_param);
    assert(res == 0);

    this_sched_param.sched_priority += prio_change;

    res = pthread_attr_setschedpolicy(&attr, this_policy);
    assert(res == 0);
    res = pthread_attr_setschedparam(&attr, &this_sched_param);
    assert(res == 0);

    res = pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    assert(res == 0);

    thread_entry_arg_t entry_arg;
    entry_arg.root = root;
    entry_arg.arg = arg;
    entry_arg.target = target;
    entry_arg.memory_len = memory_len;

    res = sem_init(&entry_arg.done_with_arg_sem, 0, 0);
    assert(res == 0);

    pthread_t thread;
    res = pthread_create(&thread, &attr, thread_entry, &entry_arg);
    assert(res == 0);

    res = pthread_attr_destroy(&attr);
    assert(res == 0);

    stack->data[-3] = (int) thread;
    stack->data -= 2;
    stack->len -= 2;

    /* don't let `entry_arg` go out of scope until the new thread is done with it */
    res = sem_wait(&entry_arg.done_with_arg_sem);
    assert(res == 0);
    res = sem_destroy(&entry_arg.done_with_arg_sem);
    assert(res == 0);
#else
    assert(0);
#endif
    return 0;
}

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

static const m4_runtime_cb_pair_t special_runtime_cbs[] = {
    {thread_create, NULL},
    {thread_join, NULL},
};

int m4_x86_32_engine_run(
    const uint8_t * bin,
    int bin_len,
    uint8_t * memory_start,
    int memory_len,
    const m4_runtime_cb_array_t ** cb_arrays,
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
    c->stack_base = (int *) memory_p;
    memory_p += STACK_CELL_COUNT * 4;
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) return M4_OUT_OF_MEMORY_ERROR;
    c->stack.max = STACK_CELL_COUNT;
    c->stack.len = 0;

    int ** variables;
    const m4_runtime_cb_pair_t ** runtime_cbs;
    const uint8_t * program_start;

    c->callback_array_offset = callbacks_used;
    int callback_count;

    res = m4_unpack_binary_header(
        bin,
        bin_len,
        memory_p,
        m4_bytes_remaining(memory_start, memory_p, memory_len),
        cb_arrays,
        missing_runtime_word_dst,
        MAX_CALLBACKS - callbacks_used,
        &callback_count,
        &variables,
        &runtime_cbs,
        &c->data,
        &c->callback_info,
        &c->callback_word_locations,
        &program_start,
        &c->memory
    );
    if(res) return res;

    c->edi_table = runtime_cbs - 1;
    c->call_runtime_word = m4_x86_32_engine_call_runtime_word;
    c->callback_targets = callback_targets;
    c->special_runtime_cbs = special_runtime_cbs;
    c->total_extra_memory_size = m4_bytes_remaining(memory_start, c->memory, memory_len);

    m4_global_main_callbacks_set_ctx(callback_count, c);

    m4_x86_32_engine_run_asm(c, program_start);

    return 0;
}
