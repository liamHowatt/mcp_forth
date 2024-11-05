align here lv_style_t allot
dup lv_style_init

dup 5 lv_style_set_radius
dup LV_OPA_COVER lv_style_set_bg_opa
dup LV_PALETTE_GREY 1 lv_palette_lighten lv_style_set_bg_color

dup 2 lv_style_set_outline_width
dup LV_PALETTE_BLUE lv_palette_main lv_style_set_outline_color
dup 8 lv_style_set_outline_pad

lv_screen_active lv_obj_create
dup 2 pick 0 lv_obj_add_style
dup lv_obj_center

2drop
