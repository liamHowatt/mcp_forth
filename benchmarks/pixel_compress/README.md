The results of this benchmark are in the root README.md

This test compresses a pixel buffer and outputs
the to stdout. the test is run 1000 times
redundantly, only outputting once.

The C code is based on this: https://bitbucket.org/4DPi/gen4-hats/src/236d9c064b06640f6a33be649ddd0aae147e0239/4d-hats.c#lines-781:830

```sh
gcc -Wall -O3 c.c -o c
time ./c < ~/Downloads/rgb565data.bin > res_c
```

the Forth version reads the input file internally
from a hardcoded path.

```sh
time gforth fs.fs -e bye > res_gforth
```

In the Makefile entry for `execute-exe`, replace
`-fsanitize=address -g` with `-O3`.

```sh
../../compile-exe vm fs.fs fs.vm.bin
time ../../execute-exe vm fs.vm.bin > res_m4_vm

../../compile-exe x86 fs.fs fs.x86.bin
time ../../execute-exe x86 fs.x86.bin > res_m4_x86
```

assert all the outputs are the same.

```sh
cmp res_c res_gforth
cmp res_c res_m4_vm
cmp res_c res_m4_x86
```
