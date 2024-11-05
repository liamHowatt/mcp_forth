#include "mcp_forth.h"

int m4_num_encoded_size_from_int(int num) {
    uint8_t buf[5];
    m4_num_encode(num, buf);
    return m4_num_encoded_size_from_encoded(buf);
}

void m4_num_encode(int num, uint8_t * dst) {
    union {int i; unsigned u;} iu;
    iu.i = num;
    unsigned u = iu.u;

    bool negative = num < 0;

    if(negative) u = ~u;

    while(u & ~0x3fu) { /* while there are bits set above the 6th */
        *(dst++) = (u & 0x7fu) | 0x80u; /* mask lower 7 bits and set the 8th bit */
        u >>= 7u;
    }

    if(negative) u |= 0x40u; /* set the sign bit (7th bit) */

    *dst = u; /* the 8th bit is clear */
}

int m4_num_encoded_size_from_encoded(const uint8_t * src) {
    const uint8_t * p = src;
    while(*(p++) & 0x80);
    return p - src;
}

int m4_num_decode(const uint8_t * src) {
    int len = m4_num_encoded_size_from_encoded(src);
    unsigned b = src[len - 1];
    bool negative = b & 0x40u;
    unsigned u = b & 0x3fu;
    for(int i=len-2; i >= 0; i--) {
        b = src[i];
        u <<= 7u;
        u |= b & 0x7fu;
    }

    if(negative) u = ~u;

    union {int i; unsigned u;} iu;
    iu.u = u;
    return iu.i;
}

int m4_bytes_remaining(void * base, void * p, int len)
{
    return len - (p - base);
}

void * m4_align(void * p)
{
    union {void * v; unsigned u;} vu = {.v=p};
    vu.u += 3u;
    vu.u &= ~3u;
    return vu.v;
}

int m4_unpack_binary_header(
    const uint8_t * bin,
    int bin_len,
    uint8_t * memory_start,
    int memory_len,
    const m4_runtime_cb_array_t ** cb_arrays,
    const char ** missing_runtime_word_dst,
    int max_callbacks,
    int *** variables_dst,
    const m4_runtime_cb_pair_t *** runtime_cbs_dst,
    const uint8_t ** data_start_dst,
    uint8_t ** callback_info_dst,
    const uint8_t *** callback_words_locations_dst,
    const uint8_t ** program_start_dst,
    uint8_t ** memory_used_end_dst
)
{
    const uint8_t * bin_p = bin;
    uint8_t * memory_p = m4_align(memory_start);

    int n_variables = m4_num_decode(bin_p);
    bin_p += m4_num_encoded_size_from_encoded(bin_p);
    if(n_variables) {
        *variables_dst = (int **) memory_p;
        memory_p += n_variables * 4;
        if(m4_bytes_remaining(memory_start, memory_p, memory_len) < 0) {
            return M4_OUT_OF_MEMORY_ERROR;
        }
    }

    int n_runtime_words = m4_num_decode(bin_p);
    bin_p += m4_num_encoded_size_from_encoded(bin_p);
    if(n_runtime_words) {
        const char * bin_runtime_word_p = (const char *) bin_p;
        if(!cb_arrays || !(*cb_arrays)) {
            if(missing_runtime_word_dst) *missing_runtime_word_dst = bin_runtime_word_p;
            return M4_RUNTIME_WORD_MISSING_ERROR;
        }
        const m4_runtime_cb_pair_t ** runtime_cbs_p = (const m4_runtime_cb_pair_t **) memory_p;
        if(m4_bytes_remaining(memory_start, runtime_cbs_p, memory_len) < n_runtime_words * 4) {
            return M4_OUT_OF_MEMORY_ERROR;
        }
        *runtime_cbs_dst = runtime_cbs_p;
        for(int i=0; i<n_runtime_words; i++) {
            const m4_runtime_cb_array_t ** cb_arrays_p = cb_arrays;
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
    }

    int bin_data_len = m4_num_decode(bin_p);
    bin_p += m4_num_encoded_size_from_encoded(bin_p);
    *data_start_dst = bin_p;
    bin_p += bin_data_len;

    int n_callbacks = m4_num_decode(bin_p);
    bin_p += m4_num_encoded_size_from_encoded(bin_p);
    if(n_callbacks) {
        if(n_callbacks > max_callbacks) {
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
            callback_word_offsets[i] = m4_num_decode(bin_p);
            bin_p += m4_num_encoded_size_from_encoded(bin_p);
        }
        const uint8_t ** callback_word_locations = (const uint8_t **) callback_word_offsets;
        for(int i=0; i<n_callbacks; i++) {
            callback_word_locations[i] = bin_p + callback_word_offsets[i];
        }
        *callback_words_locations_dst = callback_word_locations;
    }

    *program_start_dst = bin_p;

    *memory_used_end_dst = memory_p;

    return 0;
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

int m4_f01(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 1)) return M4_STACK_UNDERFLOW_ERROR;
    void (*func)(int) = param;
    stack->data -= 1;
    stack->len -= 1;
    func(stack->data[0]);
    return 0;
}

