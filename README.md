# mcp_forth

`gcc-multilib` needs to be installed (unless OS is 32 bit) to compile `execute-exe` as 32 bit.

```sh
make
make test-simple
```

How to run `life.fs` and the hash_xors

```sh
./compile-and-run.sh vm forth_programs/life/life.fs < forth_programs/life/starting_board.txt
./compile-and-run.sh vm forth_programs/hash_xor/hash_xor.fs < forth_programs/hash_xor/hash_input.txt
```

## Performance Measurements

| Test                             | Gforth  | mcp-forth vm -O3 | mcp-forth x86 -O3 | C equivalent -O3 (-m32) |
| -------------------------------- | ------- | ---------------- | ----------------- | ----------------------- |
| SPI pixel data compression       | 10.263s | 1m24.360s        | 4.622s            | 0.432s (0.975s)         |

See the bemchmarks directory for the test source files.

## Why

I need portable driver code.

An imaginary display module has a Forth program in its ROM which is retrieved by the host
and executed to copy updated areas to the display. This display is not trivially controlled.
It receives a compressed stream of data over SPI. The Forth program has a function with
a host-expected signature like `( x1 y1 x2 y2 src -- )` which implements the compression
algorithm and uses host-provided facilities to achieve the SPI transmission. It needs
to be fast so mcp-forth must be capable of producing somewhat optimal machine code. Forth is
the chosen language instead of C because I need the compiler to use a minimal amount of
memory to do the compilation. The mcp-forth compiler is intended to run on hosts which are
MCUs with RAM around the range of 100 kB to 10 MB. A secondary requirement is the host cross-compiling
Forth programs to run on peripheral MCUs with as little as 8 kB of RAM.

## Supported Architectures

The bytecode VM is one of the available compiler backends for ease of development and while
having some freestanding merit such as being portable to platforms for which there isn't a
native backend yet. It will likely always boast the smallest binary size across architectures
due to using a stream of variable-length numbers as its encoding for opcodes and operands.
It is the default choice for testing new Forth code due to having over/underflow/run
checks where the other backends may forgo safety in favor speed. In summary, it is the default
choice unless speed is a requirement.

Currently supported architectures:

- Interpreted bytecode VM (explained above)
- x86-32

Planned:

- ARM Thumb (Cortex M0+)
- Xtensa (LX6, i.e. ESP32)

## 32 Bits

For simplicity, mcp-forth only supports 32 bit. The C code that implements the runtimes assumes
that both `int` and `void *` are 32 bits wide. The Forth cell size in mcp-forth is always 4 bytes. Pointers
can be transparently handled as integers. The compiler can work on 64 bit machines so cross-compiling
Forth programs on a 64 bit host is possible but running the output is only possible if
the host supports some kind of 32 bit mode where pointers are 32 bits wide. This means that
Apple M1, M2, M3, and M4 processors cannot execute the output of the mcp-forth compiler even with the
VM runtime because they have no support for 32 bit programs. An emulator such as Qemu is required.

## Non-standard Quirks

- `C' <word>` works like `' <word>` except it creates a C function pointer from the word so that
  Forth words can be used as C callback functions. The number of parameters and optional
  return value is derived from the `( -- )` signature and an error is raised at compile time
  if the signature is missing. The `( -- )` signature has no other semantic meaning besides this.
- Currently, defined words must only be used after they're defined or else a compile time
  error is raised.
- Any word that was not found at compile time is a runtime dependency and must be provided by
  the runtime.
- Gforth's "compile time only words" can be used outside of functions in mcp-forth.
- `UNLOOP` is not required and is a no-op

## Minutia

### Iterative "Fragment Solving"

Fragments aka snippets of machine code have variable sizes depending on their operands. If a jump
instruction jumps somewhere nearby, it may only use 1 byte to encode the offset, otherwise 4 bytes.
Literal values are similar. An immediate literal may be loaded into a register differently
depending on its size. Some architectures require multiple instructions to load larger immediate
literal values.

Given that the jump distance may not be known at the time of a jump fragment's creation, the
collection of all fragments at the end of compilation must be solved in an iterative way to
achieve optimal packing.

Question: will iterative solving ever cause the compiler to hang in an infinite loop that it can't solve?

### Optimizing Compiler Memory Usage

The compiler allocates a few arrays which it repeatedly appends elements to during compilation
and resizes them when their capacities are exceeded. There are no small allocations since the overhead
of N allocations of a small struct may be greater than an allocated contiguous array of N small structs.

Strings referring to source code tokens are not allocated arrays of bytes.
They are pointers into the source code and a length.

Since struct references are always being invalidated due to array resizing, struct references
are stored as array indices instead of pointers.

Question: is it a good or bad idea to reduce memory usage by:

- Storing strings as only a pointer with no length. The strings are whitespace-terminated since they
  point inside the source code. There is a special case for a string that is the last token in the
  source with no following whitespace.
- Extending the previous point, should 16 bit offsets into the source be used instead of 32 bit pointers?
- For indices into arrays of structs, should 16 bit indices be used instead of 32 bit ints?
- For a case where less-than-32-bit integer types are used to store offsets, can all the members of
  an array of offsets be dynamically promoted as needed? The first offset that exceeds 65535 would
  cause the array to be converted from an array of 16 bit offsets to an array of 32 bit offsets.
