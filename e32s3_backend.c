#include "mcp_forth.h"
#include "x86-32_backend_generated.h"

static bool asm_num_is_small(int num)
{
    return num >= -128 && num <= 127;
}

static int get_flags(const m4_fragment_t * fragment)
{
    switch (fragment->op) {
        case M4_OPCODE_BUILTIN_WORD:
            return m4_x86_32_backend_table[fragment->param].flags;
        case M4_OPCODE_DEFINED_WORD:
            return 0; /*call (relative lit) ; e8 xx xx xx xx*/
        case M4_OPCODE_RUNTIME_WORD:
            return M4_X86_32_ADD4 | M4_X86_32_MVBA | M4_X86_32_CLOB | M4_X86_32_SUB4 | M4_X86_32_MVAB;
        case M4_OPCODE_PUSH_LITERAL:
            return M4_X86_32_ADD4 | M4_X86_32_MVBA | M4_X86_32_CLOB; /*mov eax, (lit)*/
        case M4_OPCODE_EXIT_WORD:
            return 0;
        case M4_OPCODE_DEFINED_WORD_LOCATION:
            return 0;
        case M4_OPCODE_PUSH_DATA_ADDRESS:
            return M4_X86_32_ADD4 | M4_X86_32_MVBA | M4_X86_32_CLOB;
        case M4_OPCODE_DECLARE_VARIABLE:
            return 0;
        case M4_OPCODE_USE_VARIABLE_OR_CONSTANT:
            return M4_X86_32_ADD4 | M4_X86_32_MVBA | M4_X86_32_CLOB;
        case M4_OPCODE_HALT:
            return 0;
        case M4_OPCODE_BRANCH_LOCATION:
            return 0;
        case M4_OPCODE_BRANCH_IF_ZERO:
            return 0;
        case M4_OPCODE_BRANCH:
            return 0;
        case M4_OPCODE_DO:
            return M4_X86_32_SUB4 | M4_X86_32_MVAB;
        case M4_OPCODE_LOOP:
            return 0;
        case M4_OPCODE_PUSH_OFFSET_ADDRESS:
            return M4_X86_32_ADD4 | M4_X86_32_MVBA | M4_X86_32_CLOB;
        case M4_OPCODE_PUSH_CALLBACK:
            return M4_X86_32_ADD4 | M4_X86_32_MVBA | M4_X86_32_CLOB;
        case M4_OPCODE_DECLARE_CONSTANT:
            return M4_X86_32_SUB4 | M4_X86_32_MVAB;
        default:
            assert(0);
    }
}

static int get_needs(const m4_fragment_t * all_fragments, const int * sequence, int sequence_len, int sequence_i)
{
    const m4_fragment_t * fragment = &all_fragments[sequence[sequence_i]];
    int needs = get_flags(fragment);
    if(sequence_i > 0) {
        const m4_fragment_t * frag_up = &all_fragments[sequence[sequence_i - 1]];
        int frag_up_flags = get_flags(frag_up);
        if(frag_up_flags & M4_X86_32_SUB4) {
            needs &= ~M4_X86_32_ADD4;
            if(frag_up_flags & M4_X86_32_MVAB) {
                needs &= ~M4_X86_32_MVBA;
            }
        }
    }
    if(sequence_i + 1 < sequence_len) {
        const m4_fragment_t * frag_down = &all_fragments[sequence[sequence_i + 1]];
        int frag_down_flags = get_flags(frag_down);
        if(frag_down_flags & M4_X86_32_ADD4) {
            needs &= ~M4_X86_32_SUB4;
            if((frag_down_flags & M4_X86_32_MVBA) && (frag_down_flags & M4_X86_32_CLOB)) {
                needs &= ~M4_X86_32_MVAB;
            }
        }
    }
    return needs;
}

