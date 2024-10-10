#include "lvgl_forth_runtime.h"
#include "lvgl/lvgl.h"
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "mcp_forth.h"

#define TOO_FEW_ARGS_ERROR -1
#define THAT_MANY_ARGS_NOT_SUPPORTED_ERROR -2
#define USER_DATA_WITH_DEFINED_WORD_CB_NOT_SUPPORTED_ERROR -3

#define ITOV(expr) (void *) expr
#define NOP(expr) expr

#define F01(fname, cvt1) static int rt_ ## fname(void * ctx, stack_t * stack) { \
    if(!(stack->len >= 1)) return STACK_UNDERFLOW_ERROR; \
    fname(cvt1(stack->data[-1])); \
    stack->data -= 1; \
    stack->len -= 1; \
    return 0; \
}
#define F02(fname, cvt1, cvt2) static int rt_ ## fname(void * ctx, stack_t * stack) { \
    if(!(stack->len >= 2)) return STACK_UNDERFLOW_ERROR; \
    fname(cvt1(stack->data[-2]), cvt2(stack->data[-1])); \
    stack->data -= 2; \
    stack->len -= 2; \
    return 0; \
}
#define F03(fname, cvt1, cvt2, cvt3) static int rt_ ## fname(void * ctx, stack_t * stack) { \
    if(!(stack->len >= 3)) return STACK_UNDERFLOW_ERROR; \
    fname(cvt1(stack->data[-3]), cvt2(stack->data[-2]), cvt3(stack->data[-1])); \
    stack->data -= 3; \
    stack->len -= 3; \
    return 0; \
}
#define F04(fname, cvt1, cvt2, cvt3, cvt4) static int rt_ ## fname(void * ctx, stack_t * stack) { \
    if(!(stack->len >= 4)) return STACK_UNDERFLOW_ERROR; \
    fname(cvt1(stack->data[-4]), cvt2(stack->data[-3]), cvt3(stack->data[-2]), cvt4(stack->data[-1])); \
    stack->data -= 4; \
    stack->len -= 4; \
    return 0; \
}
#define F10(fname) static int rt_ ## fname(void * ctx, stack_t * stack) { \
    if(!(stack->len < stack->max)) return STACK_OVERFLOW_ERROR; \
    *stack->data = (int) fname(); \
    stack->data += 1; \
    stack->len += 1; \
    return 0; \
}
#define F11(fname, cvt1) static int rt_ ## fname(void * ctx, stack_t * stack) { \
    if(!(stack->len >= 1)) return STACK_UNDERFLOW_ERROR; \
    stack->data[-1] = (int) fname(cvt1(stack->data[-1])); \
    return 0; \
}
#define F12(fname, cvt1, cvt2) static int rt_ ## fname(void * ctx, stack_t * stack) { \
    if(!(stack->len >= 2)) return STACK_UNDERFLOW_ERROR; \
    stack->data[-2] = (int) fname(cvt1(stack->data[-2]), cvt2(stack->data[-1])); \
    stack->data -= 1; \
    stack->len -= 1; \
    return 0; \
}
#define F14(fname, cvt1, cvt2, cvt3, cvt4) static int rt_ ## fname(void * ctx, stack_t * stack) { \
    if(!(stack->len >= 4)) return STACK_UNDERFLOW_ERROR; \
    stack->data[-4] = fname(cvt1(stack->data[-4]), cvt2(stack->data[-3]), cvt3(stack->data[-2]), cvt4(stack->data[-1])); \
    stack->data -= 3; \
    stack->len -= 3; \
    return 0; \
}
#define CONS(cname) static int rt_ ## cname(void * ctx, stack_t * stack) { \
    if(!(stack->len < stack->max)) return STACK_OVERFLOW_ERROR; \
    *stack->data = cname; \
    stack->data += 1; \
    stack->len += 1; \
    return 0; \
}

union iu {int i; unsigned u;};

struct cb_info;

typedef struct {
    const uint8_t * binary;
    const uint8_t * binary_end;
    const mcp_forth_engine_t * engine;
    struct cb_info ** cb_infos;
    int cb_info_count;
    stack_t * stack;
} ctx_t;

struct cb_info {
    const uint8_t * defined_word_pointer;
    ctx_t * ctx;
};

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
    if(!(stack->len < stack->max)) return STACK_OVERFLOW_ERROR;
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


