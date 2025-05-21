#include "mcp_forth.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

static_assert(sizeof(int) == sizeof(void *), "expected a 32 bit system");

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(*(arr)))

int main(int argc, char ** argv)
{
    assert(argc == 3);

    int fd;
    int res;

    int (*run_command)(
        const uint8_t * bin,
        int bin_len,
        uint8_t * memory_start,
        int memory_len,
        const m4_runtime_cb_array_t ** cb_arrays,
        const char ** missing_runtime_word_dst
    );
    int mmap_prot = PROT_READ;

    if(0 == strcmp("vm", argv[1])) {
        run_command = m4_vm_engine_run;
    }
    else if(0 == strcmp("x86", argv[1])) {
        run_command = m4_x86_32_engine_run;
        mmap_prot |= PROT_EXEC;
    }
    else {
        assert(0);
    }

    fd = open(argv[2], O_RDONLY);
    assert(fd != -1);

    struct stat statbuf;
    res = fstat(fd, &statbuf);
    assert(res == 0);
    int bin_len = statbuf.st_size;

    uint8_t * bin = mmap(NULL, bin_len, mmap_prot, MAP_PRIVATE, fd, 0);
    assert(bin != MAP_FAILED);

    res = close(fd);
    assert(res != -1);

    const m4_runtime_cb_array_t * cbs[] = {
        m4_runtime_lib_io,
        m4_runtime_lib_time,
        m4_runtime_lib_string,
        m4_runtime_lib_process,
        m4_runtime_lib_file,
        m4_runtime_lib_assert,
        NULL
    };

    static uint8_t memory[8 * 1024 * 1024];
    const char * missing_word;
    res = run_command(
        bin,
        bin_len,
        memory,
        sizeof(memory),
        cbs,
        &missing_word
    );
    if(res == M4_RUNTIME_WORD_MISSING_ERROR) {
        fprintf(stderr, "runtime word \"%s\" missing\n", missing_word);
    }
    if(res) {
        fprintf(stderr, "engine error %d\n", res);
    }

    res = munmap(bin, bin_len);
    assert(res == 0);

    m4_vm_engine_global_cleanup();
    m4_x86_32_engine_global_cleanup();
}
