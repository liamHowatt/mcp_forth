variable cnt 0 cnt !

: NULL 0 ;

: btn_event_cb ( e -- )
    dup lv_event_get_code           \ duplicate e and pass it
    over lv_event_get_target        \ go over the code to get e
    over LV_EVENT_CLICKED = if      \ go over the btn to get the code
        cnt @ 1+ cnt !              \ increment cnt

        \ Get the first child of the button which is the label and change its text
        dup 0 lv_obj_get_child      \ duplicate the btn and push 0 and pass them
        dup s" Button: %d" drop cnt @ 3 lv_label_set_text_fmt  \ the 3 means take 3 parameters for this var-arg function
        drop \ drop the label
    then
    drop drop drop \ drop the btn, the code, and the e
;

lv_screen_active lv_button_create
dup 10 10 lv_obj_set_pos
dup 120 50 lv_obj_set_size
dup c' btn_event_cb LV_EVENT_ALL NULL lv_obj_add_event_cb drop

dup lv_label_create
dup s" Button" drop lv_label_set_text
dup lv_obj_center

drop drop
