#include <fcntl.h>
#include <unistd.h>
#include "mcp_forth.h"

int main(int argc, char ** argv)
{
    assert(argc == 4);

    const mcp_forth_backend_t * backend;
    if(0 == strcmp("vm", argv[1])) {
        backend = &compact_bytecode_vm_backend;
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
    int bin_len = mcp_forth_compile(source, source_len, &bin, backend, &error_near);
    free(source);
    if(bin_len < 0) {
        printf("error %d near %d\n", bin_len, error_near);
        return 1;
    }

    fd = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0664);
    assert(fd != -1);
    res = write(fd, bin, bin_len);
    assert(res == bin_len);
    free(bin);

    res = close(fd);
    assert(res != -1);
}
