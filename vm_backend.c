#include "mcp_forth.h"

typedef struct vm_fragment_t {
    fragment_t base;
    opcode_t op;
    union {
        int i;
        struct vm_fragment_t * frag;
    } param;
} vm_fragment_t;

static fragment_t * create_fragment(opcode_t op, void * param) {
    vm_fragment_t * vm_fragment = malloc(sizeof(vm_fragment_t));
    vm_fragment->op = op;
    switch (op) {
        case OPCODE_BUILTIN_WORD:
        case OPCODE_RUNTIME_WORD:
        case OPCODE_PUSH_LITERAL:
        case OPCODE_PUSH_DATA_ADDRESS:
        case OPCODE_DECLARE_VARIABLE:
        case OPCODE_USE_VARIABLE:
            vm_fragment->param.i = *((int *) param);
            break;
        case OPCODE_DEFINED_WORD:
        case OPCODE_BRANCH_IF_ZERO:
        case OPCODE_BRANCH:
        case OPCODE_LOOP:
        case OPCODE_PUSH_OFFSET_ADDRESS:
            vm_fragment->param.frag = param;
            break;
        case OPCODE_EXIT_WORD:
        case OPCODE_DEFINED_WORD_LOCATION:
        case OPCODE_HALT:
        case OPCODE_BRANCH_LOCATION:
        case OPCODE_DO:
            break;
        default:
            assert(0);
    };
    return &vm_fragment->base;
}

static void destroy_fragment(fragment_t * fragment) {
    free(fragment);
}

static int fragment_bin_size(fragment_t * fragment) {
    vm_fragment_t * vm_fragment = (vm_fragment_t *) fragment;
    switch (vm_fragment->op) {
        case OPCODE_BUILTIN_WORD:
            if(vm_fragment->param.i <= NUM_MAX_ONE_BYTE - OPCODE_LAST_) {
                return 1;
            }
            /* fall through */
        case OPCODE_RUNTIME_WORD:
        case OPCODE_PUSH_LITERAL:
        case OPCODE_PUSH_DATA_ADDRESS:
        case OPCODE_DECLARE_VARIABLE:
        case OPCODE_USE_VARIABLE:
            return num_encoded_size_from_int(vm_fragment->op)
                   + num_encoded_size_from_int(vm_fragment->param.i);
        case OPCODE_DEFINED_WORD:
        case OPCODE_BRANCH_IF_ZERO:
        case OPCODE_BRANCH:
        case OPCODE_LOOP:
        case OPCODE_PUSH_OFFSET_ADDRESS:
            return num_encoded_size_from_int(vm_fragment->op)
                   + num_encoded_size_from_int(vm_fragment->param.frag->base.position
                                               - vm_fragment->base.position);
        case OPCODE_EXIT_WORD:
        case OPCODE_HALT:
        case OPCODE_DO:
            return num_encoded_size_from_int(vm_fragment->op);
        case OPCODE_DEFINED_WORD_LOCATION:
        case OPCODE_BRANCH_LOCATION:
            return 0;
        default:
            assert(0);
    };
}

static void fragment_bin_get(fragment_t *fragment, uint8_t * dst) {
    vm_fragment_t * vm_fragment = (vm_fragment_t *) fragment;
    switch (vm_fragment->op) {
        case OPCODE_BUILTIN_WORD:
            if(vm_fragment->param.i <= NUM_MAX_ONE_BYTE - OPCODE_LAST_) {
                *dst = vm_fragment->param.i + OPCODE_LAST_;
                break;
            }
            /* fall through */
        case OPCODE_RUNTIME_WORD:
        case OPCODE_PUSH_LITERAL:
        case OPCODE_PUSH_DATA_ADDRESS:
        case OPCODE_DECLARE_VARIABLE:
        case OPCODE_USE_VARIABLE:
            num_encode(vm_fragment->op, dst);
            dst += num_encoded_size_from_int(vm_fragment->op);
            num_encode(vm_fragment->param.i, dst);
            break;
        case OPCODE_DEFINED_WORD:
        case OPCODE_BRANCH_IF_ZERO:
        case OPCODE_BRANCH:
        case OPCODE_LOOP:
        case OPCODE_PUSH_OFFSET_ADDRESS:
            num_encode(vm_fragment->op, dst);
            dst += num_encoded_size_from_int(vm_fragment->op);
            num_encode(vm_fragment->param.frag->base.position
                       - vm_fragment->base.position, dst);
            break;
        case OPCODE_EXIT_WORD:
        case OPCODE_HALT:
        case OPCODE_DO:
            num_encode(vm_fragment->op, dst);
            break;
        case OPCODE_DEFINED_WORD_LOCATION:
        case OPCODE_BRANCH_LOCATION:
            break;
        default:
            assert(0);
    };
}

const mcp_forth_backend_t compact_bytecode_vm_backend = {
    .create_fragment=create_fragment,
    .destroy_fragment=destroy_fragment,
    .fragment_bin_size=fragment_bin_size,
    .fragment_bin_get=fragment_bin_get
};
