#include "mcp_forth.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(*(arr)))

static void rt_type(int ** stack, int stack_max, int * stack_len)
{
    assert(*stack_len >= 2);
    fwrite((void *)(*stack)[-2], 1, (*stack)[-1], stdout);
    fflush(stdout);
    *stack -= 2;
    *stack_len -= 2;
}

static void rt_cr(int ** stack, int stack_max, int * stack_len)
{
    puts("");
}

static void rt_dot(int ** stack, int stack_max, int * stack_len)
{
    assert(*stack_len >= 1);
    printf("%d ", (*stack)[-1]);
    fflush(stdout);
    *stack -= 1;
    *stack_len -= 1;
}

static void rt_key(int ** stack, int stack_max, int * stack_len)
{
    assert(*stack_len < stack_max);
    int c = getchar();
    if(c == EOF) exit(1);
    **stack = c;
    *stack += 1;
    *stack_len += 1;
}

static void rt_emit(int ** stack, int stack_max, int * stack_len)
{
    assert(*stack_len >= 1);
    putchar((*stack)[-1]);
    fflush(stdout);
    *stack -= 1;
    *stack_len -= 1;
}

static void rt_ms(int ** stack, int stack_max, int * stack_len)
{
    assert(*stack_len >= 1);
    *stack -= 1;
    *stack_len -= 1;
    unsigned long long ms = **stack;
    unsigned long long sec = ms / 1000ull;
    unsigned long long ms_no_sec = ms % 1000ull;
    unsigned long long ns_no_sec = ms_no_sec * 1000000ull;
    struct timespec tspec = {.tv_sec=sec, .tv_nsec=ns_no_sec};
    int res = nanosleep(&tspec, NULL);
    assert(res == 0);
}

static const struct {const char * name; runtime_cb cb;} rt_cbs[] = {
    {"type", rt_type},
    {"cr", rt_cr},
    {".", rt_dot},
    {"key", rt_key},
    {"emit", rt_emit},
    {"ms", rt_ms},
};

static runtime_cb get_runtime_cb(const char * name)
{
    for(int i=0; i<ARRAY_LEN(rt_cbs); i++) {
        if(0 == strcmp(name, rt_cbs[i].name)) {
            return rt_cbs[i].cb;
        }
    }
    fprintf(stderr, "no runtime word for \"%s\"\n", name);
    exit(1);
}

int main(int argc, char ** argv)
{
    assert(argc == 3);

    const mcp_forth_engine_t * engine;
    if(0 == strcmp("vm", argv[1])) {
        engine = &compact_bytecode_vm_engine;
    } else {
        assert(0);
    }

    int fd;
    int res;

    fd = open(argv[2], O_RDONLY);
    assert(fd != -1);
    int bin_len = lseek(fd, 0, SEEK_END);
    assert(bin_len != -1);
    res = lseek(fd, 0, SEEK_SET);
    assert(res != -1);

    uint8_t * bin = malloc(bin_len);
    assert(bin);

    res = read(fd, bin, bin_len);
    assert(res == bin_len);

    res = close(fd);
    assert(res != -1);

    // if(isatty(STDIN_FILENO)) {
    //     struct termios t;
    //     res = tcgetattr(STDIN_FILENO, &t); assert(res == 0);
    //     t.c_lflag &= ~(ICANON | ECHO); /* Disable canonical mode and echo */
    //     res = tcsetattr(STDIN_FILENO, TCSANOW, &t); assert(res == 0);
    // }

    res = mcp_forth_execute(bin, bin_len, get_runtime_cb, engine);

    free(bin);

    if(res) {
        fprintf(stderr, "engine error %d\n", res);
        return 1;
    }
}
