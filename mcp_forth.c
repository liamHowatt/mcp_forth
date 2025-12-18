#include "mcp_forth.h"
#include "global.h"

#ifndef M4_NO_TLS
    #include <stdlib.h> /* malloc+free for global */
#endif

#ifndef M4_NO_THREAD
    #include <pthread.h>
    _Static_assert(sizeof(pthread_t) == 4);
    #ifdef __NuttX__
        #include <sched.h> /* struct sched_param */
    #endif
#endif

#if defined(M4_NO_TLS) && !defined(M4_NO_THREAD)
    #error TLS is needed for thread feature
#endif

#ifndef M4_NO_THREAD
    __thread global_t * m4_global_;
#elif !defined(M4_NO_TLS)
    __thread global_main_t * m4_global_;
#else
    global_main_t m4_global_;
#endif

void * m4_global_get_ctx(int cb_i)
{
#ifndef M4_NO_THREAD
    global_t * global = m4_global_;
    if(global->is_main) {
        global_main_t * global_main = (global_main_t *) global;
        return global_main->ctxs[cb_i];
    } else {
        global_thread_t * global_thread = (global_thread_t *) global;
        return global_thread->ctx;
    }
#elif !defined(M4_NO_TLS)
    return m4_global_->ctxs[cb_i];
#else
    return m4_global_.ctxs[cb_i];
#endif
}

#ifndef M4_NO_THREAD
void m4_global_thread_set(global_thread_t * global_thread, void * ctx)
{
    m4_global_ = (global_t *) global_thread;
    global_thread->base.is_main = false;
    global_thread->ctx = ctx;
}
#endif

int m4_global_main_get_callbacks_used(int * callbacks_used_dst)
{
#ifndef M4_NO_THREAD
    if(m4_global_ && !m4_global_->is_main) return M4_THREADS_CANNOT_LAUNCH_PROGRAMS_ERROR;
    global_main_t * global_main = (global_main_t *) m4_global_;
    *callbacks_used_dst = global_main ? global_main->callbacks_used : 0;
#elif !defined(M4_NO_TLS)
    global_main_t * global_main = m4_global_;
    *callbacks_used_dst = global_main ? global_main->callbacks_used : 0;
#else
    *callbacks_used_dst = m4_global_.callbacks_used;
#endif
    return 0;
}

void m4_global_main_callbacks_set_ctx(int callback_count, void * ctx)
{
    global_main_t * global_main;

#ifndef M4_NO_THREAD
    global_main = (global_main_t *) m4_global_;
#elif !defined(M4_NO_TLS)
    global_main = m4_global_;
#else
    global_main = &m4_global_;
#endif

#ifndef M4_NO_TLS
    if(callback_count && !global_main) {
        global_main = malloc(sizeof(global_main_t));
        assert(global_main);
#ifndef M4_NO_THREAD
        global_main->base.is_main = true;
#endif
        global_main->callbacks_used = 0;
#ifndef M4_NO_THREAD
        m4_global_ = (global_t *) global_main;
#else
        m4_global_ = global_main;
#endif
    }
#endif

    assert(!callback_count || global_main->callbacks_used + callback_count <= M4_MAX_CALLBACKS);
    for(int i = 0; i < callback_count; i++) {
        global_main->ctxs[global_main->callbacks_used++] = ctx;
    }
}

void m4_global_cleanup(void)
{
#ifndef M4_NO_TLS
    free(m4_global_);
    m4_global_ = NULL;
#else
    m4_global_.callbacks_used = 0;
#endif
}

#ifndef M4_NO_THREAD
int m4_engine_thread_create(void * param, m4_stack_t * stack)
{
    int res;

    assert(stack->len >= 3);

    const m4_engine_thread_create_param_t * engine_thread_create_param = param;
    int arg = stack->data[-3];
    int prio_change = stack->data[-2];
    const uint8_t * target = (const uint8_t *) stack->data[-1];

    pthread_attr_t attr;
    res = pthread_attr_init(&attr);
    assert(res == 0);

    size_t default_stack_size;
    res = pthread_attr_getstacksize(&attr, &default_stack_size);
    assert(res == 0);
    int extra_stack_space = engine_thread_create_param->extra_stack_space_get_cb(stack);
    res = pthread_attr_setstacksize(&attr, default_stack_size + extra_stack_space);
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

    m4_engine_thread_entry_arg_t entry_arg;
    entry_arg.stack = stack;
    entry_arg.arg = arg;
    entry_arg.target = target;

    res = sem_init(&entry_arg.done_with_arg_sem, 0, 0);
    assert(res == 0);

    pthread_t thread;
    res = pthread_create(&thread, &attr, engine_thread_create_param->thread_entry, &entry_arg);
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

    return 0;
}
#endif /*ndef M4_NO_THREAD*/

