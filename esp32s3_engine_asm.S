#define M4_ESP32S3_DEFINE_REG_ALIASES
#include "esp32s3.h"

.extern m4_global_get_ctx
.extern m4_lit

.globl m4_esp32s3_engine_run_asm
.globl m4_esp32s3_engine_callback_target_0
.globl m4_esp32s3_engine_callback_target_1
.globl m4_esp32s3_engine_callback_target_2
.globl m4_esp32s3_engine_callback_target_3
.globl m4_esp32s3_engine_callback_target_4
.globl m4_esp32s3_engine_callback_target_5
.globl m4_esp32s3_engine_callback_target_6
.globl m4_esp32s3_engine_callback_target_7


.align 4
m4_esp32s3_engine_run_asm:
entry  sp, 48

mov    s0, a3
l32i   rs, cs, RETURN_STACK_DATA
addi   rs, rs, -rso - 4
l32i   dp, cs, MEMORY
l32i   va, cs, VARIABLES
l32i   la, cs, LITERALS

movi   rc, call_runtime_word
movi   n1, -1
movi   n4, -4
l32i   ds, cs, STACK_DATA
addi   ds, ds, -dso - 4
l32i   dt, ds, dso

s32i   lr, sp, 0
callx0 s0
l32i   lr, sp, 0

// save state
s32i   rs, cs, RETURN_STACK_DATA
s32i   dp, cs, MEMORY
addi   ds, ds, dso+4
s32i   dt, ds, 0
l32i   s2, cs, DEPTH
sub    s2, ds, s2
srli   s2, s2, 2
addi   ds, ds, 4
s32i   ds, cs, STACK_DATA
s32i   s2, cs, STACK_LEN

retw


.align 4
call_runtime_word:

addi   ds, ds, 4
s32i   dt, ds, dso

addi   s1, cs, RUNTIME_CBS
addi   s2, s1, SPECIAL_RUNTIME_CBS - RUNTIME_CBS
movltz s1, s2, s0
srai   s2, s0, 31
xor    s0, s0, s2

l32i   s1, s1, 0
addx4  s0, s0, s1
l32i   s0, s0, 0

l32i   dt, s0, 0

movi   s2, m4_lit
bne    dt, s2, 1f
l32i   dt, s0, 4
ret
1:

// save state
s32i   rs, cs, RETURN_STACK_DATA
s32i   dp, cs, MEMORY
addi   ds, ds, dso
l32i   s2, cs, DEPTH
sub    s2, ds, s2
srli   s2, s2, 2
addi   ds, ds, 4
s32i   ds, cs, STACK_DATA
s32i   s2, cs, STACK_LEN

l32i   s2, s0, 4
mov    s1, cs
callx8 dt

l32i   rs, cs, RETURN_STACK_DATA
l32i   dp, cs, MEMORY
movi   rc, call_runtime_word
movi   n1, -1
movi   n4, -4
l32i   ds, cs, STACK_DATA
addi   ds, ds, -dso - 8
l32i   dt, ds, dso + 4

ret


.align 4
m4_esp32s3_engine_callback_target_0:
entry  sp, 48
movi   a10, 0
j      callback_handler
.align 4
m4_esp32s3_engine_callback_target_1:
entry  sp, 48
movi   a10, 1
j      callback_handler
.align 4
m4_esp32s3_engine_callback_target_2:
entry  sp, 48
movi   a10, 2
j      callback_handler
.align 4
m4_esp32s3_engine_callback_target_3:
entry  sp, 48
movi   a10, 3
j      callback_handler
.align 4
m4_esp32s3_engine_callback_target_4:
entry  sp, 48
movi   a10, 4
j      callback_handler
.align 4
m4_esp32s3_engine_callback_target_5:
entry  sp, 48
movi   a10, 5
j      callback_handler
.align 4
m4_esp32s3_engine_callback_target_6:
entry  sp, 48
movi   a10, 6
j      callback_handler
.align 4
m4_esp32s3_engine_callback_target_7:
entry  sp, 48
movi   a10, 7
callback_handler:

s32i   a10, sp,  0
_call8  m4_global_get_ctx
l32i   a8,  sp,  0

l32i   a9,  a10, CALLBACK_INFO
add    a9,  a9,  a8
l8ui   a9,  a9,  0
s32i   a9,  sp,  4
srli   a9,  a9,  1

l32i   a15, a10, STACK_DATA
addx4  a13, a9,  a15

s32i   a2,  a15, 0
beqi   a9,  1,   2f
s32i   a3,  a15, 4
beqi   a9,  2,   2f
s32i   a4,  a15, 8
beqi   a9,  3,   2f
s32i   a5,  a15, 12
beqi   a9,  4,   2f
s32i   a6,  a15, 16
beqi   a9,  5,   2f
s32i   a7,  a15, 20
beqi   a9,  6,   2f

mov    a12, sp
addi   a11, a15, 24
1:
l32i   a14, a12, 48
s32i   a14, a11, 0
addi   a12, a12, 4
addi   a11, a11, 4
bne    a11, a13, 1b
2:

mov    a2,  a10

addi   a13, a13, -dso - 8
l32i   a14, a13, dso + 4

l32i   rs, cs, RETURN_STACK_DATA
l32i   dp, cs, MEMORY
l32i   va, cs, VARIABLES
l32i   la, cs, LITERALS

l32i   a10, cs,  CALLBACK_LOCATIONS
addx4  a10, a8,  a10
l32i   a10, a10, 0

movi   rc, call_runtime_word
movi   n1, -1
movi   n4, -4

s32i   lr, sp, 0
callx0 a10
l32i   lr, sp, 0

l32i   s0, sp, 4
bbsi.l s0, 0,  1f
addi   ds, ds, 4
s32i   dt, ds, dso
1:

// save state
s32i   rs, cs, RETURN_STACK_DATA
s32i   dp, cs, MEMORY
addi   ds, ds, dso
l32i   s2, cs, DEPTH
sub    s2, ds, s2
srli   s2, s2, 2
addi   ds, ds, 4
s32i   ds, cs, STACK_DATA
s32i   s2, cs, STACK_LEN

mov    a2, dt

retw
