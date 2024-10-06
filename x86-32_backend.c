#include "mcp_forth.h"

#define EB_HELPER(lit) do { \
    if(dst) memcpy(dst, lit, sizeof(lit) - 1); \
    return sizeof(lit) - 1; \
} while(0);

typedef struct x86_fragment_t {
    fragment_t base;
    opcode_t op;
    union {
        int i;
        struct x86_fragment_t * frag;
    } param;
} x86_fragment_t;

static int encode_builtin(uint8_t * dst, int builtin_i)
{
    switch(builtin_i) {
        case 0: /* dup */
            EB_HELPER("\x83\xc3\x04" /* add ebx, 4                            */
                      "\x89\x03"     /* mov [ebx], eax                        */);
        case 1: /* - */
            EB_HELPER("\x89\xc2"     /* mov edx, eax                          */
                      "\x8b\x03"     /* mov eax, [ebx]                        */
                      "\x29\xd0"     /* sub eax, edx                          */
                      "\x83\xeb\x04" /* sub ebx, 4                            */);
        case 2: /* > */
            EB_HELPER("\x39\x03"     /* cmp [ebx], eax                        */
                      "\x0f\x9e\xc0" /* setng al                              */
                      "\x0f\xb6\xc0" /* movzx eax, al                         */
                      "\x48"         /* dec eax                               */
                      "\x83\xeb\x04" /* sub ebx, 4                            */);
        case 3: /* + */
            EB_HELPER("\x03\x03"     /* add eax, [ebx]                        */
                      "\x83\xeb\x04" /* sub ebx, 4                            */);
        case 4: /* = */
            EB_HELPER("\x39\x03"     /* cmp [ebx], eax                        */
                      "\x0f\x95\xc0" /* setne al                              */
                      "\x0f\xb6\xc0" /* movzx eax, al                         */
                      "\x48"         /* dec eax                               */
                      "\x83\xeb\x04" /* sub ebx, 4                            */);
        case 5: /* over */
            EB_HELPER("\x83\xc3\x04" /* add ebx, 4                            */
                      "\x89\x03"     /* mov [ebx], eax                        */
                      "\x8b\x43\xfc" /* mov eax, [ebx-4]                      */);
        case 6: /* mod */
            EB_HELPER("\x89\xc1"     /* mov  ecx, eax                         */
                      "\x8b\x03"     /* mov  eax, [ebx]                       */
                      "\x99"         /* cdq                                   */
                      "\xf7\xf9"     /* idiv ecx                              */
                      "\x89\xd0"     /* mov  eax, edx                         */
                      "\x83\xeb\x04" /* sub  ebx, 4                           */);
        case 7: /* drop */
            EB_HELPER("\x8b\x03"     /* mov eax, [ebx]                        */
                      "\x83\xeb\x04" /* sub ebx, 4                            */);
        case 8: /* swap */
            EB_HELPER("\x87\x03"     /* xchg [ebx], eax                       */);
        case 9: /* * */
            EB_HELPER("\x0f\xaf\x03" /* imul eax, [ebx]                       */
                      "\x83\xeb\x04" /* sub ebx, 4                            */);
        case 10: /* allot */
            EB_HELPER("\x0f\x0b"     /* ud2 ; TODO                            */);
        case 11: /* 1+ */
            EB_HELPER("\x40"         /* inc eax                               */);
        case 12: /* invert */
            EB_HELPER("\xf7\xd0"     /* not eax                               */);
        case 13: /* 0= */
            EB_HELPER("\x83\xf8\x01" /* cmp eax, 1   ; from C dissasembly of  */
                      "\x19\xc0"     /* sbb eax, eax ; x == 0 ? -1 : 0        */);
        case 14: /* i */
            EB_HELPER("\x0f\x0b"     /* ud2 ; TODO                            */);
        case 15: /* j */
            EB_HELPER("\x0f\x0b"     /* ud2 ; TODO                            */);
        case 16: /* 1- */
            EB_HELPER("\x48"         /* dec eax                               */);
        case 17: /* rot */
            EB_HELPER("\x0f\x0b"     /* ud2 ; TODO                            */);
        case 18: /* pick */
            EB_HELPER("\xf7\xd0"     /* not eax                               */
                      "\x8b\x04\x83" /* mov eax, [ebx+eax*4]                  */);
        case 19: /* xor */
            EB_HELPER("\x33\x03"     /* xor eax, [ebx]                        */
                      "\x83\xeb\x04" /* sub ebx, 4                            */);
        case 20: /* lshift */
            EB_HELPER("\x89\xc1"     /* mov ecx, eax                          */
                      "\x8b\x03"     /* mov eax, [ebx]                        */
                      "\xd3\xe0"     /* shl eax, cl                           */
                      "\x83\xeb\x04" /* sub ebx, 4                            */);
        case 21: /* rshift */
            EB_HELPER("\x89\xc1"     /* mov ecx, eax                          */
                      "\x8b\x03"     /* mov eax, [ebx]                        */
                      "\xd3\xe8"     /* shr eax, cl                           */
                      "\x83\xeb\x04" /* sub ebx, 4                            */);
        default:
            assert(0);
    }
}

