[BITS 32]

%include "mcp_forth_generated.mac"

extern m4_global_get_ctx
extern m4_lit

global m4_x86_32_engine_run_asm
global m4_x86_32_engine_call_runtime_word
%assign i 0
%rep M4_MAX_CALLBACKS
global m4_x86_32_engine_callback_target_%+i
%assign i i+1
%endrep


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
mov eax, [ebx]
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
add ebx, 4
mov [ebx], eax
lea eax, [rel m4_lit]
cmp dword [ecx+0], eax
jne .L0
mov eax, [ecx+4]
ret
.L0:
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
sub ebx, 8
mov edx, [esi+12] ; get new data-space pointer
mov eax, [ebx+4]
ret


%assign i 0
%rep M4_MAX_CALLBACKS
m4_x86_32_engine_callback_target_%+i:
mov ecx, i
jmp callback_handler
%assign i i+1
%endrep

callback_handler:

push ebx  ; save caller's ebx so we can save our ecx in it for the next call

mov ebx, ecx
sub esp, 4
push ecx                     ; pass ecx as param
call m4_global_get_ctx
add esp, 8
mov ecx, ebx                 ; get the ecx back from ebx

push edi
push esi

mov esi, eax        ; struct
mov ebx, [esi+0]    ; stack data

mov eax, [esi+20]
movzx eax, byte [eax+ecx]
push eax
shr eax, 1
xor edi, edi
.L0:
mov edx, [esp+16+4+edi]
mov [ebx+edi], edx
add edi, 4
dec eax
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
test ecx, 1
jnz .L1
add ebx, 4
mov [ebx], eax
.L1:

save_state ecx

pop esi
pop edi
pop ebx

ret
