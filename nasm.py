from subprocess import check_output, check_call
import sys

def mktemp():
    return check_output(("mktemp", )).strip()

def rm(path):
    check_call(("rm", path))

def nasm(asm, dbg=False):
    in_tmpf = mktemp()
    out_tmpf = mktemp()
    try:
        with open(in_tmpf, "w") as f:
            f.write("[BITS 32]\n")
            f.write(asm)
        if not dbg:
            check_call(("nasm", "-f", "bin", "-o", out_tmpf, in_tmpf))
            return check_output(("ndisasm", "-b", "32", out_tmpf))
        else:
            check_call(("nasm", "-g", "-f", "elf32", "-o", out_tmpf, in_tmpf), )
            return check_output(("objdump", "-d", "-S", "-Mintel", out_tmpf))
    finally:
        rm(in_tmpf)
        rm(out_tmpf)

def main():
    assert len(sys.argv) >= 2
    with open(sys.argv[1]) as f:
        asm = f.read()
    print(nasm(asm, dbg=True).decode(), end="")

if __name__ == "__main__":
    main()