static fragment_t * create_fragment(opcode_t op, void * param) {
    x86_fragment_t * x86_fragment = malloc(sizeof(x86_fragment_t));
    x86_fragment->op = op;
    switch (op) {
        case OPCODE_BUILTIN_WORD:
        case OPCODE_RUNTIME_WORD:
        case OPCODE_PUSH_LITERAL:
        case OPCODE_PUSH_DATA_ADDRESS:
        case OPCODE_DECLARE_VARIABLE:
        case OPCODE_USE_VARIABLE:
            x86_fragment->param.i = *((int *) param);
            break;
        case OPCODE_DEFINED_WORD:
        case OPCODE_BRANCH_IF_ZERO:
        case OPCODE_BRANCH:
        case OPCODE_LOOP:
            x86_fragment->param.frag = param;
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
    return &x86_fragment->base;
}

static void destroy_fragment(fragment_t * fragment) {
    free(fragment);
}

static int fragment_bin_size(fragment_t * fragment) {
    x86_fragment_t * x86_fragment = (x86_fragment_t *) fragment;
    switch (x86_fragment->op) {
        case OPCODE_BUILTIN_WORD:
            return encode_builtin(NULL, x86_fragment->param.i);
        case OPCODE_RUNTIME_WORD:
        case OPCODE_PUSH_LITERAL:
        case OPCODE_PUSH_DATA_ADDRESS:
        case OPCODE_DECLARE_VARIABLE:
        case OPCODE_USE_VARIABLE:
            return num_encoded_size_from_int(x86_fragment->op)
                   + num_encoded_size_from_int(x86_fragment->param.i);
        case OPCODE_DEFINED_WORD:
        case OPCODE_BRANCH_IF_ZERO:
        case OPCODE_BRANCH:
        case OPCODE_LOOP:
            return num_encoded_size_from_int(x86_fragment->op)
                   + num_encoded_size_from_int(x86_fragment->param.frag->base.position
                                               - x86_fragment->base.position);
        case OPCODE_EXIT_WORD:
        case OPCODE_HALT:
        case OPCODE_DO:
            return num_encoded_size_from_int(x86_fragment->op);
        case OPCODE_DEFINED_WORD_LOCATION:
        case OPCODE_BRANCH_LOCATION:
            return 0;
        default:
            assert(0);
    };
}

static void fragment_bin_get(fragment_t *fragment, uint8_t * dst) {
    x86_fragment_t * x86_fragment = (x86_fragment_t *) fragment;
    switch (x86_fragment->op) {
        case OPCODE_BUILTIN_WORD:
            if(x86_fragment->param.i <= NUM_MAX_ONE_BYTE - OPCODE_LAST_) {
                *dst = x86_fragment->param.i + OPCODE_LAST_;
                break;
            }
            /* fall through */
        case OPCODE_RUNTIME_WORD:
        case OPCODE_PUSH_LITERAL:
        case OPCODE_PUSH_DATA_ADDRESS:
        case OPCODE_DECLARE_VARIABLE:
        case OPCODE_USE_VARIABLE:
            num_encode(x86_fragment->op, dst);
            dst += num_encoded_size_from_int(x86_fragment->op);
            num_encode(x86_fragment->param.i, dst);
            break;
        case OPCODE_DEFINED_WORD:
        case OPCODE_BRANCH_IF_ZERO:
        case OPCODE_BRANCH:
        case OPCODE_LOOP:
            num_encode(x86_fragment->op, dst);
            dst += num_encoded_size_from_int(x86_fragment->op);
            num_encode(x86_fragment->param.frag->base.position
                       - x86_fragment->base.position, dst);
            break;
        case OPCODE_EXIT_WORD:
        case OPCODE_HALT:
        case OPCODE_DO:
            num_encode(x86_fragment->op, dst);
            break;
        case OPCODE_DEFINED_WORD_LOCATION:
        case OPCODE_BRANCH_LOCATION:
            break;
        default:
            assert(0);
    };
}

const mcp_forth_backend_t x86_32_backend = {
    .create_fragment=create_fragment,
    .destroy_fragment=destroy_fragment,
    .fragment_bin_size=fragment_bin_size,
    .fragment_bin_get=fragment_bin_get
};
