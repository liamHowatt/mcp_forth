#include "mcp_forth.h"

static int fragment_bin_size(m4_compiler_ctx_t * compiler, const m4_fragment_t * all_fragments,
                             const int * sequence, int sequence_len, int sequence_i) {
    const m4_fragment_t * fragment = &all_fragments[sequence[sequence_i]];
    switch (fragment->op) {
        case M4_OPCODE_BUILTIN_WORD:
            if(fragment->param <= M4_NUM_MAX_ONE_BYTE - M4_OPCODE_LAST_) {
                return 1;
            }
            /* fall through */
        case M4_OPCODE_RUNTIME_WORD:
        case M4_OPCODE_PUSH_LITERAL:
        case M4_OPCODE_PUSH_DATA_ADDRESS:
        case M4_OPCODE_DECLARE_VARIABLE:
        case M4_OPCODE_USE_VARIABLE_OR_CONSTANT:
        case M4_OPCODE_PUSH_CALLBACK:
        case M4_OPCODE_DECLARE_CONSTANT:
            return m4_num_encoded_size_from_int(fragment->op)
                   + m4_num_encoded_size_from_int(fragment->param);
        case M4_OPCODE_DEFINED_WORD:
        case M4_OPCODE_BRANCH_IF_ZERO:
        case M4_OPCODE_BRANCH:
        case M4_OPCODE_LOOP:
        case M4_OPCODE_PUSH_OFFSET_ADDRESS:
            return m4_num_encoded_size_from_int(fragment->op)
                   + fragment->relaxation + 1;
        case M4_OPCODE_EXIT_WORD:
        case M4_OPCODE_HALT:
        case M4_OPCODE_DO:
        case M4_OPCODE_EXECUTE:
        case M4_OPCODE_THREAD_CREATE:
        case M4_OPCODE_THREAD_JOIN:
            return m4_num_encoded_size_from_int(fragment->op);
        case M4_OPCODE_DEFINED_WORD_LOCATION:
        case M4_OPCODE_BRANCH_LOCATION:
        case M4_OPCODE_ENTRY:
            return 0;
        default:
            assert(0);
    };
}

static bool fragment_bin_is_representable(m4_compiler_ctx_t * compiler, const m4_fragment_t * all_fragments,
                                          const int * sequence, int sequence_len, int sequence_i)
{
    const m4_fragment_t * fragment = &all_fragments[sequence[sequence_i]];
    switch(fragment->op) {
        case M4_OPCODE_DEFINED_WORD:
        case M4_OPCODE_BRANCH_IF_ZERO:
        case M4_OPCODE_BRANCH:
        case M4_OPCODE_LOOP:
        case M4_OPCODE_PUSH_OFFSET_ADDRESS:
            return m4_num_encoded_size_from_int(all_fragments[fragment->param].position
                                                - fragment->position)
                   <= fragment->relaxation + 1;
        default:
            return true;
    }
    assert(0);
}

static void fragment_bin_dump(m4_compiler_ctx_t * compiler, const m4_fragment_t * all_fragments,
                              const int * sequence, int sequence_len, int sequence_i, uint8_t * dst) {
    const m4_fragment_t * fragment = &all_fragments[sequence[sequence_i]];
    switch (fragment->op) {
        case M4_OPCODE_BUILTIN_WORD:
            if(fragment->param <= M4_NUM_MAX_ONE_BYTE - M4_OPCODE_LAST_) {
                *dst = fragment->param + M4_OPCODE_LAST_;
                break;
            }
            /* fall through */
        case M4_OPCODE_RUNTIME_WORD:
        case M4_OPCODE_PUSH_LITERAL:
        case M4_OPCODE_PUSH_DATA_ADDRESS:
        case M4_OPCODE_DECLARE_VARIABLE:
        case M4_OPCODE_USE_VARIABLE_OR_CONSTANT:
        case M4_OPCODE_PUSH_CALLBACK:
        case M4_OPCODE_DECLARE_CONSTANT:
            dst += m4_num_encode(fragment->op, dst);
            m4_num_encode(fragment->param, dst);
            break;
        case M4_OPCODE_DEFINED_WORD:
        case M4_OPCODE_BRANCH_IF_ZERO:
        case M4_OPCODE_BRANCH:
        case M4_OPCODE_LOOP:
        case M4_OPCODE_PUSH_OFFSET_ADDRESS:
            dst += m4_num_encode(fragment->op, dst);
            m4_num_encode_with_size(all_fragments[fragment->param].position
                                    - fragment->position, dst, fragment->relaxation + 1);
            break;
        case M4_OPCODE_EXIT_WORD:
        case M4_OPCODE_HALT:
        case M4_OPCODE_DO:
        case M4_OPCODE_EXECUTE:
        case M4_OPCODE_THREAD_CREATE:
        case M4_OPCODE_THREAD_JOIN:
            m4_num_encode(fragment->op, dst);
            break;
        case M4_OPCODE_DEFINED_WORD_LOCATION:
        case M4_OPCODE_BRANCH_LOCATION:
        case M4_OPCODE_ENTRY:
            break;
        default:
            assert(0);
    };
}

const m4_backend_t m4_compact_bytecode_vm_backend = {
    .fragment_bin_size=fragment_bin_size,
    .fragment_bin_is_representable=fragment_bin_is_representable,
    .fragment_bin_dump=fragment_bin_dump
};