static int fragment_bin_size(const m4_fragment_t * all_fragments, const int * sequence, int sequence_len, int sequence_i) {
    int needs = get_needs(all_fragments, sequence, sequence_len, sequence_i);
    int size;
    const m4_fragment_t * fragment = &all_fragments[sequence[sequence_i]];
    switch(fragment->op) {
        case M4_OPCODE_BUILTIN_WORD:
            size = m4_x86_32_backend_table[fragment->param].len;
            break;
        case M4_OPCODE_DEFINED_WORD:
            size = 5; /*call (relative lit) ; e8 xx xx xx xx*/
            break;
        case M4_OPCODE_RUNTIME_WORD:
            size = 3 + (asm_num_is_small((fragment->param + 1) * 4) ? 3 : 6);
            break;
        case M4_OPCODE_PUSH_LITERAL:
            size = fragment->param ? 5 : 2; /*mov eax, (lit)*/ /* xor eax, eax */
            break;
        case M4_OPCODE_EXIT_WORD:
            size = 1 + (all_fragments[fragment->param].param ? 1 : 0);
            break;
        case M4_OPCODE_DEFINED_WORD_LOCATION:
            size = fragment->param ? 3 : 0;
            break;
        case M4_OPCODE_PUSH_DATA_ADDRESS:
            /* add eax, (lit) is 83 c0 xx for small and 05 xx xx xx xx for large */
            size = 3 + (!fragment->param ? 0 : asm_num_is_small(fragment->param) ? 3 : 5);
            break;
        case M4_OPCODE_DECLARE_VARIABLE:
            size = 9 + (asm_num_is_small(-fragment->param * 4) ? 3 : 6);
            break;
        case M4_OPCODE_USE_VARIABLE_OR_CONSTANT:
            size = asm_num_is_small(-fragment->param * 4) ? 3 : 6;
            break;
        case M4_OPCODE_HALT:
            size = 1;
            break;
        case M4_OPCODE_BRANCH_LOCATION:
            size = 0;
            break;
        case M4_OPCODE_BRANCH_IF_ZERO:
            size = 8 + (asm_num_is_small(all_fragments[fragment->param].position
                                         - all_fragments[sequence[sequence_i + 1]].position)
                        ? 2 : 6);
            break;
        case M4_OPCODE_BRANCH:
            size = asm_num_is_small(all_fragments[fragment->param].position
                                    - all_fragments[sequence[sequence_i + 1]].position)
                   ? 2 : 5;
            break;
        case M4_OPCODE_DO:
            size = 6;
            break;
        case M4_OPCODE_LOOP:
            size = 9 + (asm_num_is_small(all_fragments[fragment->param].position
                                         - (all_fragments[sequence[sequence_i + 1]].position
                                            - 3))
                        ? 2 : 6);
            break;
        case M4_OPCODE_PUSH_OFFSET_ADDRESS: {
            int offset = all_fragments[fragment->param].position
                         - (fragment->position
                            + ((needs & M4_X86_32_ADD4) ? sizeof(m4_x86_32_add4) : 0)
                            + ((needs & M4_X86_32_MVBA) ? sizeof(m4_x86_32_mvba) : 0)
                            + 5);
            size = 6 + (!offset ? 0 : asm_num_is_small(offset) ? 3 : 5);
            break;
        }
        case M4_OPCODE_PUSH_CALLBACK:
            size = 3 + (asm_num_is_small(fragment->param * 4) ? 3 : 6);
            break;
        case M4_OPCODE_DECLARE_CONSTANT:
            size = asm_num_is_small(-fragment->param * 4) ? 3 : 6;
            break;
        default:
            assert(0);
    }
    if(needs & M4_X86_32_ADD4) size += sizeof(m4_x86_32_add4);
    if(needs & M4_X86_32_MVBA) size += sizeof(m4_x86_32_mvba);
    if(needs & M4_X86_32_SUB4) size += sizeof(m4_x86_32_sub4);
    if(needs & M4_X86_32_MVAB) size += sizeof(m4_x86_32_mvab);
    if(sequence_i + 1 < sequence_len) {
        m4_opcode_t next_fragment_op = all_fragments[sequence[sequence_i + 1]].op;
        int next_fragment_misalignment = (fragment->position + size) % 4;
        if(next_fragment_op == M4_OPCODE_DEFINED_WORD_LOCATION) {
            assert(fragment->op == M4_OPCODE_EXIT_WORD);
            size = (size + 3) & ~3;
        }
        else if (next_fragment_op == M4_OPCODE_BRANCH_LOCATION) {
            if(next_fragment_misalignment == 2 || next_fragment_misalignment == 3) {
                size += 2; // nop.n
            }
        }
    }
    return size;
}

static void cpy_and_inc(uint8_t ** dst, const uint8_t * src, int n)
{
    memcpy(*dst, src, n);
    *dst += n;
}

static void ser_le32_and_inc(uint8_t ** dst, int x)
{
    union {int i; unsigned u;} iu = {.i=x};
    uint8_t * d = *dst;
    for(int i = 0; i < 4; i++) {
        *(d++) = iu.u & 0xffu;
        iu.u >>= 8;
    }
    *dst = d;
}

static uint8_t i8_as_byte(int x) {
    union {int8_t i; uint8_t u;} iu = {.i=x};
    return iu.u;
}

static void ser_i8_or_le32_and_inc(uint8_t ** dst, int x)
{
    if(asm_num_is_small(x)) {
        **dst = i8_as_byte(x);
        *dst += 1;
    } else {
        ser_le32_and_inc(dst, x);
    }
}

