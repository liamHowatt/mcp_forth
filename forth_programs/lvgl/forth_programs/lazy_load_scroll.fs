variable top_num
variable bottom_num

: add_thing ( parent num -- )
    swap
    lv_obj_create
    dup 100 lv_pct LV_SIZE_CONTENT lv_obj_set_size
    dup lv_label_create
    \ stack: num, obj, label
    rot s" %d" drop swap 3 lv_label_set_text_fmt
;

: update_scroll ( obj -- )
    \ load objs we're getting close to
    begin bottom_num @ 30 <= if dup lv_obj_get_scroll_bottom 200 < else 0 then while
        dup bottom_num @ add_thing drop
            1 bottom_num +!
        dup lv_obj_update_layout
        ." loaded bottom" cr
    repeat
    begin top_num @ -30 >= if dup lv_obj_get_scroll_top 200 < else 0 then while
        dup lv_obj_get_scroll_bottom >r
        dup top_num @ add_thing
            -1 top_num +!
        0 lv_obj_move_to_index
        dup lv_obj_update_layout
        dup lv_obj_get_scroll_bottom >r
        dup 0 2r> - LV_ANIM_OFF lv_obj_scroll_by
        ." loaded top" cr
    repeat

    \ delete far-away objs
    begin dup lv_obj_get_scroll_bottom 1000 > while
        dup -1 lv_obj_get_child
        lv_obj_delete
        -1 bottom_num +!
        dup lv_obj_update_layout
        ." deleted bottom" cr
    repeat
    begin dup lv_obj_get_scroll_top 1000 > while
        dup lv_obj_get_scroll_bottom >r
        dup 0 lv_obj_get_child
        lv_obj_delete
        1 top_num +!
        dup lv_obj_update_layout
        dup lv_obj_get_scroll_bottom >r
        dup 0 2r> - LV_ANIM_OFF lv_obj_scroll_by
        ." deleted top" cr
    repeat

    drop
;

: event_cb ( e -- )
    lv_event_get_target_obj
    update_scroll
;

: infinite_scroll_example ( -- )
    lv_screen_active lv_obj_create
    dup 350 dup lv_obj_set_size
    dup lv_obj_center
    dup LV_FLEX_FLOW_COLUMN lv_obj_set_flex_flow

    \ dup LV_OPA_TRANSP LV_PART_SCROLLBAR lv_obj_set_style_opa

    \ 10 0 do
    \     dup i add_thing drop
    \ loop
    \ -1 top_num !
    \ 10 bottom_num !
    0 top_num !
    0 bottom_num !
    dup lv_obj_update_layout
    dup update_scroll

    c' event_cb LV_EVENT_SCROLL 0 lv_obj_add_event_cb
;

infinite_scroll_example
