#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#define RUNTIME_WORD_MISSING_ERROR 1000

#define STACK_UNDERFLOW_ERROR           1
#define STACK_OVERFLOW_ERROR            2
#define RETURN_STACK_OVERFLOW_ERROR     3
#define OUT_OF_MEMORY_ERROR             4

typedef enum {
    OPCODE_BUILTIN_WORD,
    OPCODE_DEFINED_WORD,
    OPCODE_RUNTIME_WORD,
    OPCODE_PUSH_LITERAL,
    OPCODE_EXIT_WORD,
    OPCODE_DEFINED_WORD_LOCATION,
    OPCODE_PUSH_DATA_ADDRESS,
    OPCODE_DECLARE_VARIABLE,
    OPCODE_USE_VARIABLE,
    OPCODE_HALT,
    OPCODE_BRANCH_LOCATION,
    OPCODE_BRANCH_IF_ZERO,
    OPCODE_BRANCH,
    OPCODE_DO,
    OPCODE_LOOP,
    OPCODE_PUSH_OFFSET_ADDRESS,
    OPCODE_LAST_
} opcode_t;

typedef struct {
    int position;
    bool is_word_fragment;
} fragment_t;

typedef struct {
    fragment_t * (*create_fragment)(opcode_t op, void * param);
    void (*destroy_fragment)(fragment_t *);
    int (*fragment_bin_size)(fragment_t *);
    void (*fragment_bin_get)(fragment_t *, uint8_t * dst);
} mcp_forth_backend_t;

extern const mcp_forth_backend_t compact_bytecode_vm_backend;

int mcp_forth_compile(
    const char * source,
    int source_len,
    uint8_t ** bin_out,
    const mcp_forth_backend_t * backend,
    int * error_near
);

int num_encoded_size_from_int(int num);
void num_encode(int num, uint8_t * dst);
int num_encoded_size_from_encoded(const uint8_t * src);
int num_decode(const uint8_t * src);
#define NUM_MAX_ONE_BYTE 63

typedef struct {int * data; int max; int len; } stack_t;
typedef int (*runtime_cb)(void * runtime_ctx_t, stack_t * stack);
typedef struct {const char * name; runtime_cb cb;} runtime_cb_array_t;

typedef struct {
    const runtime_cb_array_t * runtime_cb_array;
} runtime_t;

typedef struct {
    int (*run)(
        const uint8_t * program_start,
        const uint8_t * data_start,
        runtime_cb * runtime_cbs,
        void * runtime_ctx,
        int variable_count
    );
    int (*call_defined_word)(
        const uint8_t * defined_word_ptr,
        stack_t * stack
    );
} mcp_forth_engine_t;

extern const mcp_forth_engine_t compact_bytecode_vm_engine;

int mcp_forth_execute(
    const uint8_t * bin,
    int bin_len,
    const runtime_t * runtime,
    void * runtime_ctx,
    const mcp_forth_engine_t * engine,
    const char ** missing_word
);
