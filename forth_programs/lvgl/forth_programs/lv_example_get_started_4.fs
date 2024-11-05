variable label

: NULL 0 ;

: slider_event_cb ( e -- )
    lv_event_get_target

    label @
    dup s" %d" drop 3 pick lv_slider_get_value 3 lv_label_set_text_fmt
    swap LV_ALIGN_OUT_TOP_MID 0 -15 lv_obj_align_to
;

: lv_example_get_started_4
    lv_screen_active

    dup lv_slider_create
    dup 200 lv_obj_set_width
    dup lv_obj_center
    dup c' slider_event_cb LV_EVENT_VALUE_CHANGED NULL lv_obj_add_event_cb drop

    over lv_label_create dup label !
    dup s" 0" drop lv_label_set_text
    swap LV_ALIGN_OUT_TOP_MID 0 -15 lv_obj_align_to

    drop
;

lv_example_get_started_4
