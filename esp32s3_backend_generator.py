import subprocess
import os
from backend_generator import generate_builtins

"""
lr link register                     untouchable
sp stack pointer
cs ctx struct                        need persist
rs return stack - rso                would need save + restore
rb return stack base
dp data-space pointer
va variable array                    would need restore only
la literal array
   --- (save line) ---
rc runtime word function pointer     trivial movi restore
n1 constant -1
n4 constant -4
ds data stack - dso                  always save + restore anyways
dt data stack top
s0 scratch 0                         unmaintained
s1 scratch 1
s2 scratch 2

dso data stack offset
rso return stack offset


a0  lr
a1  sp
a2  cs
a3  rs
a4  rb
a5  dp
a6  va
a7  la

a8  rc
a9  n1
a10 s2
a11 s1
a12 n4
a13 ds
a14 dt
a15 s0

dso 4
rso 8

"""

builtins = (
      ("dup", (), """
addi.n ds, ds, 4
s32i.n dt, ds, dso

"""), ("-", (), """
l32i.n s0, ds, dso
sub    dt, s0, dt
add.n  ds, ds, n4

"""), (">", (), """
l32i.n s0, ds, dso
sub    s0, dt, s0
movi.n dt, 0
movltz dt, n1, s0
add.n  ds, ds, n4

"""), ("+", (), """
l32i.n s0, ds, dso
add.n  dt, s0, dt
add.n  ds, ds, n4

"""), ("=", (), """
l32i.n s0, ds, dso
sub    s0, s0, dt
movi.n dt, 0
moveqz dt, n1, s0
add.n  ds, ds, n4

"""), ("over", ("clobber", ), """
addi.n ds, ds, 4
s32i.n dt, ds, dso
l32i.n dt, ds, dso-4

"""), ("mod", (), """
l32i.n s0, ds, dso
rems   dt, s0, dt
add.n  ds, ds, n4

"""), ("drop", (), """
l32i.n dt, ds, dso
add.n  ds, ds, n4

"""), ("swap", (), """
l32i.n s0, ds, dso
s32i.n dt, ds, dso
mov.n  dt, s0

"""), ("*", (), """
l32i.n s0, ds, dso
mull   dt, s0, dt
add.n  ds, ds, n4

"""), ("allot", (), """
add.n  dp, dp, dt
l32i.n dt, ds, dso
add.n  ds, ds, n4

"""), ("1+", (), """
addi.n dt, dt, 1

"""), ("invert", (), """
xor dt, n1, dt

"""), ("0=", (), """
mov.n   s0, dt
movi.n  dt, 0
moveqz  dt, n1, s0

"""), ("i", ("clobber", ), """
addi.n ds, ds, 4
s32i.n dt, ds, dso
l32i.n dt, rs, rso

"""), ("j", ("clobber", ), """
addi.n ds, ds, 4
s32i.n dt, ds, dso
l32i.n dt, rs, rso-8

"""), ("1-", (), """
addi.n dt, dt, -1

"""), ("rot", (), """
l32i.n s0, ds, dso
s32i.n dt, ds, dso
l32i.n dt, ds, dso-4
s32i.n s0, ds, dso-4

"""), ("pick", (), """
slli   dt, dt, 2
sub    dt, ds, dt
l32i.n dt, dt, dso

"""), ("xor", (), """
l32i.n s0, ds, dso
xor    dt, s0, dt
add.n  ds, ds, n4

"""), ("lshift", (), """
l32i.n s0, ds, dso
ssl dt
sll dt, s0
add.n  ds, ds, n4

"""), ("rshift", (), """
l32i.n s0, ds, dso
ssr dt
sra dt, s0
add.n  ds, ds, n4

"""), ("<", (), """
l32i.n s0, ds, dso
sub    s0, s0, dt
movi.n dt, 0
movltz dt, n1, s0
add.n  ds, ds, n4

"""), (">=", (), """
l32i.n s0, ds, dso
sub    s0, s0, dt
movi.n dt, 0
movgez dt, n1, s0
add.n  ds, ds, n4

"""), ("<=", (), """
l32i.n s0, ds, dso
sub    s0, dt, s0
movi.n dt, 0
movgez dt, n1, s0
add.n  ds, ds, n4

"""), ("or", (), """
l32i.n s0, ds, dso
or     dt, s0, dt
add.n  ds, ds, n4

"""), ("and", (), """
l32i.n s0, ds, dso
and    dt, s0, dt
add.n  ds, ds, n4

"""), ("depth", ("clobber", ), """
addi.n ds, ds, 4
s32i.n dt, ds, dso
l32i.n dt, cs, DEPTH
sub    dt, ds, dt
addi.n dt, dt, dso
srli   dt, dt, 2

"""), ("c!", (), """
l32i.n s0, ds, dso
s8i    s0, dt, 0
add.n  ds, ds, n4
l32i.n dt, ds, dso
add.n  ds, ds, n4

"""), ("c@", (), """
l8ui  dt, dt, 0

"""), ("0<", (), """
srai  dt, dt, 31

"""), ("!", (), """
l32i.n s0, ds, dso
s32i.n s0, dt, 0
add.n  ds, ds, n4
l32i.n dt, ds, dso
add.n  ds, ds, n4

"""), ("@", (), """
l32i.n dt, dt, 0

"""), ("c,", (), """
s8i    dt, dp, 0
addi.n dp, dp, 1
l32i.n dt, ds, dso
add.n  ds, ds, n4

"""), ("here", ("clobber", ), """
addi.n ds, ds, 4
s32i.n dt, ds, dso
mov.n  dt, dp

"""), ("max", (), """
l32i.n s0, ds, dso
max    dt, s0, dt
add.n  ds, ds, n4

"""), ("2*", (), """
add.n  dt, dt, dt

"""), ("2drop", (), """
add.n  ds, ds, n4
l32i.n dt, ds, dso
add.n  ds, ds, n4

"""), ("tuck", (), """
addi.n ds, ds, 4
l32i.n s0, ds, dso-4
s32i.n s0, ds, dso
s32i.n dt, ds, dso-4

"""), ("cell+", (), """
addi.n dt, dt, 4

"""), ("align", (), """
addi.n dp, dp, 3
and    dp, dp, n4

"""), (",", (), """
s32i.n dt, dp, 0
addi.n dp, dp, 4
l32i.n dt, ds, dso
add.n  ds, ds, n4

"""), ("?dup", (), """
addi.n ds, ds, 4
s32i.n dt, ds, dso
add.n  s0, ds, n4
moveqz ds, s0, dt

"""), ("<>", (), """
l32i.n s0, ds, dso
sub    dt, s0, dt
movnez dt, n1, dt
add.n  ds, ds, n4

"""), (">r", (), """
addi.n rs, rs, 4
s32i.n dt, rs, rso
l32i.n dt, ds, dso
add.n  ds, ds, n4

"""), ("r>", ("clobber", ), """
addi.n ds, ds, 4
s32i.n dt, ds, dso
l32i.n dt, rs, rso
add.n  rs, rs, n4

"""), ("2>r", (), """
addi.n rs, rs, 8
l32i.n s0, ds, dso
s32i.n s0, rs, rso-4
s32i.n dt, rs, rso
add.n  ds, ds, n4
l32i.n dt, ds, dso
add.n  ds, ds, n4

"""), ("2r>", ("clobber", ), """
addi.n ds, ds, 4
s32i.n dt, ds, dso
addi.n ds, ds, 4
l32i.n dt, rs, rso-4
s32i.n dt, ds, dso
l32i.n dt, rs, rso
addi   rs, rs, -8

"""), ("2/", (), """
srai  dt, dt, 1

"""), ("2dup", (), """
addi.n ds, ds, 4
s32i.n dt, ds, dso
l32i.n s0, ds, dso-4
addi.n ds, ds, 4
s32i.n s0, ds, dso

"""), ("0<>", (), """
movnez dt, n1, dt

"""), ("0>", (), """
srai  s0, dt, 31
sub   dt, s0, dt
srai  dt, dt, 31

"""), ("w@", (), """
l16ui dt, dt, 0

"""), ("w!", (), """
l32i.n s0, ds, dso
s16i   s0, dt, 0
add.n  ds, ds, n4
l32i.n dt, ds, dso
add.n  ds, ds, n4

"""), ("+!", (), """
l32i.n s0, ds, dso
l32i.n s1, dt, 0
add.n  s0, s1, s0
s32i.n s0, dt, 0
add.n  ds, ds, n4
l32i.n dt, ds, dso
add.n  ds, ds, n4

"""), ("/", (), """
l32i.n s0, ds, dso
quos   dt, s0, dt
add.n  ds, ds, n4

"""), ("cells", (), """
slli dt, dt, 2

"""), ("nip", (), """
add.n  ds, ds, n4

"""), ("min", (), """
l32i.n s0, ds, dso
min    dt, s0, dt
add.n  ds, ds, n4

"""), ("abs", (), """
abs    dt, dt

"""), ("r@", ("clobber", ), """
addi.n ds, ds, 4
s32i.n dt, ds, dso
l32i.n dt, rs, rso

""")
)

