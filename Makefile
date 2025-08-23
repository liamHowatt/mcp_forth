NO_FLAGS =
# NO_FLAGS += -DM4_NO_THREAD
# NO_FLAGS += -DM4_NO_TLS

all: compile-exe execute-exe

x86_32_backend_generated.c: x86_32_backend_generator.py backend_generator.py
	python3 x86_32_backend_generator.py

compile-exe: mcp_forth.h mcp_forth.c compile.c compile-exe.c vm_backend.c x86-32_backend.c x86_32_backend_generated.c x86_32_backend_generated.h elf_linux.c
	gcc $(NO_FLAGS) -m32 -Wall -fsanitize=address -g mcp_forth.c compile.c compile-exe.c vm_backend.c x86-32_backend.c x86_32_backend_generated.c elf_linux.c -o compile-exe

x86-32_engine_asm.o: x86-32_engine_asm.s
	nasm -felf32 -g -o x86-32_engine_asm.o x86-32_engine_asm.s

execute-exe: mcp_forth.h mcp_forth.c global.h execute-exe.c runtime_io.c runtime_time.c runtime_string.c runtime_process.c runtime_file.c runtime_assert.c vm_engine.c x86-32_engine.c x86-32_engine_asm.o
	gcc $(NO_FLAGS) -m32 -no-pie -Wall -fsanitize=address -g mcp_forth.c execute-exe.c runtime_io.c runtime_time.c runtime_string.c runtime_process.c runtime_file.c runtime_assert.c runtime_threadutil.c vm_engine.c x86-32_engine.c x86-32_engine_asm.o -lpthread -ldl -o execute-exe

test-simple: all
	find forth_programs/simple -maxdepth 1 -type f | xargs -I{} ./compile-and-run.sh vm {}

test-simple-x86: all
	find forth_programs/simple -maxdepth 1 -type f | xargs -I{} ./compile-and-run.sh x86 {}

clean:
	rm -f compile-exe execute-exe x86_32_backend_generated.c x86_32_backend_generated.h x86-32_engine_asm.o
