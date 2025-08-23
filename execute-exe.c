#include "mcp_forth.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

static_assert(sizeof(int) == sizeof(void *), "expected a 32 bit system");

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(*(arr)))

int main(int argc, char ** argv)
{
    assert(argc == 3);

    ssize_t rwres;
    int res;

    m4_engine_run_t run_command;

    void * bin;
    void * dl_handle;
    if(0 == strcmp("vm", argv[1])) {
        run_command = m4_vm_engine_run;

        int fd = open(argv[2], O_RDONLY);
        assert(fd != -1);

        struct stat statbuf;
        res = fstat(fd, &statbuf);
        assert(res == 0);
        int bin_len = statbuf.st_size;

        bin = malloc(bin_len);
        assert(bin);
        rwres = read(fd, bin, bin_len);
        assert(rwres == bin_len);

        res = close(fd);
        assert(res != -1);
    }
    else if(0 == strcmp("x86", argv[1])) {
        run_command = m4_x86_32_engine_run;

        char * abs_path = realpath(argv[2], NULL);
        assert(abs_path);

        dl_handle = dlopen(abs_path, RTLD_NOW | RTLD_LOCAL);
        assert(dl_handle);

        free(abs_path);

        bin = dlsym(dl_handle, "cont");
        assert(bin);
    }
    else {
        assert(0);
    }

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
        NULL,
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

    if(run_command == m4_vm_engine_run) {
        free(bin);
    }
    else {
        res = dlclose(dl_handle);
        assert(res == 0);
    }

    m4_global_cleanup();
}
