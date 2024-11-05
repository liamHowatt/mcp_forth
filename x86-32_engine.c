#include "mcp_forth.h"

#define MAX_CALLBACKS 8

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
    void * (*memset)(void *, int, size_t);
    void * (*memmove)(void *, const void *, size_t);
    int (*compare)(void * a1, unsigned u1, void * a2, unsigned u2);
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

#ifdef M4_NO_TLS
static ctx_t * g_ctx;
#else
static __thread ctx_t * g_ctx;
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

static int compare(void * a1, unsigned u1, void * a2, unsigned u2)
{
    int result = memcmp(a1, a2, u1 < u2 ? u1 : u2);
    if(result == 0) {
        return u1 < u2 ? -1 : u1 > u2 ? 1 : 0;
    }
    return result < 0 ? -1 : 1;
}

int m4_x86_32_engine_run(
    const uint8_t * bin,
    int bin_len,
    uint8_t * memory_start,
    int memory_len,
    const m4_runtime_cb_array_t ** cb_arrays,
    const char ** missing_runtime_word_dst
) {
    uint8_t * memory_p = m4_align(memory_start);
    ctx_t * c = (ctx_t *) memory_p;
    memory_p += sizeof(ctx_t);
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) return M4_OUT_OF_MEMORY_ERROR;
    g_ctx = c;

    c->stack.data = (int *) memory_p;
    c->stack_base = (int *) memory_p;
    memory_p += 100 * 4;
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) return M4_OUT_OF_MEMORY_ERROR;
    c->stack.max = 100;
    c->stack.len = 0;

    int ** variables;
    const m4_runtime_cb_pair_t ** runtime_cbs;
    const uint8_t * program_start;
    int res = m4_unpack_binary_header(
        bin,
        bin_len,
        memory_p,
        m4_bytes_remaining(memory_start, memory_p, memory_len),
        cb_arrays,
        missing_runtime_word_dst,
        MAX_CALLBACKS,
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
    c->memset = memset;
    c->memmove = memmove;
    c->compare = compare;

    m4_x86_32_engine_run_asm(c, program_start);

    return 0;
}

ctx_t * m4_x86_32_engine_get_ctx(void)
{
    return g_ctx;
}
