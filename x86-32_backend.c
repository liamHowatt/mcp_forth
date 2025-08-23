/*

; defined word
call 42 ; e8 xx xx xx xx

; runtime word
mov ecx, [edi+(42+1)*4]
call [esi+36]

; push literal
; add ebx, 4
; mov [ebx], eax
mov eax, 42

; exit word
leave  ; sometimes omitted
ret

; defined word location
push ebp       ; sometimes omitted
mov ebp, esp   ; |

; push data address
; add ebx, 4
; mov [ebx], eax
mov eax, [esi+16]
add eax, 42

; declare variable
add edx, 3  ; align data-space pointer
and edx, ~3
mov [edi+(-42)*4], edx
add edx, 4

; use variable or constant
; add ebx, 4
; mov [ebx], eax
mov eax, [edi+(-42)*4]

; halt
ret

; branch location
; (empty)

; branch if zero
sub ebx, 4
test eax, eax
mov eax, [ebx+4]
jz 42 ; '74 42' or '0f 84 42 00 00 00'

; branch
jmp 42 ; 'eb 2a' or 'e9 2a 00 00 00'

; do
push dword [ebx]
push eax
mov  eax, [ebx-4]
sub  ebx, 8
; DO is always followed by
; a branch location which won't
; have any elide flags, so don't
; bother replacing the above 2 lines with
;     sub ebx, 4
;     ; mov eax, [ebx]
;     ; sub ebx, 4

; loop
pop ecx
inc ecx
cmp ecx, [esp]
push ecx
jl 42
add esp, 8

; push offset address
; add ebx, 4
; mov [ebx], eax
call .L0
.L0:
pop eax
add eax, 42

; push callback
; add ebx, 4
; mov [ebx], eax
mov eax, [esi+40]
mov eax, [eax+42]

; declare constant
mov [edi+(-42)*4], eax
; mov eax, [ebx]
; sub ebx, 4

; execute
mov ecx, eax
mov eax, [ebx]
sub ebx, 4
call ecx

; thread create
mov ecx, [esi+44]
call [esi+36]

; thread join
mov ecx, [esi+44]
add ecx, 8
call [esi+36]

; entry
; (empty)
*/

#include "mcp_forth.h"
#include "x86_32_backend_generated.h"

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(*(arr)))

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
            return 0;
        case M4_OPCODE_PUSH_LITERAL:
            return M4_BACKEND_FLAG_A | M4_BACKEND_FLAG_B | M4_BACKEND_FLAG_CLOB; /*mov eax, (lit)*/
        case M4_OPCODE_EXIT_WORD:
            return 0;
        case M4_OPCODE_DEFINED_WORD_LOCATION:
            return 0;
        case M4_OPCODE_PUSH_DATA_ADDRESS:
            return M4_BACKEND_FLAG_A | M4_BACKEND_FLAG_B | M4_BACKEND_FLAG_CLOB;
        case M4_OPCODE_DECLARE_VARIABLE:
            return 0;
        case M4_OPCODE_USE_VARIABLE_OR_CONSTANT:
            return M4_BACKEND_FLAG_A | M4_BACKEND_FLAG_B | M4_BACKEND_FLAG_CLOB;
        case M4_OPCODE_HALT:
            return 0;
        case M4_OPCODE_BRANCH_LOCATION:
            return 0;
        case M4_OPCODE_BRANCH_IF_ZERO:
            return 0;
        case M4_OPCODE_BRANCH:
            return 0;
        case M4_OPCODE_DO:
            return 0;
        case M4_OPCODE_LOOP:
            return 0;
        case M4_OPCODE_PUSH_OFFSET_ADDRESS:
            return M4_BACKEND_FLAG_A | M4_BACKEND_FLAG_B | M4_BACKEND_FLAG_CLOB;
        case M4_OPCODE_PUSH_CALLBACK:
            return M4_BACKEND_FLAG_A | M4_BACKEND_FLAG_B | M4_BACKEND_FLAG_CLOB;
        case M4_OPCODE_DECLARE_CONSTANT:
            return M4_BACKEND_FLAG_Z | M4_BACKEND_FLAG_Y;
        case M4_OPCODE_EXECUTE:
            return 0;
        case M4_OPCODE_THREAD_CREATE:
        case M4_OPCODE_THREAD_JOIN:
            return 0;
        case M4_OPCODE_ENTRY:
            return 0;
        default:
            assert(0);
    }
}

