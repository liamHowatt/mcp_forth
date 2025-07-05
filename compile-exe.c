#include "mcp_forth.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char ** argv)
{
    assert(argc == 4);

    const m4_backend_t * backend;
    bool make_elf = false;
    m4_arch_t arch;
    if(0 == strcmp("vm", argv[1])) {
        backend = &m4_compact_bytecode_vm_backend;
    } else if(0 == strcmp("x86", argv[1])) {
        backend = &m4_x86_32_backend;
        make_elf = true;
        arch = M4_ARCH_X86_32;
    } else {
        assert(0);
    }

    int fd;
    int res;

    fd = open(argv[2], O_RDONLY);
    assert(fd != -1);
    int source_len = lseek(fd, 0, SEEK_END);
    assert(source_len != -1);
    res = lseek(fd, 0, SEEK_SET);
    assert(res != -1);

    char * source = malloc(source_len);
    assert(source);

    res = read(fd, source, source_len);
    assert(res == source_len);

    res = close(fd);
    assert(res != -1);

    uint8_t * bin;
    int error_near = -1;
    int bin_len = m4_compile(source, source_len, &bin, backend, &error_near);
    free(source);
    if(bin_len < 0) {
        printf("error %d near %d\n", bin_len, error_near);
        return 1;
    }

    int elf_len;
    void * elf;
    if(make_elf) {
        elf_len = m4_elf_linux_size();
        elf = malloc(elf_len);
        assert(elf);
        m4_elf_linux(elf, arch, bin_len);
    }

    fd = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0664);
    assert(fd != -1);
    if(make_elf) {
        res = write(fd, elf, elf_len);
        assert(res == elf_len);
        free(elf);
    }
    res = write(fd, bin, bin_len);
    assert(res == bin_len);
    free(bin);

    res = close(fd);
    assert(res != -1);
}
