\ Basic example to create a "Hello world" label

\ Change the active screen's background color
lv_screen_active 0x003a57 lv_color_hex LV_PART_MAIN lv_obj_set_style_bg_color

\ Create a white label, set its text and align it to the center
lv_screen_active lv_label_create
dup s" Hello world!" drop lv_label_set_text
lv_screen_active 0xffffff lv_color_hex LV_PART_MAIN lv_obj_set_style_text_color
dup LV_ALIGN_CENTER 0 0 lv_obj_align

drop     \ drop the label pointer from the stack