static int fragment_bin_size(m4_compiler_ctx_t * compiler, const m4_fragment_t * all_fragments,
                             const int * sequence, int sequence_len, int sequence_i) {
    int needs = m4_backend_fragment_get_needed_chores(all_fragments, sequence, sequence_len, sequence_i, get_flags);
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
            size = 8 + (fragment->relaxation == 0 ? 2 : 6);
            break;
        case M4_OPCODE_BRANCH:
            size = fragment->relaxation == 0 ? 2 : 5;
            break;
        case M4_OPCODE_DO:
            size = 9;
            break;
        case M4_OPCODE_LOOP:
            size = 9 + (fragment->relaxation == 0 ? 2 : 6);
            break;
        case M4_OPCODE_PUSH_OFFSET_ADDRESS:
            size = 6;
            switch(fragment->relaxation) {
                case 0:
                    break;
                case 1:
                    size += 3;
                    break;
                case 2:
                    size += 5;
                    break;
                default:
                    assert(0);
            }
            break;
        case M4_OPCODE_PUSH_CALLBACK:
            size = 3 + (asm_num_is_small(fragment->param * 4) ? 3 : 6);
            break;
        case M4_OPCODE_DECLARE_CONSTANT:
            size = asm_num_is_small(-fragment->param * 4) ? 3 : 6;
            break;
        case M4_OPCODE_EXECUTE:
            size = 9;
            break;
        case M4_OPCODE_THREAD_CREATE:
            size = 6;
            break;
        case M4_OPCODE_THREAD_JOIN:
            size = 9;
            break;
        case M4_OPCODE_ENTRY:
            size = 0;
            break;
        default:
            assert(0);
    }
    if(needs & M4_BACKEND_FLAG_A) size += sizeof(m4_x86_32_backend_chore_a);
    if(needs & M4_BACKEND_FLAG_B) size += sizeof(m4_x86_32_backend_chore_b);
    if(needs & M4_BACKEND_FLAG_Z) size += sizeof(m4_x86_32_backend_chore_z);
    if(needs & M4_BACKEND_FLAG_Y) size += sizeof(m4_x86_32_backend_chore_y);
    return size;
}

