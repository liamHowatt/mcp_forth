import sys
from nasm import nasm
import re

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
    44  memset
    48  memmove
    52  compare
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
xchg eax, [ebx]

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
xchg eax, [ebx]
xchg eax, [ebx-4]

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

"""), ("fill", (), """
push edx
mov ecx, esp
and esp, ~15
push ecx
push dword [ebx]
push eax
push dword [ebx-4]
call [esi+44]
mov esp, [esp+12]
pop edx
sub ebx, 8
mov eax, [ebx]
sub ebx, 4

"""), ("2drop", (), """
sub ebx, 4
mov eax, [ebx]
sub ebx, 4

"""), ("move", (), """
push edx
mov ecx, esp
and esp, ~15
push ecx
push eax
push dword [ebx-4]
push dword [ebx]
call [esi+48]
mov esp, [esp+12]
pop edx
sub ebx, 8
mov eax, [ebx]
sub ebx, 4

"""), ("execute", (), """
mov ecx, eax
mov eax, [ebx]
sub ebx, 4
call ecx

"""), ("align", (), """
add edx, 3
and edx, ~3

"""), (",", (), """
mov [edx], eax
add edx, 4
mov eax, [ebx]
sub ebx, 4

"""), ("compare", (), """
push edx
mov ecx, esp
and esp, ~15
push ecx
push eax
push dword [ebx]
push dword [ebx-4]
push dword [ebx-8]
call [esi+52]
mov esp, [esp+16]
pop edx
sub ebx, 8
sub ebx, 4

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

""")
)

ADD4 = 0
MVBA = 1
SUB4 = 2
MVAB = 3
CLOB = 4

def assemble(asm):
    disasm = nasm(asm)
    instr = tuple(bytes.fromhex(line.split(maxsplit=2)[1].decode())
                  for line in disasm.splitlines())
    return instr

class Chore:
    def __init__(self, normal, regex):
        assert None is not re.fullmatch(regex, normal)
        self.normal = normal
        self.regex = regex
        instr = assemble(normal)
        assert len(instr) == 1
        self.ins = instr[0]

    def two_way_match(self, ins, a):
        if self.ins == ins:
            assert None is not re.fullmatch(self.regex, a)
            return True
        assert None is re.fullmatch(self.regex, a)
        return False

flag_macros = (
    "M4_X86_32_ADD4",
    "M4_X86_32_MVBA",
    "M4_X86_32_SUB4",
    "M4_X86_32_MVAB",
    "M4_X86_32_CLOB",
)

stack_chores = (
    Chore(
        "add ebx, 4",
        r"^\s*add\s+ebx\s*,\s*4\s*(;.*|$)",
    ),
    Chore(
        "mov [ebx], eax",
        r"^\s*mov\s*\[\s*ebx\s*\]\s*,\s*eax\s*(;.*|$)",
    ),
    Chore(
        "sub ebx, 4",
        r"^\s*sub\s+ebx\s*,\s*4\s*(;.*|$)",
    ),
    Chore(
        "mov eax, [ebx]",
        r"^\s*mov\s+eax\s*,\s*\[\s*ebx\s*\]\s*(;.*|$)",
    ),
)

def flag(x):
    return 1 << x

def non_instr_lines(instr, asm):
    ins_it = iter(instr)
    a_it = iter(asm)
    while True:
        while True:
            try:
                a = next(a_it)
            except StopIteration:
                try:
                    next(ins_it)
                except StopIteration:
                    return
                assert False
            spl = a.split()
            if not spl or spl[0][0] in ";.":
                yield b'', a
                continue
            break
        ins = next(ins_it)
        yield ins, a

def prune_instr_and_get_flags(instr, asm, opts):
    lines = [list(pair) for pair in non_instr_lines(instr, asm)]
    flags = 0
    for i in range(len(lines)):
        ins, a = lines[i]
        if not ins:
            continue
        if stack_chores[ADD4].two_way_match(ins, a):
            flags |= flag(ADD4)
            lines[i][0] = b''
            i += 1
            if i < len(lines) and stack_chores[MVBA].two_way_match(*lines[i]):
                lines[i][0] = b''
                flags |= flag(MVBA)
        break
    for i in reversed(range(len(lines))):
        ins, a = lines[i]
        if not ins:
            continue
        if stack_chores[SUB4].two_way_match(ins, a):
            flags |= flag(SUB4)
            lines[i][0] = b''
            i -= 1
            if i >= 0 and stack_chores[MVAB].two_way_match(*lines[i]):
                lines[i][0] = b''
                flags |= flag(MVAB)
        break

    for opt in opts:
        assert opt == "clobber" and (flags & flag(MVBA)) and not (flags & flag(CLOB))
        flags |= flag(CLOB)

    return tuple(ins for ins, a in lines), tuple(a for ins, a in lines), flags

def fmt_machine_code_inner(ins, a, s1, ipad, apad, f):
    if s1 is None:
        s1 = (', '.join("0x" + hex(b)[2:].zfill(2) for b in ins) + ("," if ins else '')).ljust(ipad if ipad is not None else 0)
    s2 = a.ljust(apad if apad is not None else 0)
    print(f'    {s1} /* {s2}  */', file=f)

def fmt_machine_code(num, name, instr, asm, flags, igmax, agmax, f):
    if flags & flag(CLOB):
        print(f"/* {name} */ /* clobber */", file=f)
    else:
        print(f"/* {name} */", file=f)
    s_omitted = "/* omitted */ "
    ipad = max(igmax * 6, len(s_omitted))
    apad = agmax
    for ins, a in zip(instr, asm):
        if not ins and any(None is not re.fullmatch(ch.regex, a) for ch in stack_chores):
            s1 = s_omitted.ljust(ipad)
        else:
            s1 = None
        fmt_machine_code_inner(ins, a, s1, ipad, apad, f)

def table(results, f):
    cumulative = 0
    for i, bi_name, instr, asm, flags in results:
        n_bytes = sum(map(len, instr))
        pos = (str(cumulative) + ",").ljust(6)
        length = (str(n_bytes) + ",").ljust(6)
        s_flags = " | ".join(macro for i, macro in enumerate(flag_macros) if flag(i) & flags)
        print(f"    {{{pos} {length}{s_flags}}},", file=f)
        cumulative += n_bytes

def main():
    results = []
    for i, (bi_name, opts, asm) in enumerate(builtins):
        instr = assemble(asm)
        asm = tuple(asm.splitlines())
        instr, asm2, flags = prune_instr_and_get_flags(instr, asm, opts)
        assert asm == asm2
        results.append((i, bi_name, instr, asm, flags))

    total_machine_code_bytes = sum(sum(map(len, instr)) for _, _, instr, _, _ in results)

    with open("x86-32_backend_generated.c", "w") as f:
        print(f"""/* generated by {sys.argv[0]} DO NOT EDIT */
