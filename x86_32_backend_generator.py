import subprocess
import os
from backend_generator import generate_builtins

"""
eax holds the top stack element
ebx points to the stack element second from the top.
    the stack grows upwards.
edi shall point to a struct of:
        ...
        variable 2 address
        variable 1 address
     -> variable 0 address
        runtime word 0 m4_runtime_cb_pair_t ptr
        runtime word 1 m4_runtime_cb_pair_t ptr
        runtime word 2 m4_runtime_cb_pair_t ptr
        ...
esi shall point to a struct of:
        m4_stack_t
            data
    4       max
    8       len
    12  memory
    16  data
    20  callback info (uint8_t *)
    24  callback_word_locations
    28  a number that when subtracted from ebx and divided
            by 4 is the stack depth
    32  edi value
    36  runtime word function
    40  callback targets
    44  special_runtime_cbs
    48  total_extra_memory_size
edx holds the data-space pointer
"""

builtins = (
      ("dup", (), """
add ebx, 4
mov [ebx], eax

"""), ("-", (), """
neg eax
add eax, [ebx]
sub ebx, 4

"""), (">", (), """
cmp [ebx], eax
setng al
movzx eax, al
dec eax
sub ebx, 4

"""), ("+", (), """
add eax, [ebx]
sub ebx, 4

"""), ("=", (), """
cmp [ebx], eax
setne al
movzx eax, al
dec eax
sub ebx, 4

"""), ("over", ("clobber", ), """
add ebx, 4
mov [ebx], eax
mov eax, [ebx-4]

"""), ("mod", (), """
push edx
mov  ecx, eax
mov  eax, [ebx]
cdq
idiv ecx
mov  eax, edx
pop edx
sub  ebx, 4

"""), ("drop", (), """
mov eax, [ebx]
sub ebx, 4

"""), ("swap", (), """
mov ecx, [ebx]
mov [ebx], eax
mov eax, ecx

"""), ("*", (), """
imul eax, [ebx]
sub ebx, 4

"""), ("allot", (), """
add edx, eax
mov eax, [ebx]
sub ebx, 4

"""), ("1+", (), """
inc eax

"""), ("invert", (), """
not eax

"""), ("0=", (), """
cmp eax, 1   ; from C dissasembly of
sbb eax, eax ; x == 0 ? -1 : 0

"""), ("i", ("clobber", ), """
add ebx, 4
mov [ebx], eax
mov eax, [esp]

"""), ("j", ("clobber", ), """
add ebx, 4
mov [ebx], eax
mov eax, [esp+8]

"""), ("1-", (), """
dec eax

"""), ("rot", (), """
mov ecx, [ebx]
mov [ebx], eax
mov eax, [ebx-4]
mov [ebx-4], ecx

"""), ("pick", (), """
neg eax ; maybe flip the stack growth direction?
mov eax, [ebx+eax*4]

"""), ("xor", (), """
xor eax, [ebx]
sub ebx, 4

"""), ("lshift", (), """
mov ecx, eax
mov eax, [ebx]
shl eax, cl
sub ebx, 4

"""), ("rshift", (), """
mov ecx, eax
mov eax, [ebx]
shr eax, cl
sub ebx, 4

"""), ("<", (), """
cmp [ebx], eax
setnl al
movzx eax, al
dec eax
sub ebx, 4

"""), (">=", (), """
cmp [ebx], eax
setnge al
movzx eax, al
dec eax
sub ebx, 4

"""), ("<=", (), """
cmp [ebx], eax
setnle al
movzx eax, al
dec eax
sub ebx, 4

"""), ("or", (), """
or eax, [ebx]
sub ebx, 4

"""), ("and", (), """
and eax, [ebx]
sub ebx, 4

"""), ("depth", ("clobber", ), """
add ebx, 4
mov [ebx], eax
mov eax, ebx
sub eax, [esi+28]
shr eax, 2

"""), ("c!", (), """
mov ecx, [ebx]
mov [eax], cl
sub ebx, 4
mov eax, [ebx]
sub ebx, 4

"""), ("c@", (), """
movzx eax, byte [eax]

"""), ("0<", (), """
sar eax, 31

"""), ("!", (), """
mov ecx, [ebx]
mov [eax], ecx
sub ebx, 4
mov eax, [ebx]
sub ebx, 4

"""), ("@", (), """
mov eax, [eax]

"""), ("c,", (), """
mov [edx], al
inc edx
mov eax, [ebx]
sub ebx, 4

"""), ("here", ("clobber", ), """
add ebx, 4
mov [ebx], eax
mov eax, edx

"""), ("max", (), """
mov ecx, [ebx]
cmp eax, ecx
cmovl eax, ecx
sub ebx, 4

"""), ("2*", (), """
add eax, eax

"""), ("2drop", (), """
sub ebx, 4
mov eax, [ebx]
sub ebx, 4

"""), ("tuck", (), """
add ebx, 4
mov ecx, [ebx-4]
mov [ebx], ecx
mov [ebx-4], eax

"""), ("cell+", (), """
add eax, 4

"""), ("align", (), """
add edx, 3
and edx, ~3

"""), (",", (), """
mov [edx], eax
add edx, 4
mov eax, [ebx]
sub ebx, 4

"""), ("?dup", (), """
add ebx, 4
mov [ebx], eax
lea ecx, [ebx-4]
test eax, eax
cmovz ebx, ecx

"""), ("<>", (), """
cmp [ebx], eax
sete al
movzx eax, al
dec eax
sub ebx, 4

"""), (">r", (), """
push eax
mov eax, [ebx]
sub ebx, 4

"""), ("r>", ("clobber", ), """
add ebx, 4
mov [ebx], eax
pop eax

"""), ("2>r", (), """
push dword [ebx]
push eax
sub ebx, 4
mov eax, [ebx]
sub ebx, 4

"""), ("2r>", ("clobber", ), """
add ebx, 4
mov [ebx], eax
add ebx, 4
pop eax
pop dword [ebx]

"""), ("2/", (), """
sar eax, 1

"""), ("2dup", (), """
add ebx, 4
mov [ebx], eax
mov ecx, [ebx-4]
add ebx, 4
mov [ebx], ecx

"""), ("0<>", (), """
neg eax      ; from C dissasembly of
sbb eax, eax ; x != 0 ? -1 : 0

"""), ("0>", (), """
test eax, eax
setng al
movzx eax, al
dec eax

"""), ("w@", (), """
movzx eax, word [eax]

"""), ("w!", (), """
mov ecx, [ebx]
mov [eax], cx
sub ebx, 4
mov eax, [ebx]
sub ebx, 4

"""), ("+!", (), """
mov ecx, [ebx]
add [eax], ecx
sub ebx, 4
mov eax, [ebx]
sub ebx, 4

"""), ("/", (), """
push edx
mov  ecx, eax
mov  eax, [ebx]
cdq
idiv ecx
pop edx
sub  ebx, 4

"""), ("cells", (), """
sal eax, 2

"""), ("nip", (), """
sub ebx, 4

"""), ("min", (), """
mov ecx, [ebx]
cmp ecx, eax
cmovl eax, ecx
sub ebx, 4

"""), ("abs", (), """
mov ecx, eax
neg ecx
cmovns eax, ecx

"""), ("r@", ("clobber", ), """
add ebx, 4
mov [ebx], eax
mov eax, [esp]

""")
)

elidables = (
    ("add ebx, 4",      r" *add +ebx *, *4 *"),
    ("mov [ebx], eax",  r" *mov *\[ *ebx *\] *, *eax *"),
    ("mov eax, [ebx]",  r" *mov +eax *, *\[ *ebx *\] *"),
    ("sub ebx, 4",      r" *sub +ebx *, *4 *"),
)

def src_to_code(src):
    src_path = "tmp.s"
    bin_path = "tmp.bin"

    with open(src_path, "w") as f:
        f.write("[BITS 32]\n")
        f.write(src)

    subprocess.check_call(("nasm", "-f", "bin", src_path, "-o", bin_path))

    with open(bin_path, "rb") as f:
        code = f.read()

    os.unlink(src_path)
    os.unlink(bin_path)

    return code

def main():
    generate_builtins("x86_32", builtins, elidables, src_to_code)

if __name__ == "__main__":
    main()
