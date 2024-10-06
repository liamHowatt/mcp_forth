# mcp_forth

`gcc-multilib` needs to be installed (unless OS is 32 bit) to compile `execute-exe` as 32 bit.

```sh
make
make test-simple
```

How to run `life.fs` and the hash_xors

```sh
./compile-and-run.sh forth_programs/life/life.fs < forth_programs/life/starting_board.txt
./compile-and-run.sh forth_programs/hash_xor/hash_xor.fs < forth_programs/hash_xor/hash_input.txt
```
