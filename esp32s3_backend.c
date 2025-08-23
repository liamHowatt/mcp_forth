/*

; defined word
;   align so that the following fragment addr mod 4 is 0 or 1
call0 42    ; signed 20 bits

; runtime word
;   align so that the following fragment addr mod 4 is 0 or 1
movi.n  s0, 42
callx0  rc

; push literal
; add ebx, 4
; mov [ebx], eax
movi.n  dt, 42

; exit word
; if using loops
mov.n   rs, rb
l32i.n  rb, rs, rso+4
ret.n
; if calling a function
add.n   rs, rs, n4
l32i.n  lr, rs, rso+4
ret.n
; if using loops and calling a function
mov.n   rs, rb
l32i.n  rb, rs, rso+4
l32i.n  lr, rs, rso+8
ret.n

; defined word location
;   align 32 bit
; if using loops
s32i.n  rb, rs, rso+4
mov.n   rb, rs
addi.n  rs, rs, 4
; if calling a function
s32i.n  lr, rs, rso+4
addi.n  rs, rs, 4
; if using loops and calling a function
s32i.n  rb, rs, rso+4
s32i.n  lr, rs, rso+8
mov.n   rb, rs
addi.n  rs, rs, 8

; push data address
; add ebx, 4
; mov [ebx], eax
l32i.n  dt, cs, DATA
addi.n  dt, dt, 42

; declare variable
addi.n  dp, dp, 3
and     dp, dp, n4
s32i.n  dp, va, 42*4
addi.n  dp, dp, 4

; use variable or constant
; add ebx, 4
; mov [ebx], eax
l32i.n  dt, va, 42*4

; halt
l32i.n  lr, sp, 4
ret.n

; branch location
;   align so that addr mod 4 is 0 or 1
; (empty)

; branch if zero
mov.n   s0, dt
l32i.n  dt, ds, dso
add.n   ds, ds, n4
beqz.n  s0, 42

; branch
j 42

; do
l32i.n  s0, ds, dso
addi.n  rs, rs, 8
s32i.n  s0, rs, rso-4
s32i.n  dt, rs, rso
l32i.n  dt, ds, dso-4
addi    ds, ds, -8
; DO is always followed by
; a branch location which won't
; have any elide flags, so don't
; bother replacing the above 2 lines with
;     add.n   ds, ds, n4
;     ; mov eax, [ebx]
;     ; sub ebx, 4

; loop
l32i.n  s0, rs, rso
addi.n  s0, s0, 1
s32i.n  s0, rs, rso
l32i.n  s1, rs, rso-4
blt     s0, s1, 42
addi    rs, rs, -8

; push offset address
; add ebx, 4
; mov [ebx], eax
l32i.n  dt, cs, BIN_START
addi.n  dt, dt, 42

; push callback
; add ebx, 4
; mov [ebx], eax
l32i.n  dt, cs, CALLBACK_TARGETS
l32i.n  dt, dt, 42

; declare constant
s32i.n  dt, va, 42*4
; mov eax, [ebx]
; sub ebx, 4

; execute
;   align so that the following fragment addr mod 4 is 0 or 1
mov.n  s0, dt
; mov eax, [ebx]
; sub ebx, 4
callx0 s0

; thread create
;   align so that the following fragment addr mod 4 is 0 or 1
movi.n  s0, -1
callx0  rc

; thread join
;   align so that the following fragment addr mod 4 is 0 or 1
movi.n  s0, -2
callx0  rc

; entry
s32i.n  lr, sp, 4

*/

#include "mcp_forth.h"
#include "esp32s3_backend_generated.h"
#include "esp32s3.h"

