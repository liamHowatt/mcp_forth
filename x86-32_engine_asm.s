[BITS 32]
global m4_x86_32_engine_run_asm
global m4_x86_32_engine_call_runtime_word
global m4_x86_32_engine_callback_target_0
global m4_x86_32_engine_callback_target_1
global m4_x86_32_engine_callback_target_2
global m4_x86_32_engine_callback_target_3
global m4_x86_32_engine_callback_target_4
global m4_x86_32_engine_callback_target_5
global m4_x86_32_engine_callback_target_6
global m4_x86_32_engine_callback_target_7
extern m4_x86_32_engine_get_global


%macro save_state 1
mov %1, ebx       ; use %1 to calculate stack depth
sub %1, [esi+28]  ; subtract stack base. same calculation as "depth" word
shr %1, 2         ; express as elements instead of bytes
add ebx, 4        ; m4_stack_t has *data point 1 past the end
mov [esi+0], ebx  ; store *data in stack struct
mov [esi+8], %1   ; store len in stack struct
mov [esi+12], edx ; store data-space pointer
%endmacro


m4_x86_32_engine_run_asm:
push ebx
push edi
push esi
mov esi, [esp+12+4] ; main struct
mov ebx, [esi+0]    ; stack data
sub ebx, 4
mov edi, [esi+32]   ; edi value from struct
mov edx, [esi+12]   ; data-space pointer
call [esp+12+8]
add ebx, 4
mov [ebx], eax
save_state eax
pop esi
pop edi
pop ebx
ret


m4_x86_32_engine_call_runtime_word:
save_state eax
mov ebx, esp      ; use ebx to store stack pointer misalignment
and ebx, 15       ; isolate the misalignment
xor esp, ebx      ; align the stack pointer
sub esp, 8
push esi          ; push pointer to stack struct (2nd fn param)
push dword [ecx+4]; push param                   (1st fn param)
call [ecx+0]      ; call the function
add esp, 16
or esp, ebx       ; misalign the stack pointer again
mov ebx, [esi+0]  ; get the new *data
sub ebx, 4
mov edx, [esi+12] ; get new data-space pointer
ret


m4_x86_32_engine_callback_target_0:
push 0
jmp callback_handler
m4_x86_32_engine_callback_target_1:
push 1
jmp callback_handler
m4_x86_32_engine_callback_target_2:
push 2
jmp callback_handler
m4_x86_32_engine_callback_target_3:
push 3
jmp callback_handler
m4_x86_32_engine_callback_target_4:
push 4
jmp callback_handler
m4_x86_32_engine_callback_target_5:
push 5
jmp callback_handler
m4_x86_32_engine_callback_target_6:
push 6
jmp callback_handler
m4_x86_32_engine_callback_target_7:
push 7

callback_handler:

sub esp, 8
call m4_x86_32_engine_get_global
add esp, 8
pop ecx
mov eax, [eax + 4 + ecx * 4] ; +4 to go to ctx ptr array, +ecx*4 to get our ctx
sub ecx, [eax + 44]          ; make ecx relative to our ctx's callback_array_offset

push ebx
push edi
push esi

mov esi, eax        ; struct
mov ebx, [esi+0]    ; stack data

mov eax, [esi+20]
mov al, [eax+ecx]
push eax
shr al, 1
xor edi, edi
.L0:
mov edx, [esp+16+4+edi]
mov [ebx+edi], edx
add edi, 4
dec al
jnz .L0
add ebx, edi

mov eax, [ebx-4]    ; top stack value
sub ebx, 8
mov edi, [esi+32]   ; edi value from struct
mov edx, [esi+12]   ; data-space pointer

shl ecx, 2
add ecx, [esi+24]
call [ecx]

pop ecx
test cl, 1
jnz .L1
add ebx, 4
mov [ebx], eax
.L1:

save_state ecx

pop esi
pop edi
pop ebx

ret