#include "x86-32_backend_generated.h"

const uint8_t m4_x86_32_backend_code[{total_machine_code_bytes}] = {{""", file=f)
        igmax = max(max(map(len, instr)) for _, _, instr, _, _ in results)
        agmax = max(max(map(len, asm)) for _, _, _, asm, _ in results)
        for i, bi_name, instr, asm, flags in results:
            fmt_machine_code(i, bi_name, instr, asm, flags, igmax, agmax, f)

        print("""};
""", file=f)

        for i, macro in zip(range(4), flag_macros):
            print(f"""const uint8_t {macro.lower()}[{len(stack_chores[i].ins)}] = {{""", file=f)
            fmt_machine_code_inner(stack_chores[i].ins, stack_chores[i].normal, None, None, None, f)
            print("};", file=f)

        print(f"""
const struct m4_x86_32_backend_table_entry m4_x86_32_backend_table[{len(builtins)}] = {{""", file=f)
        table(results, f)
        print("};", file=f)

    newline = "\n"
    with open("x86-32_backend_generated.h", "w") as f:
        print(f"""/* generated by {sys.argv[0]} DO NOT EDIT */
#pragma once
#include <stdint.h>

{newline.join(f'#define {macro} {hex(flag(i))}' for i, macro in enumerate(flag_macros))}

{newline.join(f"extern const uint8_t {macro.lower()}[{len(stack_chores[i].ins)}];" for i, macro in zip(range(4), flag_macros))}

extern const uint8_t m4_x86_32_backend_code[{total_machine_code_bytes}];
struct m4_x86_32_backend_table_entry {{uint16_t pos; uint8_t len; uint8_t flags;}};
extern const struct m4_x86_32_backend_table_entry m4_x86_32_backend_table[{len(builtins)}];""", file=f)

if __name__ == "__main__":
    main()