static int rt_lv_color_hex(void * ctx, stack_t * stack)
{
    if(!(stack->len >= 1)) return STACK_UNDERFLOW_ERROR;
    union iu hex;
    hex.i = stack->data[-1];
    lv_color_t col = lv_color_hex(hex.u);
    memcpy(stack->data - 1, &col, sizeof(lv_color_t));
    return 0;
}

static int rt_lv_obj_set_style_bg_color(void * ctx, stack_t * stack)
{
    if(!(stack->len >= 3)) return STACK_UNDERFLOW_ERROR;
    lv_color_t col;
    memcpy(&col, stack->data - 2, sizeof(lv_color_t));
    lv_obj_set_style_bg_color((void *)stack->data[-3], col, stack->data[-1]);
    stack->data -= 3;
    stack->len -= 3;
    return 0;
}

static int rt_lv_obj_set_style_text_color(void * ctx, stack_t * stack)
{
    if(!(stack->len >= 3)) return STACK_UNDERFLOW_ERROR;
    lv_color_t col;
    memcpy(&col, stack->data - 2, sizeof(lv_color_t));
    lv_obj_set_style_text_color((void *)stack->data[-3], col, stack->data[-1]);
    stack->data -= 3;
    stack->len -= 3;
    return 0;
}

static int rt_lv_label_set_text_fmt(void * ctx, stack_t * stack)
{
    if(!(stack->len >= 1)) return STACK_UNDERFLOW_ERROR;
    int nargs = stack->data[-1];
    stack->data -= 1;
    stack->len -= 1;
    if(!(stack->len >= nargs)) return STACK_UNDERFLOW_ERROR;
    if(!(nargs >= 2)) return TOO_FEW_ARGS_ERROR;
    switch(nargs) {
        case 2:
            lv_label_set_text_fmt((void *)stack->data[-2], (void *)stack->data[-1]);
            break;
        case 3:
            lv_label_set_text_fmt((void *)stack->data[-3], (void *)stack->data[-2], stack->data[-1]);
            break;
        case 4:
            lv_label_set_text_fmt((void *)stack->data[-4], (void *)stack->data[-3], stack->data[-2], stack->data[-1]);
            break;
        case 5:
            lv_label_set_text_fmt((void *)stack->data[-5], (void *)stack->data[-4], stack->data[-3], stack->data[-2], stack->data[-1]);
            break;
        case 6:
            lv_label_set_text_fmt((void *)stack->data[-6], (void *)stack->data[-5], stack->data[-4], stack->data[-3], stack->data[-2], stack->data[-1]);
            break;
        case 7:
            lv_label_set_text_fmt((void *)stack->data[-7], (void *)stack->data[-6], stack->data[-5], stack->data[-4], stack->data[-3], stack->data[-2], stack->data[-1]);
            break;
        case 8:
            lv_label_set_text_fmt((void *)stack->data[-8], (void *)stack->data[-7], stack->data[-6], stack->data[-5], stack->data[-4], stack->data[-3], stack->data[-2], stack->data[-1]);
            break;
        default:
            return THAT_MANY_ARGS_NOT_SUPPORTED_ERROR;
    }
    stack->data -= nargs;
    stack->len -= nargs;
    return 0;
}

static void obj_add_event_cb_wrapper(lv_event_t * e)
{
    struct cb_info * info = lv_event_get_user_data(e);
    stack_t * stack = info->ctx->stack;
    assert(stack->len < stack->max);
    stack->data[0] = (int)e;
    stack->data += 1;
    stack->len += 1;
    info->ctx->engine->call_defined_word(info->defined_word_pointer, stack);
}
static int rt_lv_obj_add_event_cb(void * ctx, stack_t * stack)
{
    if(!(stack->len >= 4)) return STACK_UNDERFLOW_ERROR;
    void * cb = (void *)stack->data[-3];
    void * user_data = (void *)stack->data[-1];
    ctx_t * ctx2 = ctx;
    if(cb >= (void *)ctx2->binary && cb <= (void *)(ctx2->binary_end - 1)) {
        /* the callback is a pointer to a defined word inside the binary */
        if(user_data) return USER_DATA_WITH_DEFINED_WORD_CB_NOT_SUPPORTED_ERROR;
        for(int i = 0; i < ctx2->cb_info_count; i++) {
            if(cb == ctx2->cb_infos[i]->defined_word_pointer) {
                user_data = ctx2->cb_infos[i];
                break;
            }
        }
        if(!user_data) {
            struct cb_info * new_info = malloc(sizeof(struct cb_info));
            assert(new_info);
            new_info->defined_word_pointer = cb;
            new_info->ctx = ctx2;
            user_data = new_info;

            ctx2->cb_info_count += 1;
            ctx2->cb_infos = realloc(ctx2->cb_infos, ctx2->cb_info_count * sizeof(struct cb_info *));
            assert(ctx2->cb_infos);
            ctx2->cb_infos[ctx2->cb_info_count - 1] = new_info;
        }
        cb = obj_add_event_cb_wrapper;
    }
    ctx2->stack = stack;
    stack->data[-4] = (int)lv_obj_add_event_cb((void *)stack->data[-4], cb, stack->data[-2], user_data);
    stack->data -= 3;
    stack->len -= 3;
    return 0;
}