elidables = (
    ("addi.n ds, ds, 4",     r" *addi\.n +ds *, *ds *, *4 *"),
    ("s32i.n dt, ds, dso",   r" *s32i\.n +dt *, *ds *, *dso *"),
    ("l32i.n dt, ds, dso",   r" *l32i\.n +dt *, *ds *, *dso *"),
    ("add.n  ds, ds, n4",    r" *add\.n +ds *, *ds *, *n4 *"),
)

def src_to_code(src):
    toolchain_bin_path = os.environ.get("M4_ESP32S3_TOOLCHAIN_PATH", default="")
    preprocessor = os.path.join(toolchain_bin_path, "xtensa-esp32s3-elf-cpp")
    assembler = os.path.join(toolchain_bin_path, "xtensa-esp32s3-elf-as")
    objcopy = os.path.join(toolchain_bin_path, "xtensa-esp32s3-elf-objcopy")

    src_path = "tmp.S"
    pp_path = "tmp.s"
    elf_path = "tmp.elf"
    bin_path = "tmp.bin"

    with open(src_path, "w") as f:
        f.write('#define M4_ESP32S3_DEFINE_REG_ALIASES\n'
                '#include "esp32s3.h"\n')
        f.write(src)

    with open(pp_path, "wb") as f:
        subprocess.check_call((preprocessor, src_path), stdout=f)

    subprocess.check_call((assembler, "--no-transform", pp_path, "-o", elf_path))

    subprocess.check_call((objcopy, "-O", "binary", elf_path, bin_path))

    with open(bin_path, "rb") as f:
        code = f.read()

    os.unlink(src_path)
    os.unlink(pp_path)
    os.unlink(elf_path)
    os.unlink(bin_path)

    return code


def main():
    generate_builtins("esp32s3", builtins, elidables, src_to_code)

if __name__ == "__main__":
    main()