static bool fragment_bin_is_representable(m4_compiler_ctx_t * compiler, const m4_fragment_t * all_fragments,
                                          const int * sequence, int sequence_len, int sequence_i)
{
    const m4_fragment_t * fragment = &all_fragments[sequence[sequence_i]];
    switch(fragment->op) {
        case M4_OPCODE_BRANCH_IF_ZERO:
        case M4_OPCODE_BRANCH:
            if(fragment->relaxation == 0) {
                return asm_num_is_small(all_fragments[fragment->param].position
                                        - all_fragments[sequence[sequence_i + 1]].position);
            }
            assert(fragment->relaxation == 1);
            return true;
        case M4_OPCODE_LOOP:
            if(fragment->relaxation == 0) {
                return asm_num_is_small(all_fragments[fragment->param].position
                                        - (all_fragments[sequence[sequence_i + 1]].position
                                           - 3));
            }
            assert(fragment->relaxation == 1);
            return true;
        case M4_OPCODE_PUSH_OFFSET_ADDRESS: {
            int needs = m4_backend_fragment_get_needed_chores(all_fragments, sequence, sequence_len, sequence_i, get_flags);
            int offset = all_fragments[fragment->param].position
                         - (fragment->position
                            + ((needs & M4_BACKEND_FLAG_A) ? sizeof(m4_x86_32_backend_chore_a) : 0)
                            + ((needs & M4_BACKEND_FLAG_B) ? sizeof(m4_x86_32_backend_chore_b) : 0)
                            + 5);
            switch(fragment->relaxation) {
                case 0:
                    return offset == 0;
                case 1:
                    return asm_num_is_small(offset);
                case 2:
                    return true;
            }
            assert(0);
        }
        default:
            return true;
    }
    assert(0);
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

static void ser_i8_and_inc(uint8_t ** dst, int x) {
    **dst = i8_as_byte(x);
    *dst += 1;
}

static void ser_i8_or_le32_and_inc(uint8_t ** dst, int x)
{
    if(asm_num_is_small(x)) {
        ser_i8_and_inc(dst, x);
    } else {
        ser_le32_and_inc(dst, x);
    }
}

static void fragment_bin_dump(m4_compiler_ctx_t * compiler, const m4_fragment_t * all_fragments,
                              const int * sequence, int sequence_len, int sequence_i, uint8_t * dst) {
    int needs = m4_backend_fragment_get_needed_chores(all_fragments, sequence, sequence_len, sequence_i, get_flags);
    if(needs & M4_BACKEND_FLAG_A) cpy_and_inc(&dst, m4_x86_32_backend_chore_a, sizeof(m4_x86_32_backend_chore_a));
    if(needs & M4_BACKEND_FLAG_B) cpy_and_inc(&dst, m4_x86_32_backend_chore_b, sizeof(m4_x86_32_backend_chore_b));
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
            if(fragment->relaxation == 0) {
                *(dst++) = 0x74;
                ser_i8_and_inc(&dst, offset);
            } else {
                *(dst++) = 0x0f;
                *(dst++) = 0x84;
                ser_le32_and_inc(&dst, offset);
            }
            break;
        }
        case M4_OPCODE_BRANCH: {
            int offset = all_fragments[fragment->param].position
                         - all_fragments[sequence[sequence_i + 1]].position;
            /* jmp ... */
            if(fragment->relaxation == 0) {
                *(dst++) = 0xeb;
                ser_i8_and_inc(&dst, offset);
            } else {
                *(dst++) = 0xe9;
                ser_le32_and_inc(&dst, offset);
            }
            break;
        }
        case M4_OPCODE_DO: {
            static const uint8_t mach_code[] = {
                0xff, 0x33,        /* push dword [ebx]  */
                0x50,              /* push eax          */
		0x8b, 0x43, 0xfc,  /* mov eax, [ebx-4]  */
                0x83, 0xeb, 0x08,  /* sub ebx, 8        */
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
            if(fragment->relaxation == 0) {
                *(dst++) = 0x7c;
                ser_i8_and_inc(&dst, offset);
            } else {
                *(dst++) = 0x0f;
                *(dst++) = 0x8c;
                ser_le32_and_inc(&dst, offset);
            }

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
                            + ((needs & M4_BACKEND_FLAG_A) ? sizeof(m4_x86_32_backend_chore_a) : 0)
                            + ((needs & M4_BACKEND_FLAG_B) ? sizeof(m4_x86_32_backend_chore_b) : 0)
                            + 5);
            /* add eax, ... */
            switch(fragment->relaxation) {
                case 0:
                    break;
                case 1:
                    *(dst++) = 0x83;
                    *(dst++) = 0xc0;
                    ser_i8_and_inc(&dst, offset);
                    break;
                case 2:
                    *(dst++) = 0x05;
                    ser_le32_and_inc(&dst, offset);
                    break;
                default:
                    assert(0);
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
        case M4_OPCODE_EXECUTE: {
            static const uint8_t mach_code[] = {
                0x89, 0xC1,        /* mov ecx, eax    */
                0x8B, 0x03,        /* mov eax, [ebx]  */
                0x83, 0xEB, 0x04,  /* sub ebx, 4      */
                0xFF, 0xD1,        /* call ecx        */
            };
            cpy_and_inc(&dst, mach_code, sizeof(mach_code));
            break;
        }
        case M4_OPCODE_THREAD_CREATE: {
            static const uint8_t mach_code[] = {
                0x8b, 0x4e,   44,  /* mov ecx, [esi+44]  */
                0xff, 0x56, 0x24,  /* call [esi+36]      */
            };
            cpy_and_inc(&dst, mach_code, sizeof(mach_code));
            break;
        }
        case M4_OPCODE_THREAD_JOIN: {
            static const uint8_t mach_code[] = {
                0x8b, 0x4e,   44,  /* mov ecx, [esi+44]  */
                0x83, 0xc1, 0x08,  /* add ecx, 8         */
                0xff, 0x56, 0x24,  /* call [esi+36]      */
            };
            cpy_and_inc(&dst, mach_code, sizeof(mach_code));
            break;
        }
        case M4_OPCODE_ENTRY:
            break;
        default:
            assert(0);
    }
    if(needs & M4_BACKEND_FLAG_Y) cpy_and_inc(&dst, m4_x86_32_backend_chore_y, sizeof(m4_x86_32_backend_chore_y));
    if(needs & M4_BACKEND_FLAG_Z) cpy_and_inc(&dst, m4_x86_32_backend_chore_z, sizeof(m4_x86_32_backend_chore_z));
}

const m4_backend_t m4_x86_32_backend = {
    .fragment_bin_size=fragment_bin_size,
    .fragment_bin_is_representable=fragment_bin_is_representable,
    .fragment_bin_dump=fragment_bin_dump
};
