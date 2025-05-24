#include "mcp_forth.h"

#ifndef M4_NO_TLS
    #include <stdlib.h> /* malloc, free */
#endif

#define MAX_CALLBACKS 8

#ifndef M4_NO_TLS
    #define GLOBAL() (global_)
#else
    #define GLOBAL() (&global_)
#endif

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
} ctx_t;

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

typedef struct {
    int callbacks_used;
    ctx_t * ctxs[MAX_CALLBACKS];
} global_t;

#ifndef M4_NO_TLS
static __thread global_t * global_;
#else
static global_t global_;
#endif

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

int m4_x86_32_engine_run(
    const uint8_t * bin,
    int bin_len,
    uint8_t * memory_start,
    int memory_len,
    const m4_runtime_cb_array_t ** cb_arrays,
    const char ** missing_runtime_word_dst
) {
#ifndef M4_NO_TLS
    int callbacks_used = global_ ? global_->callbacks_used : 0;
#else
    int callbacks_used = global_.callbacks_used;
#endif

    uint8_t * memory_p = m4_align(memory_start);
    ctx_t * c = (ctx_t *) memory_p;
    memory_p += sizeof(ctx_t);
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) return M4_OUT_OF_MEMORY_ERROR;

    c->stack.data = (int *) memory_p;
    c->stack_base = (int *) memory_p;
    memory_p += 100 * 4;
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) return M4_OUT_OF_MEMORY_ERROR;
    c->stack.max = 100;
    c->stack.len = 0;

    int ** variables;
    const m4_runtime_cb_pair_t ** runtime_cbs;
    const uint8_t * program_start;

    c->callback_array_offset = callbacks_used;
    int callback_count;

    int res = m4_unpack_binary_header(
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

#ifndef M4_NO_TLS
    if(callback_count && !global_) {
        global_ = malloc(sizeof(global_t));
        assert(global_);
        global_->callbacks_used = 0;
    }
#endif
    global_t * global = GLOBAL();
    for(int i = 0; i < callback_count; i++) {
        global->ctxs[global->callbacks_used++] = c;
    }

    m4_x86_32_engine_run_asm(c, program_start);

    return 0;
}

void m4_x86_32_engine_global_cleanup(void)
{
#ifndef M4_NO_TLS
    free(global_);
    global_ = NULL;
#else
    global_.callbacks_used = 0;
#endif
}

global_t * m4_x86_32_engine_get_global(void)
{
    return GLOBAL();
}
