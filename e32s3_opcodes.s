// this file is to be manually kept in sync
// with e32s3_backend.c

// defined word
call0 42

// runtime word
// add ebx, 4
// mov [ebx], eax
movi a8, 42
callx0 a6
// mov eax, [ebx]
// sub ebx, 4

// push literal
// add ebx, 4
// mov [ebx], eax
mov eax, 42

// exit word
leave  // sometimes omitted
ret

// defined word location
// (nothing)
// or
addi
push ebp       // sometimes omitted
mov ebp, esp   // |

// push data address
// add ebx, 4
// mov [ebx], eax
mov eax, [esi+16]
add eax, 42

// declare variable
add edx, 3  // align data-space pointer
and edx, ~3
mov [edi+(-42)*4], edx
add edx, 4

// use variable or constant
// add ebx, 4
// mov [ebx], eax
mov eax, [edi+(-42)*4]

// halt
ret

// branch location
// (empty)

// branch if zero
mov a8, a2
l32i a2, a3, 0
addi a3, a3, -4
beqz a8, 42

// branch
j 42

// do
push dword [ebx]
push eax
sub ebx, 4
// mov eax, [ebx]
// sub ebx, 4

// loop
pop ecx
inc ecx
cmp ecx, [esp]
push ecx
jl 42
add esp, 8

// push offset address
// add ebx, 4
// mov [ebx], eax
call .L0
.L0:
pop eax
add eax, 42

// push callback
// add ebx, 4
// mov [ebx], eax
mov eax, [esi+40]
mov eax, [eax+42]

// declare constant
mov [edi+(-42)*4], eax
// mov eax, [ebx]
// sub ebx, 4
