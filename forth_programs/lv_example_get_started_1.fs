\ <string boilerplate>

variable sbuf_len 4 sbuf_len !       \ `sbuf_len` starts as 4
variable sbuf                        \ define `sbuf`. It marks the beginning of the buf
: null-term ( addr len -- temporary-null-terminated-string )
    dup sbuf_len @ >= if             \ if the string is longer than the tmp buf
        dup 1+                       
        dup sbuf_len @ -             \ calculate how much more to allocate
        allot                        \ allocate it
        sbuf_len !                   \ update the len
    then
    over sbuf 2 pick move            \ memcpy the string to the buf
    0 over sbuf + c!                 \ add a null terminator to the buf
    drop drop                        \ drop the parameters
    sbuf                             \ return the temporary buffer
;

\ </string boilerplate>


\ Basic example to create a "Hello world" label

\ Change the active screen's background color
lv_screen_active 0x003a57 lv_color_hex LV_PART_MAIN lv_obj_set_style_bg_color

\ Create a white label, set its text and align it to the center
lv_screen_active lv_label_create
dup s" Hello world!" null-term lv_label_set_text
lv_screen_active 0xffffff lv_color_hex LV_PART_MAIN lv_obj_set_style_text_color
dup LV_ALIGN_CENTER 0 0 lv_obj_align

drop     \ drop the label pointer from the stack