int m4_num_encoded_size_from_int(int num) {
    unsigned u = num;

    if(num < 0) u = ~u;

    int size = 0;

    while(u & ~0x3fu) {
        u >>= 7u;
        size++;
    }

    return size + 1;
}

int m4_num_encode(int num, uint8_t * dst) {
    int size = m4_num_encoded_size_from_int(num);
    m4_num_encode_with_size(num, dst, size);
    return size;
}

void m4_num_encode_with_size(int num, uint8_t * dst, int size)
{
    assert(size >= 1 && size <= 5);

    unsigned u = num;

    bool negative = num < 0;

    if(negative) u = ~u;

    while(u & ~0x3fu) { /* while there are bits set above the 6th */
        *(dst++) = (u & 0x7fu) | 0x80u; /* mask lower 7 bits and set the 8th bit */
        u >>= 7u;
        size--;
    }
    while(size-- > 1) {
        *(dst++) = u | 0x80u;
        u = 0;
    }

    if(negative) u |= 0x40u; /* set the sign bit (7th bit) */

    *dst = u; /* the 8th bit is clear */
}

int m4_num_encoded_size_from_encoded(const uint8_t * src) {
    const uint8_t * p = src;
    while(*(p++) & 0x80);
    return p - src;
}

int m4_num_decode(const uint8_t * src, const uint8_t ** new_src) {
    int len = m4_num_encoded_size_from_encoded(src);

    if(new_src) *new_src = src + len;

    unsigned b = src[len - 1];
    bool negative = b & 0x40u;
    unsigned u = b & 0x3fu;
    for(int i=len-2; i >= 0; i--) {
        b = src[i];
        u <<= 7u;
        u |= b & 0x7fu;
    }

    if(negative) u = ~u;

    return u;
}

int m4_bytes_remaining(void * base, void * p, int len)
{
    return len - (p - base);
}

void * m4_align(const void * p)
{
    union {const void * v; uintptr_t u;} vu = {.v=p};
    vu.u += 3u;
    vu.u &= ~3u;
    return (void *) vu.v;
}

