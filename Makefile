all: compile-exe execute-exe num_test

mcp_forth_generated.h: mcp_forth_generated.phony
mcp_forth_generated.mac: mcp_forth_generated.phony

mcp_forth_generated.phony: mcp_forth_generator.py
	python3 mcp_forth_generator.py
	touch mcp_forth_generated.phony

mcp_forth.h: mcp_forth_generated.h
mcp_forth.c: mcp_forth_generated.h
x86-32_engine_asm.s: mcp_forth_generated.mac
x86_32_backend_generator.py: backend_generator.py

x86_32_backend_generated.h: x86_32_backend_generated.phony
x86_32_backend_generated.c: x86_32_backend_generated.phony

x86_32_backend_generated.phony: x86_32_backend_generator.py
	python3 x86_32_backend_generator.py
	touch x86_32_backend_generated.phony

compile-exe: mcp_forth.h mcp_forth.c compile.c compile-exe.c vm_backend.c x86-32_backend.c x86_32_backend_generated.c x86_32_backend_generated.h elf_linux.c
	gcc -m32 -Wall -fsanitize=address -g mcp_forth.c compile.c compile-exe.c vm_backend.c x86-32_backend.c x86_32_backend_generated.c elf_linux.c -o compile-exe

x86-32_engine_asm.o: x86-32_engine_asm.s
	nasm -felf32 -g -o x86-32_engine_asm.o x86-32_engine_asm.s

execute-exe: mcp_forth.h mcp_forth.c mcp_forth_generated.h global.h execute-exe.c runtime_io.c runtime_time.c runtime_string.c runtime_process.c runtime_file.c runtime_assert.c vm_engine.c x86-32_engine.c x86-32_engine_asm.o
	gcc -m32 -no-pie -Wall -fsanitize=address -g mcp_forth.c execute-exe.c runtime_io.c runtime_time.c runtime_string.c runtime_process.c runtime_file.c runtime_assert.c runtime_threadutil.c vm_engine.c x86-32_engine.c x86-32_engine_asm.o -lpthread -ldl -o execute-exe

test-simple: all
	find forth_programs/simple -maxdepth 1 -type f | xargs -I{} ./compile-and-run.sh vm {}

test-simple-x86: all
	find forth_programs/simple -maxdepth 1 -type f | xargs -I{} ./compile-and-run.sh x86 {}

num_test: num_test.c mcp_forth.h mcp_forth.c
	gcc -m32 -O3 -g -Wall num_test.c mcp_forth.c -o num_test

clean:
	rm -f *.phony mcp_forth_generated.* compile-exe execute-exe num_test *_backend_generated.* *.o *.bin
