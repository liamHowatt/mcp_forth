from subprocess import check_output, check_call
import sys

# ; 2drop
# mov eax, [ebx-4]
# sub ebx, 8

builtins = (
      ("dup", """
add ebx, 4
mov [ebx], eax

"""), ("-", """
mov edx, eax
mov eax, [ebx]
sub eax, edx
sub ebx, 4

"""), (">", """
cmp [ebx], eax
setng al
movzx eax, al
dec eax
sub ebx, 4

"""), ("+", """
add eax, [ebx]
sub ebx, 4

"""), ("=", """
cmp [ebx], eax
setne al
movzx eax, al
dec eax
sub ebx, 4

"""), ("over", """
add ebx, 4
mov [ebx], eax
mov eax, [ebx-4]

"""), ("mod", """
mov  ecx, eax
mov  eax, [ebx]
cdq
idiv ecx
mov  eax, edx
sub  ebx, 4

"""), ("drop", """
mov eax, [ebx]
sub ebx, 4

"""), ("swap", """
xchg [ebx], eax

"""), ("*", """
imul eax, [ebx]
sub ebx, 4

"""), ("allot", """
ud2 ; TODO

"""), ("1+", """
inc eax

"""), ("invert", """
not eax

"""), ("0=", """
cmp eax, 1   ; from C dissasembly of
sbb eax, eax ; x == 0 ? -1 : 0

"""), ("i", """
ud2 ; TODO

"""), ("j", """
ud2 ; TODO

"""), ("1-", """
dec eax

"""), ("rot", """
ud2 ; TODO

"""), ("pick", """
not eax
mov eax, [ebx+eax*4]

"""), ("xor", """
xor eax, [ebx]
sub ebx, 4

"""), ("lshift", """
mov ecx, eax
mov eax, [ebx]
shl eax, cl
sub ebx, 4
       
"""), ("rshift", """
mov ecx, eax
mov eax, [ebx]
shr eax, cl
sub ebx, 4





""")
)

def mktemp():
    return check_output(("mktemp", )).strip()

def rm(path):
    check_call(("rm", path))

def fmt(num, name, instr, asm, igmax, agmax):
    print(f"        case {num}: /* {name} */")
    ipad = igmax * 4 + 2
    apad = agmax
    begin = "            EB_HELPER("
    for i, (ins, a) in enumerate(zip(instr, asm)):
        s1 = ('"' + ''.join("\\x" + hex(b)[2:].zfill(2) for b in ins) + '"').ljust(ipad)
        s2 = a.ljust(apad)
        end = ");" if i == len(instr) - 1 else ""
        print(f'{begin}{s1} /* {s2}  */{end}')

        begin = " " * len(begin)

def main():
    if len(sys.argv) > 1:
        bi_name = sys.argv[1]
        todo = (next((i, *bi) for i, bi in enumerate(builtins) if bi[0] == bi_name), )
    else:
        todo = tuple((i, *bi) for i, bi in enumerate(builtins))

    in_tmpf = mktemp()
    out_tmpf = mktemp()
    try:
        results = []
        for i, bi_name, asm in todo:
            with open(in_tmpf, "w") as f:
                f.write("[BITS 32]\n")
                f.write(asm)
            check_call(("nasm", "-f", "bin", "-o", out_tmpf, in_tmpf))
            disasm = check_output(("ndisasm", "-b", "32", out_tmpf))
            instr = tuple(bytes.fromhex(line.split(maxsplit=2)[1].decode())
                          for line in disasm.splitlines())
            results.append((i, bi_name, asm, instr, asm.lstrip().splitlines()))

        igmax = max(max(map(len, ins)) for *_, ins, _ in results)
        agmax = max(max(map(len, a)) for *_, a in results)
        for i, bi_name, raw_asm, instr, asm in results:
            fmt(i, bi_name, instr, asm, igmax, agmax)
    finally:
        rm(in_tmpf)
        rm(out_tmpf)




if __name__ == "__main__":
    main()
