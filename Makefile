all: compile-exe execute-exe

x86-32_backend_generated.c: x86_32_backend_generator.py nasm.py
	python3 x86_32_backend_generator.py

compile-exe: mcp_forth.h mcp_forth.c compile.c compile-exe.c vm_backend.c x86-32_backend.c x86-32_backend_generated.c x86-32_backend_generated.h
	gcc -m32 -Wall -fsanitize=address -g mcp_forth.c compile.c compile-exe.c vm_backend.c x86-32_backend.c x86-32_backend_generated.c -o compile-exe

x86-32_engine_asm.o: x86-32_engine_asm.s
	nasm -felf32 -o x86-32_engine_asm.o x86-32_engine_asm.s

execute-exe: mcp_forth.h mcp_forth.c execute-exe.c runtime_io.c runtime_time.c runtime_string.c runtime_process.c runtime_file.c vm_engine.c x86-32_engine.c x86-32_engine_asm.o
	gcc -m32 -Wall -fsanitize=address -g mcp_forth.c execute-exe.c runtime_io.c runtime_time.c runtime_string.c runtime_process.c runtime_file.c vm_engine.c x86-32_engine.c x86-32_engine_asm.o -o execute-exe

test-simple: all
	find forth_programs/simple -maxdepth 1 -type f | xargs -I{} ./compile-and-run.sh vm {}

test-simple-x86: all
	find forth_programs/simple -maxdepth 1 -type f | xargs -I{} ./compile-and-run.sh x86 {}

clean:
	rm -f compile-exe execute-exe x86-32_backend_generated.c x86-32_backend_generated.h x86-32_engine_asm.o
