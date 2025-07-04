#pragma once
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#define M4_RUNTIME_WORD_MISSING_ERROR             1
#define M4_STACK_UNDERFLOW_ERROR                  2
#define M4_STACK_OVERFLOW_ERROR                   3
#define M4_THREADS_NOT_ENABLED_ERROR              4
#define M4_OUT_OF_MEMORY_ERROR                    5
#define M4_TOO_MANY_CALLBACKS_ERROR               6
#define M4_TOO_MANY_VAR_ARGS_ERROR                7
#define M4_DATA_SPACE_POINTER_OUT_OF_BOUNDS_ERROR 8
#define M4_THREADS_CANNOT_LAUNCH_PROGRAMS_ERROR   9

#define M4_ALIGN(x) (((x) + 3) & ~3)

typedef enum {
    M4_OPCODE_BUILTIN_WORD,
    M4_OPCODE_DEFINED_WORD,
    M4_OPCODE_RUNTIME_WORD,
    M4_OPCODE_PUSH_LITERAL,
    M4_OPCODE_EXIT_WORD,
    M4_OPCODE_DEFINED_WORD_LOCATION,
    M4_OPCODE_PUSH_DATA_ADDRESS,
    M4_OPCODE_DECLARE_VARIABLE,
    M4_OPCODE_USE_VARIABLE_OR_CONSTANT,
    M4_OPCODE_HALT,
    M4_OPCODE_BRANCH_LOCATION,
    M4_OPCODE_BRANCH_IF_ZERO,
    M4_OPCODE_BRANCH,
    M4_OPCODE_DO,
    M4_OPCODE_LOOP,
    M4_OPCODE_PUSH_OFFSET_ADDRESS,
    M4_OPCODE_PUSH_CALLBACK,
    M4_OPCODE_DECLARE_CONSTANT,
    M4_OPCODE_EXECUTE,
    M4_OPCODE_THREAD_CREATE,
    M4_OPCODE_THREAD_JOIN,
    M4_OPCODE_LAST_
} m4_opcode_t;

typedef enum {
    M4_ARCH_X86_32,
    M4_ARCH_ESP32S3
} m4_arch_t;

typedef struct {
    int param;
    int position;
    uint16_t op;
    bool is_word_fragment;
    uint8_t ext_param;
} m4_fragment_t;

typedef struct {
    int (*fragment_bin_size)(const m4_fragment_t * all_fragments, const int * sequence, int sequence_len, int sequence_i);
    void (*fragment_bin_dump)(const m4_fragment_t * all_fragments, const int * sequence, int sequence_len, int sequence_i, uint8_t * dst);
} m4_backend_t;

int m4_compile(
    const char * source,
    int source_len,
    uint8_t ** bin_out,
    const m4_backend_t * backend,
    int * error_near
);

int m4_num_encoded_size_from_int(int num);
void m4_num_encode(int num, uint8_t * dst);
int m4_num_encoded_size_from_encoded(const uint8_t * src);
int m4_num_decode(const uint8_t * src);
#define M4_NUM_MAX_ONE_BYTE 63

typedef struct {int * data; int max; int len;} m4_stack_t;
typedef int (*m4_runtime_cb)(void * param, m4_stack_t * stack);
typedef struct {m4_runtime_cb cb; void * param;} m4_runtime_cb_pair_t;
typedef struct {const char * name; m4_runtime_cb_pair_t cb_pair;} m4_runtime_cb_array_t;
typedef int (*m4_engine_run_t)(
    const uint8_t * bin,
    int bin_len,
    uint8_t * memory_start,
    int memory_len,
    const m4_runtime_cb_array_t ** cb_arrays,
    const char ** missing_runtime_word_dst
);

int m4_vm_engine_run(
    const uint8_t * bin,
    int bin_len,
    uint8_t * memory_start,
    int memory_len,
    const m4_runtime_cb_array_t ** cb_arrays,
    const char ** missing_runtime_word_dst
);
int m4_x86_32_engine_run(
    const uint8_t * bin,
    int bin_len,
    uint8_t * memory_start,
    int memory_len,
    const m4_runtime_cb_array_t ** cb_arrays,
    const char ** missing_runtime_word_dst
);

void m4_global_cleanup(void);

extern const m4_backend_t m4_compact_bytecode_vm_backend;
extern const m4_backend_t m4_x86_32_backend;
extern const m4_runtime_cb_array_t m4_runtime_lib_io[];
extern const m4_runtime_cb_array_t m4_runtime_lib_time[];
extern const m4_runtime_cb_array_t m4_runtime_lib_string[];
extern const m4_runtime_cb_array_t m4_runtime_lib_process[];
extern const m4_runtime_cb_array_t m4_runtime_lib_file[];
extern const m4_runtime_cb_array_t m4_runtime_lib_assert[];
extern const m4_runtime_cb_array_t m4_runtime_lib_threadutil[];

int m4_bytes_remaining(void * base, void * p, int len);
void * m4_align(const void * p);
int m4_unpack_binary_header(
    const uint8_t * bin,
    int bin_len,
    uint8_t * memory_start,
    int memory_len,
    const m4_runtime_cb_array_t ** cb_arrays,
    const char ** missing_runtime_word_dst,
    int max_callbacks,
    int * callback_count_dst,
    int *** variables_dst,
    const m4_runtime_cb_pair_t *** runtime_cbs_dst,
    const uint8_t ** data_start_dst,
    uint8_t ** callback_info_dst,
    const uint8_t *** callback_words_locations_dst,
    const uint8_t ** program_start_dst,
    uint8_t ** memory_used_end_dst
);

typedef struct {uint32_t bin_len; uint8_t bin[];} m4_elf_content_t;
int m4_elf_linux_size(void);
void m4_elf_linux(void * aligned_elf_dst, m4_arch_t machine, int bin_len);
int m4_elf_nuttx_size(void);
void m4_elf_nuttx(void * aligned_elf_dst, m4_arch_t machine, int bin_len);

int m4_lit(void * param, m4_stack_t * stack);
int m4_f00(void * param, m4_stack_t * stack);
int m4_f01(void * param, m4_stack_t * stack);
int m4_f02(void * param, m4_stack_t * stack);
int m4_f03(void * param, m4_stack_t * stack);
int m4_f04(void * param, m4_stack_t * stack);
int m4_f05(void * param, m4_stack_t * stack);
int m4_f06(void * param, m4_stack_t * stack);
int m4_f07(void * param, m4_stack_t * stack);
int m4_f08(void * param, m4_stack_t * stack);
int m4_f09(void * param, m4_stack_t * stack);
int m4_f010(void * param, m4_stack_t * stack);
int m4_f10(void * param, m4_stack_t * stack);
int m4_f11(void * param, m4_stack_t * stack);
int m4_f12(void * param, m4_stack_t * stack);
int m4_f13(void * param, m4_stack_t * stack);
int m4_f14(void * param, m4_stack_t * stack);
int m4_f15(void * param, m4_stack_t * stack);
int m4_f16(void * param, m4_stack_t * stack);
int m4_f17(void * param, m4_stack_t * stack);
int m4_f18(void * param, m4_stack_t * stack);
int m4_f19(void * param, m4_stack_t * stack);
int m4_f110(void * param, m4_stack_t * stack);
int m4_f0x(void * param, m4_stack_t * stack);
int m4_f1x(void * param, m4_stack_t * stack);