static int get_flags(const m4_fragment_t * fragment)
{
    switch (fragment->op) {
        case M4_OPCODE_BUILTIN_WORD:
            return m4_esp32s3_backend_table[fragment->param].flags;
        case M4_OPCODE_DEFINED_WORD:
            return 0;
        case M4_OPCODE_RUNTIME_WORD:
            return 0;
        case M4_OPCODE_PUSH_LITERAL:
            return M4_BACKEND_FLAG_A | M4_BACKEND_FLAG_B | M4_BACKEND_FLAG_CLOB;
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

static void l32i_n(uint8_t ** dst, int regdst, int regptr, int offset)
{
    assert((regdst & ~0xf) == 0);
    assert((regptr & ~0xf) == 0);
    assert((offset & ~0b111100) == 0);
    (*dst)[0] = (regdst << 4) | 0b1000;
    (*dst)[1] = (offset << 2) | regptr;
    *dst += 2;
}

static void s32i_n(uint8_t ** dst, int regsrc, int regptr, int offset)
{
    assert((regsrc & ~0xf) == 0);
    assert((regptr & ~0xf) == 0);
    assert((offset & ~0b111100) == 0);
    (*dst)[0] = (regsrc << 4) | 0b1001;
    (*dst)[1] = (offset << 2) | regptr;
    *dst += 2;
}

static void add_n(uint8_t ** dst, int regdst, int regleft, int regright)
{
    assert((regdst & ~0xf) == 0);
    assert((regleft & ~0xf) == 0);
    assert((regright & ~0xf) == 0);
    (*dst)[0] = (regright << 4) | 0b1010;
    (*dst)[1] = (regdst << 4) | regleft;
    *dst += 2;
}

static void cpy_and_inc(uint8_t ** dst, const uint8_t * src, int n)
{
    memcpy(*dst, src, n);
    *dst += n;
}

static void le3(uint8_t ** dst, uint32_t val)
{
    (*dst)[0] = val & 0xff;
    val >>= 8;
    (*dst)[1] = val & 0xff;
    val >>= 8;
    (*dst)[2] = val & 0xff;
    *dst += 3;
}

/*
target_pos  ==  (call0_pos & -4) + (offset << 2) + 4
(target_pos - (call0_pos & -4) - 4) >> 2  ==  offset
*/
static void call0(uint8_t ** dst, int target_pos, int call0_pos)
{
    assert(target_pos % 4 == 0);
    int offset = (target_pos - (call0_pos & -4) - 4) >> 2;
    assert(offset >= -131072 && offset <= 131071); /* TODO handle further offsets */

    uint32_t code = (offset << 6) | 0x5;
    le3(dst, code);
}

static void nop_n(uint8_t ** dst)
{
    (*dst)[0] = 0x3d;
    (*dst)[1] = 0xf0;
    *dst += 2;
}

static void callx0(uint8_t ** dst, int reg)
{
    assert((reg & ~0xf) == 0);
    uint32_t code = (reg << 8) | (0b11 << 6);
    le3(dst, code);
}

static void mov_n(uint8_t ** dst, int regdst, int regsrc)
{
    assert((regdst & ~0xf) == 0);
    assert((regsrc & ~0xf) == 0);
    (*dst)[0] = (regdst << 4) | 0b1101;
    (*dst)[1] = regsrc;
    *dst += 2;
}

static void movi_n(uint8_t ** dst, int regdst, int imm)
{
    assert((regdst & ~0xf) == 0);
    assert(imm >= -32 && imm <= 95);
    (*dst)[0] = (imm & 0x70) | 0b1100;
    (*dst)[1] = ((imm & 0x0f) << 4) | regdst;
    *dst += 2;
}

static void ret_n(uint8_t ** dst)
{
    (*dst)[0] = 0b00001101;
    (*dst)[1] = 0b11110000;
    *dst += 2;
}

static void addi_n(uint8_t ** dst, int regdst, int regsrc, int imm)
{
    assert((regdst & ~0xf) == 0);
    assert((regsrc & ~0xf) == 0);
    assert(imm >= -1 && imm <= 15 && imm != 0);
    (*dst)[0] = ((imm == -1 ? 0 : imm) << 4) | 0b1011;
    (*dst)[1] = (regdst << 4) | regsrc;
    *dst += 2;
}

static void l32i(uint8_t ** dst, int regdst, int regptr, int offset)
{
    assert((regdst & ~0xf) == 0);
    assert((regptr & ~0xf) == 0);
    assert((offset & ~0x3fc) == 0);
    (*dst)[0] = (regdst << 4) | 0b0010;
    (*dst)[1] = (0b0010 << 4) | regptr;
    (*dst)[2] = offset >> 2;
    *dst += 3;
}

static void s32i(uint8_t ** dst, int regsrc, int regptr, int offset)
{
    assert((regsrc & ~0xf) == 0);
    assert((regptr & ~0xf) == 0);
    assert((offset & ~0x3fc) == 0);
    (*dst)[0] = (regsrc << 4) | 0b0010;
    (*dst)[1] = (0b0110 << 4) | regptr;
    (*dst)[2] = offset >> 2;
    *dst += 3;
}

static void addi(uint8_t ** dst, int regdst, int regsrc, int imm)
{
    assert((regdst & ~0xf) == 0);
    assert((regsrc & ~0xf) == 0);
    assert(imm >= -128 && imm <= 127);
    (*dst)[0] = (regdst << 4) | 0b0010;
    (*dst)[1] = (0b1100 << 4) | regsrc;
    (*dst)[2] = imm & 0xff;
    *dst += 3;
}

static void and(uint8_t ** dst, int regdst, int regleft, int regright)
{
    assert((regdst & ~0xf) == 0);
    assert((regleft & ~0xf) == 0);
    assert((regright & ~0xf) == 0);
    (*dst)[0] = (regright << 4) | 0b0000;
    (*dst)[1] = (regdst << 4) | regleft;
    (*dst)[2] = 0b00010000;
    *dst += 3;
}

static void addmi(uint8_t ** dst, int regdst, int regsrc, int raw_imm)
{
    assert((regdst & ~0xf) == 0);
    assert((regsrc & ~0xf) == 0);
    assert(raw_imm >= -128 && raw_imm <= 127);
    (*dst)[0] = (regdst << 4) | 0b0010;
    (*dst)[1] = (0b1101 << 4) | regsrc;
    (*dst)[2] = raw_imm;
    *dst += 3;
}

static bool s32i_l32i_auto_using_relaxation_is_representable(int relaxation, int offset)
{
    assert(!(offset & 0x3));
    offset /= 4;
    assert(offset <= 0x1fff); /* TODO handle offsets more than 8191 */
    switch(relaxation) {
        case 0: return offset <= 0xf;
        case 1: return offset <= 0xff;
        case 2: return (offset & 0xff) <= 0xf;
        default: return true;
    }
}

static int s32i_l32i_auto_using_relaxation_size(int relaxation)
{
    switch(relaxation) {
        case 0: return 2;
        case 1: return 3;
        case 2: return 5;
        case 3: return 6;
        default: assert(0);
    }
}

static int s32i_l32i_auto_size(int offset)
{
    int relaxation = 0;
    while(!s32i_l32i_auto_using_relaxation_is_representable(relaxation, offset)) {
        relaxation += 1;
    }
    return s32i_l32i_auto_using_relaxation_size(relaxation);
}

static void s32i_l32i_auto_using_relaxation(bool is_store, int relaxation, uint8_t ** dst, int regsrcdst, int regptr,
                                             int offset, int regscratch)
{
    assert(!(offset & 0x3));
    assert(offset / 4 <= 0x1fff); /* TODO handle offsets more than 8191 */
    switch(relaxation) {
        case 0:
            if(is_store)
                s32i_n(dst, regsrcdst, regptr, offset);
            else
                l32i_n(dst, regsrcdst, regptr, offset);
            return;
        case 1:
            if(is_store)
                s32i(dst, regsrcdst, regptr, offset);
            else
                l32i(dst, regsrcdst, regptr, offset);
            return;
        case 2:
            addmi(dst, regscratch, regptr, (offset & ~0x3ff) >> 8);
            if(is_store)
                s32i_n(dst, regsrcdst, regscratch, offset & 0x3ff);
            else
                l32i_n(dst, regsrcdst, regscratch, offset & 0x3ff);
            return;
        case 3:
            addmi(dst, regscratch, regptr, (offset & ~0x3ff) >> 8);
            if(is_store)
                s32i(dst, regsrcdst, regscratch, offset & 0x3ff);
            else
                l32i(dst, regsrcdst, regscratch, offset & 0x3ff);
            return;
        default:
            assert(0);
    }
}

static void s32i_l32i_auto(bool is_store, uint8_t ** dst, int regsrcdst, int regptr, int offset,
                           int regscratch)
{
    int relaxation = 0;
    while(!s32i_l32i_auto_using_relaxation_is_representable(relaxation, offset)) {
        relaxation += 1;
    }
    s32i_l32i_auto_using_relaxation(is_store, relaxation, dst, regsrcdst, regptr, offset, regscratch);
}

static bool movi_auto_using_relaxation_is_representable(m4_compiler_ctx_t * compiler, int relaxation, int imm)
{
    switch(relaxation) {
        case 0: return imm >= -32 && imm <= 95;
        case 1: return imm >= -2048 && imm <= 2047;
        default: {
            int literal_offset = m4_compiler_literal(compiler, imm);
            return s32i_l32i_auto_using_relaxation_is_representable(relaxation - 2, literal_offset * 4);
        }
    }
}

static int movi_auto_using_relaxation_size(int relaxation)
{
    switch(relaxation) {
        case 0: return 2;
        case 1: return 3;
        default: return s32i_l32i_auto_using_relaxation_size(relaxation - 2);
    }
}

static int movi_auto_size(m4_compiler_ctx_t * compiler, int imm)
{
    int relaxation = 0;
    while(!movi_auto_using_relaxation_is_representable(compiler, relaxation, imm)) {
        relaxation += 1;
    }
    return movi_auto_using_relaxation_size(relaxation);
}

static void movi_auto_using_relaxation(m4_compiler_ctx_t * compiler, int relaxation, uint8_t ** dst, int regdst, int imm)
{
    switch(relaxation) {
        case 0:
            movi_n(dst, regdst, imm);
            return;
        case 1:
            /* movi regdst, imm */
            (*dst)[0] = (regdst << 4) | 0b0010;
            (*dst)[1] = (0b1010 << 4) | ((imm & 0xf00) >> 8);
            (*dst)[2] = imm & 0x0ff;
            *dst += 3;
            return;
        default: {
            int literal_offset = m4_compiler_literal(compiler, imm);
            s32i_l32i_auto_using_relaxation(false, relaxation - 2, dst, regdst, LA, literal_offset * 4, regdst);
            return;
        }
    }
}

static void movi_auto(m4_compiler_ctx_t * compiler, uint8_t ** dst, int regdst, int imm)
{
    int relaxation = 0;
    while(!movi_auto_using_relaxation_is_representable(compiler, relaxation, imm)) {
        relaxation += 1;
    }
    movi_auto_using_relaxation(compiler, relaxation, dst, regdst, imm);
}

static bool addi_auto_using_relaxation_is_representable(m4_compiler_ctx_t * compiler, int relaxation, int imm)
{
    switch(relaxation) {
        case 0: return imm == 0;
        case 1: return imm >= -1 && imm <= 15 && imm != 0;
        case 2: return imm >= -128 && imm <= 127;
        default: return movi_auto_using_relaxation_is_representable(compiler, relaxation - 3, imm);
    }
}

static int addi_auto_using_relaxation_size(int relaxation)
{
    switch(relaxation) {
        case 0: return 0;
        case 1: return 2;
        case 2: return 3;
        default: return movi_auto_using_relaxation_size(relaxation - 3) + 2;
    }
}

static int addi_auto_size(m4_compiler_ctx_t * compiler, int imm)
{
    int relaxation = 0;
    while(!addi_auto_using_relaxation_is_representable(compiler, relaxation, imm)) {
        relaxation += 1;
    }
    return addi_auto_using_relaxation_size(relaxation);
}

static void addi_auto_using_relaxation(m4_compiler_ctx_t * compiler, int relaxation, uint8_t ** dst, int regdst, int regsrc, int imm, int regscratch)
{
    switch(relaxation) {
        case 0:
            return;
        case 1:
            addi_n(dst, regdst, regsrc, imm);
            return;
        case 2:
            addi(dst, regdst, regsrc, imm);
            return;
        default:
            movi_auto_using_relaxation(compiler, relaxation - 3, dst, regscratch, imm);
            add_n(dst, regdst, regsrc, regscratch);
            return;
    }
}

static void addi_auto(m4_compiler_ctx_t * compiler, uint8_t ** dst, int regdst, int regsrc, int imm, int regscratch)
{
    int relaxation = 0;
    while(!addi_auto_using_relaxation_is_representable(compiler, relaxation, imm)) {
        relaxation += 1;
    }
    return addi_auto_using_relaxation(compiler, relaxation, dst, regdst, regsrc, imm, regscratch);
}

static void beqz_n(uint8_t ** dst, int regsrc, int offset)
{
    assert((regsrc & ~0xf) == 0);
    assert(offset >= 0 && offset <= 63);
    (*dst)[0] = (offset & 0x30) | 0b10001100;
    (*dst)[1] = (offset << 4) | regsrc;
    *dst += 2;
}

static void bnez_n(uint8_t ** dst, int regsrc, int offset)
{
    assert((regsrc & ~0xf) == 0);
    assert(offset >= 0 && offset <= 63);
    (*dst)[0] = (offset & 0x30) | 0b11001100;
    (*dst)[1] = (offset << 4) | regsrc;
    *dst += 2;
}

static void beqz(uint8_t ** dst, int regsrc, int offset)
{
    assert((regsrc & ~0xf) == 0);
    assert(offset >= -2048 && offset <= 2047);
    uint32_t code = (offset << 12) | (regsrc << 8) | 0b10110;
    le3(dst, code);
}

static void blt(uint8_t ** dst, int regleft, int regright, int offset)
{
    assert((regleft & ~0xf) == 0);
    assert((regright & ~0xf) == 0);
    assert(offset >= -128 && offset <= 127);
    (*dst)[0] = (regright << 4) | 0b0111;
    (*dst)[1] = (0b0010 << 4) | regleft;
    (*dst)[2] = offset & 0xff;
    *dst += 3;
}

static void bge(uint8_t ** dst, int regleft, int regright, int offset)
{
    assert((regleft & ~0xf) == 0);
    assert((regright & ~0xf) == 0);
    assert(offset >= -128 && offset <= 127);
    (*dst)[0] = (regright << 4) | 0b0111;
    (*dst)[1] = (0b1010 << 4) | regleft;
    (*dst)[2] = offset & 0xff;
    *dst += 3;
}

static void j(uint8_t ** dst, int offset)
{
    assert(offset >= -131072 && offset <= 131071);
    uint32_t code = (offset << 6) | 0b110;
    le3(dst, code);
}

static int beqz_auto_size(int relaxation, int beqz_pos)
{
    switch(relaxation) {
        case 0: return 2;
        case 1: return 3;
        case 2: {
            int skip_target_pos = beqz_pos + 5;
            if((skip_target_pos % 4) <= 1) return 5;
            return 7;
        }
    }
    assert(0);
}

static bool beqz_auto_is_representable(int relaxation, int target_pos, int beqz_pos)
{
    int offset = target_pos - beqz_pos - 4;
    switch(relaxation) {
        case 0:
            return offset >= 0 && offset <= 63;
        case 1:
            return offset >= -2048 && offset <= 2047;
        case 2:
            return true;
        default:
            assert(0);
    }
}

static void beqz_auto(uint8_t ** dst, int relaxation, int regsrc, int target_pos, int beqz_pos)
{
    int offset = target_pos - beqz_pos - 4;
    switch(relaxation) {
        case 0:
            assert(offset >= 0 && offset <= 63);
            beqz_n(dst, regsrc, offset);
            break;
        case 1:
            assert(offset >= -2048 && offset <= 2047);
            beqz(dst, regsrc, offset);
            break;
        case 2: {
            int skip_target_pos = beqz_pos + 5;
            if((skip_target_pos % 4) <= 1) {
                bnez_n(dst, regsrc, 1);
                j(dst, offset - 2);
            }
            else {
                bnez_n(dst, regsrc, 3);
                j(dst, offset - 2);
                nop_n(dst);
            }
            break;
        }
        default:
            assert(0);
    }
}

static int blt_auto_size(int relaxation, int blt_pos)
{
    switch(relaxation) {
        case 0: return 3;
        case 1: {
            int skip_target_pos = blt_pos + 6;
            if((skip_target_pos % 4) <= 1) return 6;
            return 8;
        }
    }
    assert(0);
}

static bool blt_auto_is_representable(int relaxation, int target_pos, int blt_pos)
{
    int offset = target_pos - blt_pos - 4;
    switch(relaxation) {
        case 0:
            return offset >= -128 && offset <= 127;
        case 1:
            return true;
        default:
            assert(0);
    }
}

static void blt_auto(uint8_t ** dst, int relaxation, int regleft, int regright,
                     int target_pos, int blt_pos)
{
    int offset = target_pos - blt_pos - 4;
    switch(relaxation) {
        case 0:
            assert(offset >= -128 && offset <= 127);
            blt(dst, regleft, regright, offset);
            break;
        case 1: {
            int skip_target_pos = blt_pos + 6;
            if((skip_target_pos % 4) <= 1) {
                bge(dst, regleft, regright, 2);
                j(dst, offset - 3);
            }
            else {
                bge(dst, regleft, regright, 4);
                j(dst, offset - 3);
                nop_n(dst);
            }
            break;
        }
        default:
            assert(0);
    }
}

static int fragment_bin_size(m4_compiler_ctx_t * compiler, const m4_fragment_t * all_fragments,
                             const int * sequence, int sequence_len, int sequence_i) {
    int needs = m4_backend_fragment_get_needed_chores(all_fragments, sequence, sequence_len, sequence_i, get_flags);
    int size = 0;
    if(needs & M4_BACKEND_FLAG_A) size += sizeof(m4_esp32s3_backend_chore_a);
    if(needs & M4_BACKEND_FLAG_B) size += sizeof(m4_esp32s3_backend_chore_b);
    const m4_fragment_t * fragment = &all_fragments[sequence[sequence_i]];
    switch(fragment->op) {
        case M4_OPCODE_BUILTIN_WORD:
            size += m4_esp32s3_backend_table[fragment->param].len;
            break;
        case M4_OPCODE_DEFINED_WORD:
            size += (fragment->position + size + 3) % 4 <= 1 ? 3 : 5;
            break;
        case M4_OPCODE_RUNTIME_WORD:
            size += movi_auto_size(compiler, fragment->param);
            size += (fragment->position + size + 3) % 4 <= 1 ? 3 : 5;
            break;
        case M4_OPCODE_PUSH_LITERAL:
            size += movi_auto_size(compiler, fragment->param);
            break;
        case M4_OPCODE_EXIT_WORD: {
            size += 2;
            int defined_word_param = all_fragments[fragment->param].param;
            if(defined_word_param == M4_DEFINED_WORD_LOCATION_PARAM_LOOPS
               || defined_word_param == M4_DEFINED_WORD_LOCATION_PARAM_CALLS) size += 4;
            else if(defined_word_param == (M4_DEFINED_WORD_LOCATION_PARAM_LOOPS
                                           | M4_DEFINED_WORD_LOCATION_PARAM_CALLS)) size += 6;
            break;
        }
        case M4_OPCODE_DEFINED_WORD_LOCATION: {
            int defined_word_param = fragment->param;
            if(defined_word_param == M4_DEFINED_WORD_LOCATION_PARAM_LOOPS) size += 6;
            else if(defined_word_param == M4_DEFINED_WORD_LOCATION_PARAM_CALLS) size += 4;
            else if(defined_word_param == (M4_DEFINED_WORD_LOCATION_PARAM_LOOPS
                                           | M4_DEFINED_WORD_LOCATION_PARAM_CALLS)) size += 8;
            break;
        }
        case M4_OPCODE_PUSH_DATA_ADDRESS:
            size += 2 + addi_auto_size(compiler, fragment->param);
            break;
        case M4_OPCODE_DECLARE_VARIABLE:
            size += 7 + s32i_l32i_auto_size(fragment->param * 4);
            break;
        case M4_OPCODE_USE_VARIABLE_OR_CONSTANT:
            size += s32i_l32i_auto_size(fragment->param * 4);
            break;
        case M4_OPCODE_HALT:
            size += 4;
            break;
        case M4_OPCODE_BRANCH_LOCATION:
            break;
        case M4_OPCODE_BRANCH_IF_ZERO:
            size += 6;
            size += beqz_auto_size(fragment->relaxation, fragment->position + size);
            break;
        case M4_OPCODE_BRANCH:
            size += 3;
            break;
        case M4_OPCODE_DO:
            size += 13;
            break;
        case M4_OPCODE_LOOP:
            size += 8;
            size += blt_auto_size(fragment->relaxation, fragment->position + size);
            size += 3;
            break;
        case M4_OPCODE_PUSH_OFFSET_ADDRESS:
            size += 2 + addi_auto_using_relaxation_size(fragment->relaxation);
            break;
        case M4_OPCODE_PUSH_CALLBACK:
            size += 2 + s32i_l32i_auto_size(fragment->param * 4);
            break;
        case M4_OPCODE_DECLARE_CONSTANT:
            size += s32i_l32i_auto_size(fragment->param * 4);
            break;
        case M4_OPCODE_EXECUTE:
            size += 6;
            if((fragment->position + size + 3) % 4 > 1) size += 2;
            size += 3;
            break;
        case M4_OPCODE_THREAD_CREATE:
        case M4_OPCODE_THREAD_JOIN:
            size += 2;
            if((fragment->position + size + 3) % 4 > 1) size += 2;
            size += 3;
            break;
        case M4_OPCODE_ENTRY:
            size += 2;
            break;
        default:
            assert(0);
    }
    if(needs & M4_BACKEND_FLAG_Z) size += sizeof(m4_esp32s3_backend_chore_z);
    if(needs & M4_BACKEND_FLAG_Y) size += sizeof(m4_esp32s3_backend_chore_y);

    if(sequence_i + 1 < sequence_len) {
        m4_opcode_t next_fragment_op = all_fragments[sequence[sequence_i + 1]].op;
        if(next_fragment_op == M4_OPCODE_DEFINED_WORD_LOCATION) {
            size += (4 - (fragment->position + size) % 4) % 4;
        }
        else if(next_fragment_op == M4_OPCODE_BRANCH_LOCATION) {
            if((fragment->position + size) % 4 > 1) size += 2;
        }
    }

    return size;
}

static bool fragment_bin_is_representable(m4_compiler_ctx_t * compiler, const m4_fragment_t * all_fragments,
                                          const int * sequence, int sequence_len, int sequence_i)
{
    const m4_fragment_t * fragment = &all_fragments[sequence[sequence_i]];
    m4_opcode_t op = fragment->op;
    int needs;
    int start_chores_len;
    switch(op) {
        case M4_OPCODE_BRANCH_IF_ZERO:
        case M4_OPCODE_LOOP:
            needs = m4_backend_fragment_get_needed_chores(all_fragments, sequence, sequence_len, sequence_i, get_flags);
            start_chores_len = 0;
            if(needs & M4_BACKEND_FLAG_A) start_chores_len += sizeof(m4_esp32s3_backend_chore_a);
            if(needs & M4_BACKEND_FLAG_B) start_chores_len += sizeof(m4_esp32s3_backend_chore_b);
            break;
        case M4_OPCODE_PUSH_OFFSET_ADDRESS:
            break;
        default:
            return true;
    }
    switch(op) {
        case M4_OPCODE_BRANCH_IF_ZERO:
            return beqz_auto_is_representable(fragment->relaxation, all_fragments[fragment->param].position,
                                              fragment->position + start_chores_len);
        case M4_OPCODE_LOOP:
            return blt_auto_is_representable(fragment->relaxation, all_fragments[fragment->param].position,
                                             fragment->position + start_chores_len);
        case M4_OPCODE_PUSH_OFFSET_ADDRESS:
            return addi_auto_using_relaxation_is_representable(compiler, fragment->relaxation, all_fragments[fragment->param].position);
        default:
            assert(0);
    }
}

static void fragment_bin_dump(m4_compiler_ctx_t * compiler, const m4_fragment_t * all_fragments,
                              const int * sequence, int sequence_len, int sequence_i, uint8_t * dst) {
    uint8_t * dst_start = dst;
    int needs = m4_backend_fragment_get_needed_chores(all_fragments, sequence, sequence_len, sequence_i, get_flags);
    if(needs & M4_BACKEND_FLAG_A) cpy_and_inc(&dst, m4_esp32s3_backend_chore_a, sizeof(m4_esp32s3_backend_chore_a));
    if(needs & M4_BACKEND_FLAG_B) cpy_and_inc(&dst, m4_esp32s3_backend_chore_b, sizeof(m4_esp32s3_backend_chore_b));
    const m4_fragment_t * fragment = &all_fragments[sequence[sequence_i]];
    switch(fragment->op) {
        case M4_OPCODE_BUILTIN_WORD:
            cpy_and_inc(&dst,
                        &m4_esp32s3_backend_code[m4_esp32s3_backend_table[fragment->param].pos],
                        m4_esp32s3_backend_table[fragment->param].len);
            break;
        case M4_OPCODE_DEFINED_WORD: {
            int call0_pos = fragment->position + (dst - dst_start);
            if((call0_pos + 3) % 4 > 1) {
                nop_n(&dst);
                call0_pos += 2;
            }
            call0(&dst, all_fragments[fragment->param].position, call0_pos);
            break;
        }
        case M4_OPCODE_RUNTIME_WORD: {
            movi_auto(compiler, &dst, S0, fragment->param);
            int callx0_pos = fragment->position + (dst - dst_start);
            if((callx0_pos + 3) % 4 > 1) {
                nop_n(&dst);
            }
            callx0(&dst, RC);
            break;
        }
        case M4_OPCODE_PUSH_LITERAL:
            movi_auto(compiler, &dst, DT, fragment->param);
            break;
        case M4_OPCODE_EXIT_WORD: {
            int defined_word_param = all_fragments[fragment->param].param;
            if(defined_word_param == M4_DEFINED_WORD_LOCATION_PARAM_LOOPS) {
                mov_n(&dst, RS, RB);
                l32i_n(&dst, RB, RS, RSO+4);
            }
            else if(defined_word_param == M4_DEFINED_WORD_LOCATION_PARAM_CALLS) {
                add_n(&dst, RS, RS, N4);
                l32i_n(&dst, LR, RS, RSO+4);
            }
            else if(defined_word_param == (M4_DEFINED_WORD_LOCATION_PARAM_LOOPS
                                           | M4_DEFINED_WORD_LOCATION_PARAM_CALLS)) {
                mov_n(&dst, RS, RB);
                l32i_n(&dst, RB, RS, RSO+4);
                l32i_n(&dst, LR, RS, RSO+8);
            }
            ret_n(&dst);
            break;
        }
        case M4_OPCODE_DEFINED_WORD_LOCATION: {
            int defined_word_param = fragment->param;
            if(defined_word_param == M4_DEFINED_WORD_LOCATION_PARAM_LOOPS) {
                s32i_n(&dst, RB, RS, RSO+4);
                mov_n(&dst, RB, RS);
                addi_n(&dst, RS, RS, 4);
            }
            else if(defined_word_param == M4_DEFINED_WORD_LOCATION_PARAM_CALLS) {
                s32i_n(&dst, LR, RS, RSO+4);
                addi_n(&dst, RS, RS, 4);
            }
            else if(defined_word_param == (M4_DEFINED_WORD_LOCATION_PARAM_LOOPS
                                           | M4_DEFINED_WORD_LOCATION_PARAM_CALLS)) {
                s32i_n(&dst, RB, RS, RSO+4);
                s32i_n(&dst, LR, RS, RSO+8);
                mov_n(&dst, RB, RS);
                addi_n(&dst, RS, RS, 8);
            }
            break;
        }
        case M4_OPCODE_PUSH_DATA_ADDRESS:
            l32i_n(&dst, DT, CS, DATA);
            addi_auto(compiler, &dst, DT, DT, fragment->param, S0);
            break;
        case M4_OPCODE_DECLARE_VARIABLE:
            addi_n(&dst, DP, DP, 3);
            and(&dst, DP, DP, N4);
            s32i_l32i_auto(true, &dst, DP, VA, fragment->param * 4, S0);
            addi_n(&dst, DP, DP, 4);
            break;
        case M4_OPCODE_USE_VARIABLE_OR_CONSTANT:
            s32i_l32i_auto(false, &dst, DT, VA, fragment->param * 4, S0);
            break;
        case M4_OPCODE_HALT:
            l32i_n(&dst, LR, SP, 4);
            ret_n(&dst);
            break;
        case M4_OPCODE_BRANCH_LOCATION:
            break;
        case M4_OPCODE_BRANCH_IF_ZERO:
            mov_n(&dst, S0, DT);
            l32i_n(&dst, DT, DS, DSO);
            add_n(&dst, DS, DS, N4);
            beqz_auto(&dst, fragment->relaxation, S0, all_fragments[fragment->param].position,
                      fragment->position + (dst - dst_start));
            break;
        case M4_OPCODE_BRANCH: {
            int j_pos = fragment->position + (dst - dst_start);
            int target_pos = all_fragments[fragment->param].position;
            int offset = target_pos - j_pos - 4;
            j(&dst, offset);
            break;
        }
        case M4_OPCODE_DO:
            l32i_n(&dst, S0, DS, DSO);
            addi_n(&dst, RS, RS, 8);
            s32i_n(&dst, S0, RS, RSO-4);
            s32i_n(&dst, DT, RS, RSO);
	    l32i_n(&dst, DT, DS, DSO-4);
            addi(&dst,   DS, DS, -8);
            break;
        case M4_OPCODE_LOOP:
            l32i_n(&dst, S0, RS, RSO);
            addi_n(&dst, S0, S0, 1);
            s32i_n(&dst, S0, RS, RSO);
            l32i_n(&dst, S1, RS, RSO-4);
            blt_auto(&dst, fragment->relaxation, S0, S1, all_fragments[fragment->param].position,
                     fragment->position + (dst - dst_start));
            addi(&dst,   RS, RS, -8);
            break;
        case M4_OPCODE_PUSH_OFFSET_ADDRESS: {
            l32i_n(&dst, DT, CS, BIN_START);
            addi_auto_using_relaxation(compiler, fragment->relaxation, &dst, DT, DT,
                                       all_fragments[fragment->param].position, S0);
            break;
        }
        case M4_OPCODE_PUSH_CALLBACK:
            l32i_n(&dst, DT, CS, CALLBACK_TARGETS);
            s32i_l32i_auto(false, &dst, DT, DT, fragment->param * 4, S0);
        case M4_OPCODE_DECLARE_CONSTANT:
            s32i_l32i_auto(true, &dst, DT, VA, fragment->param * 4, S0);
            break;
        case M4_OPCODE_EXECUTE: {
            mov_n(&dst, S0, DT);
            int callx0_pos = fragment->position + (dst - dst_start) + 4;
            if((callx0_pos + 3) % 4 <= 1) {
                l32i_n(&dst, DT, DS, DSO);
                add_n(&dst, DS, DS, N4);
            }
            else {
                l32i(&dst, DT, DS, DSO);
                addi(&dst, DS, DS, -4);
            }
            callx0(&dst, S0);
            break;
        }
        case M4_OPCODE_THREAD_CREATE: {
            movi_n(&dst, S0, -1);
            int callx0_pos = fragment->position + (dst - dst_start);
            if((callx0_pos + 3) % 4 > 1) {
                nop_n(&dst);
            }
            callx0(&dst, RC);
            break;
        }
        case M4_OPCODE_THREAD_JOIN: {
            movi_n(&dst, S0, -2);
            int callx0_pos = fragment->position + (dst - dst_start);
            if((callx0_pos + 3) % 4 > 1) {
                nop_n(&dst);
            }
            callx0(&dst, RC);
            break;
        }
        case M4_OPCODE_ENTRY:
            s32i_n(&dst, LR, SP, 4);
            break;
        default:
            assert(0);
    }
    if(needs & M4_BACKEND_FLAG_Y) cpy_and_inc(&dst, m4_esp32s3_backend_chore_y, sizeof(m4_esp32s3_backend_chore_y));
    if(needs & M4_BACKEND_FLAG_Z) cpy_and_inc(&dst, m4_esp32s3_backend_chore_z, sizeof(m4_esp32s3_backend_chore_z));

    if(sequence_i + 1 < sequence_len) {
        m4_opcode_t next_fragment_op = all_fragments[sequence[sequence_i + 1]].op;
        if(next_fragment_op == M4_OPCODE_DEFINED_WORD_LOCATION) {
            int pad_amnt = (4 - ((uintptr_t) dst) % 4) % 4;
            memset(dst, 0, pad_amnt);
            dst += pad_amnt;
        }
        else if(next_fragment_op == M4_OPCODE_BRANCH_LOCATION) {
            if(((uintptr_t) dst) % 4 > 1) nop_n(&dst);
        }
    }
}

static bool op_uses_lr(m4_opcode_t op)
{
    switch(op) {
        case M4_OPCODE_DEFINED_WORD:
        case M4_OPCODE_RUNTIME_WORD:
        case M4_OPCODE_EXECUTE:
        case M4_OPCODE_THREAD_CREATE:
        case M4_OPCODE_THREAD_JOIN:
            return true;
        default:
            return false;
    }
}

const m4_backend_t m4_esp32s3_backend = {
    .fragment_bin_size=fragment_bin_size,
    .fragment_bin_is_representable=fragment_bin_is_representable,
    .fragment_bin_dump=fragment_bin_dump,
    .op_uses_lr=op_uses_lr
};
