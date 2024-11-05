align here
5 , 5 ,
70 , 70 ,
120 , 10 ,
180 , 60 ,
240 , 10 ,

here
dup lv_style_init
dup 8 lv_style_set_line_width
dup LV_PALETTE_BLUE lv_palette_main lv_style_set_line_color
dup 1 lv_style_set_line_rounded

lv_screen_active lv_line_create
dup 3 pick 5 lv_line_set_points
dup 2 pick 0 lv_obj_add_style
dup lv_obj_center

2drop drop
