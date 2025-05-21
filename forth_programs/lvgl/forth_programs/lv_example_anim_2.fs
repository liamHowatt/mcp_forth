: anim_x_cb ( var v -- )
    lv_obj_set_x
;

: anim_size_cb ( var v -- )
    dup lv_obj_set_size
;

: wrapper_lv_anim_path_ease_in_out ( a -- i )
    lv_anim_path_ease_in_out
;

lv_screen_active lv_obj_create
dup LV_PALETTE_RED lv_palette_main 0 lv_obj_set_style_bg_color
dup LV_RADIUS_CIRCLE 0 lv_obj_set_style_radius

dup LV_ALIGN_LEFT_MID 10 0 lv_obj_align

align here lv_anim_t allot
dup lv_anim_init
dup 2 pick lv_anim_set_var
dup 10 50 lv_anim_set_values
dup 1000 lv_anim_set_duration
dup 100 lv_anim_set_reverse_delay
dup 300 lv_anim_set_reverse_duration
dup 500 lv_anim_set_repeat_delay
dup LV_ANIM_REPEAT_INFINITE lv_anim_set_repeat_count
dup c' wrapper_lv_anim_path_ease_in_out lv_anim_set_path_cb

dup c' anim_size_cb lv_anim_set_exec_cb
dup lv_anim_start drop
dup c' anim_x_cb lv_anim_set_exec_cb
dup 10 240 lv_anim_set_values
dup lv_anim_start drop

2drop