int m4_f02(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 2)) return M4_STACK_UNDERFLOW_ERROR;
    void (*func)(int, int) = param;
    stack->data -= 2;
    stack->len -= 2;
    func(stack->data[0], stack->data[1]);
    return 0;
}

int m4_f03(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 3)) return M4_STACK_UNDERFLOW_ERROR;
    void (*func)(int, int, int) = param;
    stack->data -= 3;
    stack->len -= 3;
    func(stack->data[0], stack->data[1], stack->data[2]);
    return 0;
}

int m4_f04(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 4)) return M4_STACK_UNDERFLOW_ERROR;
    void (*func)(int, int, int, int) = param;
    stack->data -= 4;
    stack->len -= 4;
    func(stack->data[0], stack->data[1], stack->data[2], stack->data[3]);
    return 0;
}

int m4_f05(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 5)) return M4_STACK_UNDERFLOW_ERROR;
    void (*func)(int, int, int, int, int) = param;
    stack->data -= 5;
    stack->len -= 5;
    func(stack->data[0], stack->data[1], stack->data[2], stack->data[3], stack->data[4]);
    return 0;
}

int m4_f06(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 6)) return M4_STACK_UNDERFLOW_ERROR;
    void (*func)(int, int, int, int, int, int) = param;
    stack->data -= 6;
    stack->len -= 6;
    func(stack->data[0], stack->data[1], stack->data[2], stack->data[3], stack->data[4], stack->data[5]);
    return 0;
}

int m4_f07(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 7)) return M4_STACK_UNDERFLOW_ERROR;
    void (*func)(int, int, int, int, int, int, int) = param;
    stack->data -= 7;
    stack->len -= 7;
    func(stack->data[0], stack->data[1], stack->data[2], stack->data[3], stack->data[4], stack->data[5], stack->data[6]);
    return 0;
}

int m4_f08(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 8)) return M4_STACK_UNDERFLOW_ERROR;
    void (*func)(int, int, int, int, int, int, int, int) = param;
    stack->data -= 8;
    stack->len -= 8;
    func(stack->data[0], stack->data[1], stack->data[2], stack->data[3], stack->data[4], stack->data[5], stack->data[6], stack->data[7]);
    return 0;
}

int m4_f09(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 9)) return M4_STACK_UNDERFLOW_ERROR;
    void (*func)(int, int, int, int, int, int, int, int, int) = param;
    stack->data -= 9;
    stack->len -= 9;
    func(stack->data[0], stack->data[1], stack->data[2], stack->data[3], stack->data[4], stack->data[5], stack->data[6], stack->data[7], stack->data[8]);
    return 0;
}

int m4_f010(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 10)) return M4_STACK_UNDERFLOW_ERROR;
    void (*func)(int, int, int, int, int, int, int, int, int, int) = param;
    stack->data -= 10;
    stack->len -= 10;
    func(stack->data[0], stack->data[1], stack->data[2], stack->data[3], stack->data[4], stack->data[5], stack->data[6], stack->data[7], stack->data[8], stack->data[9]);
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

