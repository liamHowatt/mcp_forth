variable cnt


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


: btn_event_cb ( e -- )
    dup lv_event_get_code           \ duplicate e and pass it
    over lv_event_get_target        \ go over the code to get e
    over LV_EVENT_CLICKED = if      \ go over the btn to get the code
        cnt @ 1+ cnt !              \ increment cnt

        \ Get the first child of the button which is the label and change its text
        dup 0 lv_obj_get_child      \ duplicate the btn and push 0 and pass them
        dup s" Button: %d" null-term cnt @ 3 lv_label_set_text_fmt  \ the 3 means take 3 parameters for this var-arg function
        drop \ drop the label
    then
    drop drop drop \ drop the btn, the code, and the e
;

lv_screen_active lv_button_create
dup 10 10 lv_obj_set_pos
dup 120 50 lv_obj_set_size
dup ' btn_event_cb create_c_cb LV_EVENT_ALL NULL lv_obj_add_event_cb

dup lv_label_create
dup s" Button" null-term lv_label_set_text
dup lv_obj_center

drop drop