int m4_unpack_binary_header(
    const uint8_t * bin,
    uint8_t * memory_start,
    int memory_len,
    const m4_runtime_cb_array_t * const * cb_arrays,
    const char ** missing_runtime_word_dst,
    int m4_max_callbacks,
    int * callback_count_dst,
    int *** variables_dst,
    const m4_runtime_cb_pair_t *** runtime_cbs_dst,
    const uint8_t ** data_start_dst,
    uint8_t ** callback_info_dst,
    const uint8_t *** callback_words_locations_dst,
    const int ** literals_dst,
    const uint8_t ** program_start_dst,
    uint8_t ** memory_used_end_dst
)
{
    const uint8_t * bin_p = bin;
    uint8_t * memory_p = m4_align(memory_start);

    int n_variables = m4_num_decode(bin_p, &bin_p);
    *variables_dst = (int **) memory_p;
    memory_p += n_variables * 4;
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) {
        return M4_OUT_OF_MEMORY_ERROR;
    }

    int n_runtime_words = m4_num_decode(bin_p, &bin_p);
    const char * bin_runtime_word_p = (const char *) bin_p;
    if(n_runtime_words && (!cb_arrays || !(*cb_arrays))) {
        if(missing_runtime_word_dst) *missing_runtime_word_dst = bin_runtime_word_p;
        return M4_RUNTIME_WORD_MISSING_ERROR;
    }
    const m4_runtime_cb_pair_t ** runtime_cbs_p = (const m4_runtime_cb_pair_t **) memory_p;
    if(m4_bytes_remaining(memory_start, runtime_cbs_p, memory_len) < n_runtime_words * 4) {
        return M4_OUT_OF_MEMORY_ERROR;
    }
    *runtime_cbs_dst = runtime_cbs_p;
    for(int i=0; i<n_runtime_words; i++) {
        const m4_runtime_cb_array_t * const * cb_arrays_p = cb_arrays;
        const m4_runtime_cb_array_t * array_p = *cb_arrays_p;
        while(1) {
            while(!array_p->name) {
                array_p = *(++cb_arrays_p);
                if(!array_p) {
                    if(missing_runtime_word_dst) *missing_runtime_word_dst = bin_runtime_word_p;
                    return M4_RUNTIME_WORD_MISSING_ERROR;
                }
            }
            if(0 == strcmp(array_p->name, bin_runtime_word_p)) {
                *(runtime_cbs_p++) = &array_p->cb_pair;
                break;
            }
            array_p++;
        }
        bin_runtime_word_p += strlen(bin_runtime_word_p) + 1;
    }
    memory_p = (uint8_t *) runtime_cbs_p;
    bin_p = (const uint8_t *) bin_runtime_word_p;

    int bin_data_len = m4_num_decode(bin_p, &bin_p);
    *data_start_dst = bin_p;
    bin_p += bin_data_len;

    int n_callbacks = m4_num_decode(bin_p, &bin_p);
    *callback_count_dst = n_callbacks;
    if(n_callbacks > m4_max_callbacks) {
        return M4_TOO_MANY_CALLBACKS_ERROR;
    }
    uint8_t * callback_info = memory_p;
    *callback_info_dst = memory_p;
    memory_p += n_callbacks;
    memory_p = m4_align(memory_p);
    int * callback_word_offsets = (int *) memory_p;
    memory_p += n_callbacks * 4;
    if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) {
        return M4_OUT_OF_MEMORY_ERROR;
    }
    for(int i=0; i<n_callbacks; i++) {
        callback_info[i] = *(bin_p++);
        callback_word_offsets[i] = m4_num_decode(bin_p, &bin_p);
    }

    int literal_count = m4_num_decode(bin_p, &bin_p);

    bin_p = m4_align(bin_p);

    if(literals_dst) *literals_dst = (int *) bin_p;
    bin_p += 4 * literal_count;

    const uint8_t ** callback_word_locations = (const uint8_t **) callback_word_offsets;
    for(int i=0; i<n_callbacks; i++) {
        callback_word_locations[i] = bin_p + callback_word_offsets[i];
    }
    *callback_words_locations_dst = callback_word_locations;

    *program_start_dst = bin_p;

    *memory_used_end_dst = memory_p;

    return 0;
}

int m4_backend_fragment_get_needed_chores(const m4_fragment_t * all_fragments, const int * sequence,
                                          int sequence_len, int sequence_i, m4_backend_get_flags_t get_flags)
{
    const m4_fragment_t * fragment = &all_fragments[sequence[sequence_i]];
    int needs = get_flags(fragment);
    if(sequence_i > 0) {
        const m4_fragment_t * frag_up = &all_fragments[sequence[sequence_i - 1]];
        int frag_up_flags = get_flags(frag_up);
        if(frag_up_flags & M4_BACKEND_FLAG_Z) {
            needs &= ~M4_BACKEND_FLAG_A;
            if(frag_up_flags & M4_BACKEND_FLAG_Y) {
                needs &= ~M4_BACKEND_FLAG_B;
            }
        }
    }
    if(sequence_i + 1 < sequence_len) {
        const m4_fragment_t * frag_down = &all_fragments[sequence[sequence_i + 1]];
        int frag_down_flags = get_flags(frag_down);
        if(frag_down_flags & M4_BACKEND_FLAG_A) {
            needs &= ~M4_BACKEND_FLAG_Z;
            if((frag_down_flags & M4_BACKEND_FLAG_B) && (frag_down_flags & M4_BACKEND_FLAG_CLOB)) {
                needs &= ~M4_BACKEND_FLAG_Y;
            }
        }
    }
    return needs;
}

int m4_lit(void * param, m4_stack_t * stack)
{
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = (int) param;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

int m4_f00(void * param, m4_stack_t * stack)
{
    void (*func)(void) = param;
    func();
    return 0;
}

int m4_f10(void * param, m4_stack_t * stack)
{
    int (*func)(void) = param;
    int y = func();
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = y;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

#define MCP_FORTH_GENERATED_MCP_FORTH_DEFINITION
#include "mcp_forth_generated.h"
#undef MCP_FORTH_GENERATED_MCP_FORTH_DEFINITION
