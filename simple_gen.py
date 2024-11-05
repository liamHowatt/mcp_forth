from x86_32_backend_generator import assemble
from x86_32_backend_generator import non_instr_lines
from x86_32_backend_generator import fmt_machine_code_inner
import sys

def main():
    with open(sys.argv[1]) as f:
        asm = f.read()
    instr = assemble(asm)
    asm = asm.splitlines()
    pairs = tuple(non_instr_lines(instr, asm))
    ipad = max(len(ins) for ins, _ in pairs) * 6
    apad = max(len(a) for _, a in pairs)
    for ins, a in pairs:
        fmt_machine_code_inner(ins, a, None, ipad, apad, None)

if __name__ == "__main__":
    main()