int m4_f11(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 1)) return M4_STACK_UNDERFLOW_ERROR;
    int (*func)(int) = param;
    stack->data -= 1;
    stack->len -= 1;
    int y = func(stack->data[0]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = y;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

int m4_f12(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 2)) return M4_STACK_UNDERFLOW_ERROR;
    int (*func)(int, int) = param;
    stack->data -= 2;
    stack->len -= 2;
    int y = func(stack->data[0], stack->data[1]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = y;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

int m4_f13(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 3)) return M4_STACK_UNDERFLOW_ERROR;
    int (*func)(int, int, int) = param;
    stack->data -= 3;
    stack->len -= 3;
    int y = func(stack->data[0], stack->data[1], stack->data[2]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = y;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

int m4_f14(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 4)) return M4_STACK_UNDERFLOW_ERROR;
    int (*func)(int, int, int, int) = param;
    stack->data -= 4;
    stack->len -= 4;
    int y = func(stack->data[0], stack->data[1], stack->data[2], stack->data[3]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = y;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

int m4_f15(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 5)) return M4_STACK_UNDERFLOW_ERROR;
    int (*func)(int, int, int, int, int) = param;
    stack->data -= 5;
    stack->len -= 5;
    int y = func(stack->data[0], stack->data[1], stack->data[2], stack->data[3], stack->data[4]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = y;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

int m4_f16(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 6)) return M4_STACK_UNDERFLOW_ERROR;
    int (*func)(int, int, int, int, int, int) = param;
    stack->data -= 6;
    stack->len -= 6;
    int y = func(stack->data[0], stack->data[1], stack->data[2], stack->data[3], stack->data[4], stack->data[5]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = y;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

int m4_f17(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 7)) return M4_STACK_UNDERFLOW_ERROR;
    int (*func)(int, int, int, int, int, int, int) = param;
    stack->data -= 7;
    stack->len -= 7;
    int y = func(stack->data[0], stack->data[1], stack->data[2], stack->data[3], stack->data[4], stack->data[5], stack->data[6]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = y;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

int m4_f18(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 8)) return M4_STACK_UNDERFLOW_ERROR;
    int (*func)(int, int, int, int, int, int, int, int) = param;
    stack->data -= 8;
    stack->len -= 8;
    int y = func(stack->data[0], stack->data[1], stack->data[2], stack->data[3], stack->data[4], stack->data[5], stack->data[6], stack->data[7]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = y;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

int m4_f19(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 9)) return M4_STACK_UNDERFLOW_ERROR;
    int (*func)(int, int, int, int, int, int, int, int, int) = param;
    stack->data -= 9;
    stack->len -= 9;
    int y = func(stack->data[0], stack->data[1], stack->data[2], stack->data[3], stack->data[4], stack->data[5], stack->data[6], stack->data[7], stack->data[8]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = y;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

int m4_f110(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 10)) return M4_STACK_UNDERFLOW_ERROR;
    int (*func)(int, int, int, int, int, int, int, int, int, int) = param;
    stack->data -= 10;
    stack->len -= 10;
    int y = func(stack->data[0], stack->data[1], stack->data[2], stack->data[3], stack->data[4], stack->data[5], stack->data[6], stack->data[7], stack->data[8], stack->data[9]);
    if(!(stack->len < stack->max)) return M4_STACK_OVERFLOW_ERROR;
    stack->data[0] = y;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

int m4_f0x(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 1)) return M4_STACK_UNDERFLOW_ERROR;
    stack->data -= 1;
    stack->len -= 1;
    switch (stack->data[0]) {
        case 0:
            return m4_f00(param, stack);
        case 1:
            return m4_f01(param, stack);
        case 2:
            return m4_f02(param, stack);
        case 3:
            return m4_f03(param, stack);
        case 4:
            return m4_f04(param, stack);
        case 5:
            return m4_f05(param, stack);
        case 6:
            return m4_f06(param, stack);
        case 7:
            return m4_f07(param, stack);
        case 8:
            return m4_f08(param, stack);
        case 9:
            return m4_f09(param, stack);
        case 10:
            return m4_f010(param, stack);
    }
    return M4_TOO_MANY_VAR_ARGS_ERROR;
}

int m4_f1x(void * param, m4_stack_t * stack)
{
    if(!(stack->len >= 1)) return M4_STACK_UNDERFLOW_ERROR;
    stack->data -= 1;
    stack->len -= 1;
    switch (stack->data[0]) {
        case 0:
            return m4_f10(param, stack);
        case 1:
            return m4_f11(param, stack);
        case 2:
            return m4_f12(param, stack);
        case 3:
            return m4_f13(param, stack);
        case 4:
            return m4_f14(param, stack);
        case 5:
            return m4_f15(param, stack);
        case 6:
            return m4_f16(param, stack);
        case 7:
            return m4_f17(param, stack);
        case 8:
            return m4_f18(param, stack);
        case 9:
            return m4_f19(param, stack);
        case 10:
            return m4_f110(param, stack);
    }
    return M4_TOO_MANY_VAR_ARGS_ERROR;
}
