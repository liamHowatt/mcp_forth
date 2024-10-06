#include "mcp_forth.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>

static int rt_type(void * ctx, stack_t * stack)
{
    if(!(stack->len >= 2)) return STACK_UNDERFLOW_ERROR;
    fwrite((void *)stack->data[-2], 1, stack->data[-1], stdout);
    fflush(stdout);
    stack->data -= 2;
    stack->len -= 2;
    return 0;
}

static int rt_cr(void * ctx, stack_t * stack)
{
    puts("");
    return 0;
}

static int rt_dot(void * ctx, stack_t * stack)
{
    if(!(stack->len >= 1)) return STACK_UNDERFLOW_ERROR;
    printf("%d ", stack->data[-1]);
    fflush(stdout);
    stack->data -= 1;
    stack->len -= 1;
    return 0;
}

static int rt_key(void * ctx, stack_t * stack)
{
    if(!(stack->len < stack->max)) return STACK_UNDERFLOW_ERROR;
    int c = getchar();
    if(c == EOF) exit(1);
    *stack->data = c;
    stack->data += 1;
    stack->len += 1;
    return 0;
}

static int rt_emit(void * ctx, stack_t * stack)
{
    if(!(stack->len >= 1)) return STACK_UNDERFLOW_ERROR;
    putchar(stack->data[-1]);
    fflush(stdout);
    stack->data -= 1;
    stack->len -= 1;
    return 0;
}

static int rt_ms(void * ctx, stack_t * stack)
{
    if(!(stack->len >= 1)) return STACK_UNDERFLOW_ERROR;
    stack->data -= 1;
    stack->len -= 1;
    unsigned long long ms = *stack->data;
    unsigned long long sec = ms / 1000ull;
    unsigned long long ms_no_sec = ms % 1000ull;
    unsigned long long ns_no_sec = ms_no_sec * 1000000ull;
    struct timespec tspec = {.tv_sec=sec, .tv_nsec=ns_no_sec};
    int res = nanosleep(&tspec, NULL);
    assert(res == 0);
    return 0;
}

static const runtime_cb_array_t runtime_cb_array[] = {
    {"type", rt_type},
    {"cr", rt_cr},
    {".", rt_dot},
    {"key", rt_key},
    {"emit", rt_emit},
    {"ms", rt_ms},
    {NULL, NULL},
};

static const runtime_t exe_runtime = {
    .runtime_cb_array=runtime_cb_array
};

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

    const char * missing_word;
    res = mcp_forth_execute(bin, bin_len, &exe_runtime, NULL, engine, &missing_word);
    if(res == RUNTIME_WORD_MISSING_ERROR) {
        fprintf(stderr, "runtime word \"%s\" missing\n", missing_word);
    }
    free(bin);

    if(res) {
        fprintf(stderr, "engine error %d\n", res);
        return 1;
    }
}
