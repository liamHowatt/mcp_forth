; this file is to manually kept in sync
; with x86-32_backend.c

; defined word
call 42 ; e8 xx xx xx xx

; runtime word
; add ebx, 4
; mov [ebx], eax
mov ecx, [edi+(42+1)*4]
call [esi+36]
; mov eax, [ebx]
; sub ebx, 4

; push literal
; add ebx, 4
; mov [ebx], eax
mov eax, 42

; exit word
leave
ret

; defined word location
push ebp
mov ebp, esp

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
sub ebx, 4
; mov eax, [ebx]
; sub ebx, 4

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