F10(lv_screen_active)
CONS(LV_PART_MAIN)
F11(lv_label_create, ITOV)
F02(lv_label_set_text, ITOV, ITOV)
CONS(LV_ALIGN_CENTER)
F04(lv_obj_align, ITOV, NOP, NOP, NOP)
F11(lv_event_get_code, ITOV)
F11(lv_event_get_target, ITOV)
CONS(LV_EVENT_CLICKED)
F12(lv_obj_get_child, ITOV, NOP)
F11(lv_button_create, ITOV)
F03(lv_obj_set_pos, ITOV, NOP, NOP)
F03(lv_obj_set_size, ITOV, NOP, NOP)
CONS(LV_EVENT_ALL)
F01(lv_obj_center, ITOV)

static int rt___lvgl_timer_loop(void * ctx, stack_t * stack)
{
    while(1) {
        uint32_t ms_delay = lv_timer_handler();
        usleep(ms_delay * 1000);
    }
    return 0;
}

static const runtime_cb_array_t runtime_cb_array[] = {
    {"type", rt_type},
    {"cr", rt_cr},
    {".", rt_dot},
    {"key", rt_key},
    {"emit", rt_emit},
    {"ms", rt_ms},

    {"lv_color_hex", rt_lv_color_hex},
    {"lv_obj_set_style_bg_color", rt_lv_obj_set_style_bg_color},
    {"lv_obj_set_style_text_color", rt_lv_obj_set_style_text_color},
    {"lv_label_set_text_fmt", rt_lv_label_set_text_fmt},
    {"lv_obj_add_event_cb", rt_lv_obj_add_event_cb},

    {"lv_screen_active", rt_lv_screen_active},
    {"lv_part_main", rt_LV_PART_MAIN},
    {"lv_label_create", rt_lv_label_create},
    {"lv_label_set_text", rt_lv_label_set_text},
    {"lv_align_center", rt_LV_ALIGN_CENTER},
    {"lv_obj_align", rt_lv_obj_align},
    {"lv_event_get_code", rt_lv_event_get_code},
    {"lv_event_get_target", rt_lv_event_get_target},
    {"lv_event_clicked", rt_LV_EVENT_CLICKED},
    {"lv_obj_get_child", rt_lv_obj_get_child},
    {"lv_button_create", rt_lv_button_create},
    {"lv_obj_set_pos", rt_lv_obj_set_pos},
    {"lv_obj_set_size", rt_lv_obj_set_size},
    {"lv_event_all", rt_LV_EVENT_ALL},
    {"lv_obj_center", rt_lv_obj_center},

    {"__lvgl_timer_loop", rt___lvgl_timer_loop},

    {NULL, NULL}
};

static const runtime_t lvgl_forth_runtime = {
    .runtime_cb_array=runtime_cb_array
};

int lvgl_forth_run_binary(const uint8_t * binary, int binary_len, const mcp_forth_engine_t * engine)
{
    ctx_t ctx = {.binary=binary, .binary_end=binary+binary_len, .engine=engine};
    const char * missing_word;
    int res = mcp_forth_execute(binary, binary_len, &lvgl_forth_runtime, &ctx, engine, &missing_word);
    if(res == RUNTIME_WORD_MISSING_ERROR) {
        fprintf(stderr, "runtime word \"%s\" missing\n", missing_word);
    }
    for(int i = 0; i < ctx.cb_info_count; i++) {
        free(ctx.cb_infos[i]);
    }
    free(ctx.cb_infos);
    return res;
}
