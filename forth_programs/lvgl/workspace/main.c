#include "lvgl/lvgl.h"
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "mcp_forth.h"
#include "lvgl_forth_runtime.h"

static_assert(sizeof(int) == sizeof(void *), "expected a 32 bit system");

#define LOOP_WORD "\n__lvgl_timer_loop"

typedef struct {
    uint8_t * binary;
    int binary_len;
} main_user_data_t;

static char * full_file_name(lv_obj_t * file_explorer)
{
    const char * cur_path =  lv_file_explorer_get_current_path(file_explorer);
    const char * sel_fn = lv_file_explorer_get_selected_file_name(file_explorer);
    int path_sz = snprintf(NULL, 0, "%s%s", cur_path, sel_fn) + 1;
    char * path = malloc(path_sz);
    snprintf(path, path_sz, "%s%s", cur_path, sel_fn);
    return path;
}

static int load_file(char ** contents_out, char * path)
{
    lv_fs_file_t file;
    assert(lv_fs_open(&file, path, LV_FS_MODE_RD) == LV_FS_RES_OK);
    assert(lv_fs_seek(&file, 0, LV_FS_SEEK_END) == LV_FS_RES_OK);
    uint32_t pos;
    assert(lv_fs_tell(&file, &pos) == LV_FS_RES_OK);
    assert(lv_fs_seek(&file, 0, LV_FS_SEEK_SET) == LV_FS_RES_OK);
    char * contents = malloc(pos);
    uint32_t br;
    assert(lv_fs_read(&file, contents, pos, &br) == LV_FS_RES_OK);
    assert(br == pos);
    assert(lv_fs_close(&file) == LV_FS_RES_OK);
    *(char **) contents_out = contents;
    return pos;
}

static void run_binary(const uint8_t * binary, int binary_len)
{
    int res = lvgl_forth_run_binary(binary, binary_len, &compact_bytecode_vm_engine);
    if(res) {
        if(res > 0) {
            fprintf(stderr, "forth engine error code %d\n", res);
        }
        else {
            fprintf(stderr, "forth LVGL runtime error code %d\n", res);
        }
        exit(1);
    }
}

static void load_and_run_simple(lv_event_t * e)
{
    lv_obj_t * file_explorer = lv_event_get_target(e);
    char * path = full_file_name(file_explorer);
    LV_LOG_USER("Running simple Forth program \"%s\"", path);
    char * source;
    int source_len = load_file(&source, path);
    free(path);
    uint8_t * binary;
    int error_near;
    int binary_len = mcp_forth_compile(source, source_len, &binary, &compact_bytecode_vm_backend, &error_near);
    free(source);
    if(binary_len < 0) {
        fprintf(stderr, "forth compile error code %d near %d\n", binary_len, error_near);
        exit(1);
    }
    run_binary(binary, binary_len);
    free(binary);
    puts(""); /* print a newline in case the program didn't */
}

static void load_graphical(lv_event_t * e)
{
    lv_obj_t * file_explorer = lv_event_get_target(e);
    char * path = full_file_name(file_explorer);
    LV_LOG_USER("Running graphical Forth program \"%s\"", path);
    char * source;
    int source_len = load_file(&source, path);
    free(path);
    /* compile once to check validity */
    uint8_t * binary;
    int error_near;
    int binary_len = mcp_forth_compile(source, source_len, &binary, &compact_bytecode_vm_backend, &error_near);
    free(binary);
    if(binary_len < 0) {
        fprintf(stderr, "forth compile error code %d near %d\n", binary_len, error_near);
        exit(1);
    }
    /* compile again with added timer loop */
    int new_source_len = source_len + (sizeof(LOOP_WORD) - 1);
    source = realloc(source, new_source_len);
    assert(source);
    memcpy(source + source_len, LOOP_WORD, sizeof(LOOP_WORD) - 1);
    binary_len = mcp_forth_compile(source, new_source_len, &binary, &compact_bytecode_vm_backend, &error_near);
    free(source);
    assert(binary_len >= 0);

    main_user_data_t * main_data = lv_event_get_user_data(e);
    main_data->binary = binary;
    main_data->binary_len = binary_len;
}

static void file_explorer_event_handler(lv_event_t * e)
{
    lv_obj_t * file_explorer = lv_event_get_target(e);
    bool simple = 0 == strcmp("A:/simple/", lv_file_explorer_get_current_path(file_explorer));
    if(simple) load_and_run_simple(e);
    else load_graphical(e);
}

int main()
{
    lv_init();

    LV_IMAGE_DECLARE(mouse_cursor_icon);
    lv_display_t * display = lv_sdl_window_create(512, 512);
    LV_UNUSED(display);

    lv_sdl_keyboard_create();
    lv_indev_t * mouse = lv_sdl_mouse_create();
    lv_sdl_mousewheel_create();
    lv_obj_t * cursor_img = lv_image_create(lv_screen_active());
    lv_image_set_src(cursor_img, &mouse_cursor_icon);
    lv_indev_set_cursor(mouse, cursor_img);

    main_user_data_t data = {.binary=NULL};
    lv_obj_t * file_explorer = lv_file_explorer_create(lv_screen_active());
    lv_file_explorer_open_dir(file_explorer, "A:");
    lv_obj_add_event_cb(file_explorer, file_explorer_event_handler, LV_EVENT_VALUE_CHANGED, &data);

    while(data.binary == NULL) {
        uint32_t ms_delay = lv_timer_handler();
        usleep(ms_delay * 1000);
    }

    lv_obj_clean(lv_screen_active());
    run_binary(data.binary, data.binary_len);
    free(data.binary);
    assert(0); /* the program should not have exited */
}
