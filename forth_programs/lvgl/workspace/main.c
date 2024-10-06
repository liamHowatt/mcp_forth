#include "lvgl/lvgl.h"
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "mcp_forth.h"
#include "lvgl_forth_runtime.h"

static_assert(sizeof(int) == sizeof(void *), "expected a 32 bit system");

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

static void file_explorer_event_handler(lv_event_t * e)
{
    lv_obj_t * file_explorer = lv_event_get_target(e);
    char * path = full_file_name(file_explorer);
    LV_LOG_USER("Running Forth program \"%s\"", path);
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
    bool simple = 0 == strcmp("A:/simple/", lv_file_explorer_get_current_path(file_explorer));
    /* if it's a simple program with no graphics, don't clear the screen */
    if(!simple) {
        lv_obj_clean(lv_screen_active());
    }
    int res = lvgl_forth_run_binary(binary, binary_len, &compact_bytecode_vm_engine);
    free(binary);
    if(res) {
        if(res > 0) {
            fprintf(stderr, "forth engine error code %d near %d\n", res, error_near);
        }
        else {
            fprintf(stderr, "forth LVGL runtime error code %d near %d\n", res, error_near);
        }
        exit(1);
    }
    if(simple) {
        /* print a newline in case the program didn't */
        puts("");
    }
}

static void ui(void)
{
    lv_obj_t * file_explorer = lv_file_explorer_create(lv_screen_active());
    lv_file_explorer_open_dir(file_explorer, "A:");
    lv_obj_add_event_cb(file_explorer, file_explorer_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
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

    ui();

    while(1) {
        uint32_t ms_delay = lv_timer_handler();
        usleep(ms_delay * 1000);
    }
}