static void fragment_bin_dump(const m4_fragment_t * all_fragments, const int * sequence, int sequence_len, int sequence_i, uint8_t * dst) {
    int needs = get_needs(all_fragments, sequence, sequence_len, sequence_i);
    if(needs & M4_X86_32_ADD4) cpy_and_inc(&dst, m4_x86_32_add4, sizeof(m4_x86_32_add4));
    if(needs & M4_X86_32_MVBA) cpy_and_inc(&dst, m4_x86_32_mvba, sizeof(m4_x86_32_mvba));
    const m4_fragment_t * fragment = &all_fragments[sequence[sequence_i]];
    switch(fragment->op) {
        case M4_OPCODE_BUILTIN_WORD:
            cpy_and_inc(&dst,
                        &m4_x86_32_backend_code[m4_x86_32_backend_table[fragment->param].pos],
                        m4_x86_32_backend_table[fragment->param].len);
            break;
        case M4_OPCODE_DEFINED_WORD:
            *(dst++) = 0xe8; /* call <label> */
            ser_le32_and_inc(&dst, all_fragments[fragment->param].position
                                   - all_fragments[sequence[sequence_i + 1]].position);
            break;
        case M4_OPCODE_RUNTIME_WORD: {
            int offset = (fragment->param + 1) * 4;
            *(dst++) = 0x8b; /* mov ecx, [edi+...] */
            *(dst++) = asm_num_is_small(offset) ? 0x4f : 0x8f;
            ser_i8_or_le32_and_inc(&dst, offset);
            *(dst++) = 0xff; /* call [esi+36] */
            *(dst++) = 0x56;
            *(dst++) = 0x24;
            break;
        }
        case M4_OPCODE_PUSH_LITERAL:
            if(fragment->param) { /* mov eax, ... */
                *(dst++) = 0xb8;
                ser_le32_and_inc(&dst, fragment->param);
            } else { /* xor eax, eax*/
                *(dst++) = 0x31;
                *(dst++) = 0xc0;
            }
            break;
        case M4_OPCODE_EXIT_WORD:
            if(all_fragments[fragment->param].param) {
                *(dst++) = 0xc9; /* leave */
            }
            *(dst++) = 0xc3; /* ret */
            break;
        case M4_OPCODE_DEFINED_WORD_LOCATION:
            if(fragment->param) {
                *(dst++) = 0x55; /* push ebp */
                *(dst++) = 0x89; /* mov ebp, esp */
                *(dst++) = 0xe5;
            }
            break;
        case M4_OPCODE_PUSH_DATA_ADDRESS:
            *(dst++) = 0x8b; /* mov eax, [esi+16] */
            *(dst++) = 0x46;
            *(dst++) = 0x10;
            if(fragment->param) {
                /* add eax, ... */
                if(asm_num_is_small(fragment->param)) {
                    *(dst++) = 0x83;
                    *(dst++) = 0xc0;
                } else {
                    *(dst++) = 0x05;
                }
                ser_i8_or_le32_and_inc(&dst, fragment->param);
            }
            break;
        case M4_OPCODE_DECLARE_VARIABLE: {
            static const uint8_t mach_code_1[] = {
                0x83, 0xc2, 0x03,  /* add edx, 3  ; align data-space pointer  */
                0x83, 0xe2, 0xfc,  /* and edx, ~3                             */
            };
            cpy_and_inc(&dst, mach_code_1, sizeof(mach_code_1));

            int offset = -fragment->param * 4;
            *(dst++) = 0x89; /* mov [edi+...], edx */
            *(dst++) = asm_num_is_small(offset) ? 0x57 : 0x97;
            ser_i8_or_le32_and_inc(&dst, offset);

            static const uint8_t mach_code_2[] = {
                0x83, 0xc2, 0x04,  /* add edx, 4  */
            };
            cpy_and_inc(&dst, mach_code_2, sizeof(mach_code_2));
            break;
        }
        case M4_OPCODE_USE_VARIABLE_OR_CONSTANT: {
            int offset = -fragment->param * 4;
            *(dst++) = 0x8b; /* mov eax, [edi+...] */
            *(dst++) = asm_num_is_small(offset) ? 0x47 : 0x87;
            ser_i8_or_le32_and_inc(&dst, offset);
            break;
        }
        case M4_OPCODE_HALT:
            *(dst++) = 0xc3; /* ret */
            break;
        case M4_OPCODE_BRANCH_LOCATION:
            break;
        case M4_OPCODE_BRANCH_IF_ZERO: {
            static const uint8_t mach_code[] = {
                0x83, 0xeb, 0x04,  /* sub ebx, 4        */
                0x85, 0xc0,        /* test eax, eax     */
                0x8b, 0x43, 0x04,  /* mov eax, [ebx+4]  */
            };
            cpy_and_inc(&dst, mach_code, sizeof(mach_code));

            int offset = all_fragments[fragment->param].position
                         - all_fragments[sequence[sequence_i + 1]].position;
            /* jz ... */
            if(asm_num_is_small(offset)) {
                *(dst++) = 0x74;
            } else {
                *(dst++) = 0x0f;
                *(dst++) = 0x84;
            }
            ser_i8_or_le32_and_inc(&dst, offset);
            break;
        }
        case M4_OPCODE_BRANCH: {
            int offset = all_fragments[fragment->param].position
                         - all_fragments[sequence[sequence_i + 1]].position;
            /* jmp ... */
            *(dst++) = asm_num_is_small(offset) ? 0xeb : 0xe9;
            ser_i8_or_le32_and_inc(&dst, offset);
            break;
        }
        case M4_OPCODE_DO: {
            static const uint8_t mach_code[] = {
                0xff, 0x33,        /* push dword [ebx]  */
                0x50,              /* push eax          */
                0x83, 0xeb, 0x04,  /* sub ebx, 4        */
                                   /* ; mov eax, [ebx]  */
                                   /* ; sub ebx, 4      */
            };
            cpy_and_inc(&dst, mach_code, sizeof(mach_code));
            break;
        }
        case M4_OPCODE_LOOP: {
            static const uint8_t mach_code_1[] = {
                0x59,              /* pop ecx         */
                0x41,              /* inc ecx         */
                0x3b, 0x0c, 0x24,  /* cmp ecx, [esp]  */
                0x51,              /* push ecx        */
            };
            cpy_and_inc(&dst, mach_code_1, sizeof(mach_code_1));

            int offset = all_fragments[fragment->param].position
                         - (all_fragments[sequence[sequence_i + 1]].position
                            - 3);
            /* jl ... */
            if(asm_num_is_small(offset)) {
                *(dst++) = 0x7c;
            } else {
                *(dst++) = 0x0f;
                *(dst++) = 0x8c;
            }
            ser_i8_or_le32_and_inc(&dst, offset);

            *(dst++) = 0x83; /* add esp, 8 */
            *(dst++) = 0xc4;
            *(dst++) = 0x08;
            break;
        }
        case M4_OPCODE_PUSH_OFFSET_ADDRESS: {
            static const uint8_t mach_code[] = {
                                               /* ; add ebx, 4      */
                                               /* ; mov [ebx], eax  */
                0xe8, 0x00, 0x00, 0x00, 0x00,  /* call .L0          */
                                               /* .L0:              */
                0x58,                          /* pop eax           */
            };
            cpy_and_inc(&dst, mach_code, sizeof(mach_code));

            int offset = all_fragments[fragment->param].position
                         - (fragment->position
                            + ((needs & M4_X86_32_ADD4) ? sizeof(m4_x86_32_add4) : 0)
                            + ((needs & M4_X86_32_MVBA) ? sizeof(m4_x86_32_mvba) : 0)
                            + 5);
            if(offset) {
                /* add eax, ... */
                if(asm_num_is_small(offset)) {
                    *(dst++) = 0x83;
                    *(dst++) = 0xc0;
                } else {
                    *(dst++) = 0x05;
                }
                ser_i8_or_le32_and_inc(&dst, offset);
            }
            break;
        }
        case M4_OPCODE_PUSH_CALLBACK: {
            int offset = fragment->param * 4;
            *(dst++) = 0x8b; /* mov eax, [esi+40] */
            *(dst++) = 0x46;
            *(dst++) = 0x28;
            *(dst++) = 0x8b; /* mov eax, [eax+...] */
            *(dst++) = asm_num_is_small(offset) ? 0x40 : 0x80;
            ser_i8_or_le32_and_inc(&dst, offset);
            break;
        }
        case M4_OPCODE_DECLARE_CONSTANT: {
            int offset = -fragment->param * 4;
            *(dst++) = 0x89; /* mov [edi+...], eax */
            *(dst++) = asm_num_is_small(offset) ? 0x47 : 0x87;
            ser_i8_or_le32_and_inc(&dst, offset);
            break;
        }
        default:
            assert(0);
    }
    if(needs & M4_X86_32_MVAB) cpy_and_inc(&dst, m4_x86_32_mvab, sizeof(m4_x86_32_mvab));
    if(needs & M4_X86_32_SUB4) cpy_and_inc(&dst, m4_x86_32_sub4, sizeof(m4_x86_32_sub4));
}

const m4_backend_t m4_x86_32_backend = {
    .fragment_bin_size=fragment_bin_size,
    .fragment_bin_dump=fragment_bin_dump
};
