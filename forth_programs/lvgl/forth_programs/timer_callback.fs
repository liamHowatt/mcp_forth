: timer_cb ( tim -- )
    lv_timer_get_user_data ." user data is " . cr
    dup ." counter is " . cr
    cr
    1+
;

-100

c' timer_cb 1000 42 lv_timer_create drop
