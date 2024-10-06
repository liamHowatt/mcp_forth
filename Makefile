all: compile-exe execute-exe

compile-exe: compile.c compile-exe.c num.c vm_backend.c
	gcc -Wall -fsanitize=address -g compile.c compile-exe.c num.c vm_backend.c -o compile-exe

execute-exe: execute.c execute-exe.c num.c vm_engine.c
	gcc -m32 -Wall -fsanitize=address -g execute.c execute-exe.c num.c vm_engine.c -o execute-exe

test-simple: all
	find forth_programs/simple -maxdepth 1 -type f | xargs -I{} ./compile-and-run.sh {}

clean:
	rm -f compile-exe execute-exe *.bin
