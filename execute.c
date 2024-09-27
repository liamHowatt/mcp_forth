#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <endian.h>

#if !defined(__BYTE_ORDER) || __BYTE_ORDER != __LITTLE_ENDIAN
#error unsupported endian
#endif

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(*(arr)))

#define STACK_COUNT 256

typedef int32_t i32;
typedef uint32_t u32;

typedef union {i32 i; u32 u;} iu_t;

typedef struct {
    iu_t * main_program;
    iu_t * defined_words;
    iu_t * defined_word_offsets;
    char * const_strings;
    iu_t * pc;
    void * end;
    iu_t ds[STACK_COUNT];
    i32 dsi;
    iu_t * rs[STACK_COUNT];
    i32 rsi;
} ctx_t;

typedef void (*cb_t)(ctx_t *);

static void push(ctx_t * ctx, u32 to_push)
{
    assert(ctx->dsi < STACK_COUNT);
    ctx->ds[ctx->dsi++].u = to_push;
}
static void pushi(ctx_t * ctx, i32 to_push)
{
    assert(ctx->dsi < STACK_COUNT);
    ctx->ds[ctx->dsi++].i = to_push;
}

static iu_t pop(ctx_t * ctx)
{
    assert(ctx->dsi);
    return ctx->ds[--ctx->dsi];
}

static void rpush(ctx_t * ctx, iu_t * to_push)
{
    assert(ctx->rsi < STACK_COUNT);
    ctx->rs[ctx->rsi++] = to_push;
}

static iu_t * rpop(ctx_t * ctx)
{
    assert(ctx->rsi);
    return ctx->rs[--ctx->rsi];
}


static void builtin_dup_cb(ctx_t * ctx)
{
    u32 to_dup = pop(ctx).u;
    push(ctx, to_dup);
    push(ctx, to_dup);
}

static void builtin_sub_cb(ctx_t * ctx)
{
    u32 right = pop(ctx).u;
    u32 left = pop(ctx).u;
    push(ctx, left - right);
}

static void builtin_gt_cb(ctx_t * ctx)
{
    i32 right = pop(ctx).i;
    i32 left = pop(ctx).i;
    pushi(ctx, left > right ? -1 : 0);
}

static void builtin_plus_cb(ctx_t * ctx)
{
    i32 right = pop(ctx).i;
    i32 left = pop(ctx).i;
    pushi(ctx, left + right);
}

static void builtin_equal_cb(ctx_t * ctx)
{
    i32 right = pop(ctx).i;
    i32 left = pop(ctx).i;
    pushi(ctx, left == right ? -1 : 0);
}

static void builtin_over_cb(ctx_t * ctx)
{
    i32 top = pop(ctx).i;
    i32 under = pop(ctx).i;
    pushi(ctx, under);
    pushi(ctx, top);
    pushi(ctx, under);
}

static void builtin_mod_cb(ctx_t * ctx)
{
    i32 right = pop(ctx).i;
    i32 left = pop(ctx).i;
    pushi(ctx, left % right);
}

static void builtin_drop_cb(ctx_t * ctx)
{
    pop(ctx);
}

static void builtin_dot_cb(ctx_t * ctx)
{
    i32 num = pop(ctx).i;
    printf("%d ", num);
}

static void builtin_cr_cb(ctx_t * ctx)
{
    puts("");
}

static void builtin_swap_cb(ctx_t * ctx)
{
    i32 top = pop(ctx).i;
    i32 under = pop(ctx).i;
    pushi(ctx, top);
    pushi(ctx, under);
}

static void builtin_mul_cb(ctx_t * ctx)
{
    i32 right = pop(ctx).i;
    i32 left = pop(ctx).i;
    pushi(ctx, left * right);
}

static const cb_t builtin_table[] = {
    builtin_dup_cb,
    builtin_sub_cb,
    builtin_gt_cb,
    builtin_plus_cb,
    builtin_equal_cb,
    builtin_over_cb,
    builtin_mod_cb,
    builtin_drop_cb,
    builtin_dot_cb,
    builtin_cr_cb,
    builtin_swap_cb,
    builtin_mul_cb
};


static void opcode_builtin_word_cb(ctx_t * ctx)
{
    builtin_table[(ctx->pc++)->u](ctx);
}

void opcode_defined_word_cb(ctx_t * ctx)
{
    u32 word_offset = (ctx->pc++)->u;
    rpush(ctx, ctx->pc);
    ctx->pc = &ctx->defined_words[ctx->defined_word_offsets[word_offset].u];
}

static void opcode_runtime_word_cb(ctx_t * ctx)
{
    assert(0);
}

static void opcode_literal_cb(ctx_t * ctx)
{
    push(ctx, (ctx->pc++)->u);
}

static void opcode_print_const_string_cb(ctx_t * ctx)
{
    printf("%s", &ctx->const_strings[(ctx->pc++)->u]);
}

void opcode_exit_word_cb(ctx_t * ctx)
{
    ctx->pc = rpop(ctx);
}


static const cb_t opcode_table[] = {
    opcode_builtin_word_cb,
    opcode_defined_word_cb,
    opcode_runtime_word_cb,
    opcode_literal_cb,
    opcode_print_const_string_cb,
    opcode_exit_word_cb
};


int main(int argc, char ** argv)
{
    assert(argc == 2);

    int res;

    int fd = open(argv[1], O_RDONLY);
    assert(fd != -1);
    int len = lseek(fd, 0, SEEK_END);
    assert(len != -1);
    // fprintf(stderr, "bytecode is %d bytes long\n", len);
    res = lseek(fd, 0, SEEK_SET);
    assert(res != -1);

    void * program = malloc(len);
    assert(program);

    res = read(fd, program, len);
    assert(res == len);

    res = close(fd);
    assert(res != -1);


    ctx_t ctx;


    for(int i = 2; i < argc; i++) {
        i32 initial;
        res = sscanf(argv[i], "%d", &initial);
        assert(res == 1);
        pushi(&ctx, initial);
    }


    i32 * p = program;
    assert(0 == memcmp("<\0\0\0", p, 4));
    p ++;

    i32 step = *p++;
    ctx.main_program = (iu_t *) p;
    p += step;

    step = *p++;
    ctx.defined_words = (iu_t *) p;
    p += step;

    step = *p++;
    ctx.defined_word_offsets = (iu_t *) p;
    p += step;

    step = *p++;
    p++;
    ctx.const_strings = (char *) p;

    char * runtime_words = &ctx.const_strings[step];
    (void)runtime_words;



    ctx.pc = (iu_t *) ctx.main_program;
    ctx.end = ctx.defined_words - 1;

    ctx.dsi = 0;
    ctx.rsi = 0;

    while(ctx.pc != ctx.end) {
        opcode_table[(ctx.pc++)->u](&ctx);
    }


    free(program);
}
