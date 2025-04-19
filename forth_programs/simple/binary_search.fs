: binary-search-drop-params ( key base nmemb size compar upper lower -- )
    2drop 2drop 2drop drop
;

: binary-search ( key base nmemb size compar -- res-ptr )
    -1 3 pick
    begin
        2dup swap - 1 >
    while
        2dup + 2/ \ midpoint of two bounds
        dup \ save a copy of midpoint
        5 pick * \ multiply size
        7 pick + \ add base
        dup \ save copy of memb
        9 pick swap \ prepare parameters for comapr: key, memb
        6 pick execute \ call compar
        dup 0= if
            drop \ drop comparison result
            >r \ move the copy of memb to the return stack
            drop \ drop the midpoint
            binary-search-drop-params
            r> \ return the copy of memb
            exit \ return
        then
        swap drop \ drop the memb
        0< if
            swap drop \ the midpoint is the new upper bound
        else
            rot drop swap \ the midpoint is the new lower bound
        then
    repeat
    binary-search-drop-params
    0 \ return NULL
;

align here constant arr
4 , 41 , 104 , 782 , 8888 , 91283 , 92000 , 100000 , 130000 ,

: compar ( key memb -- diff )
    @ swap @ swap -
;

: run-test ( key -- res-ptr )
    arr 9 1 cells ' compar binary-search
;

: run-test-expect-success ( arr-idx -- )
    cells arr +
    dup run-test
    2dup = assert
    drop @ . cr
;

: run-test-expect-failure ( non-existent-num -- )
    dup here swap ,
    run-test
    0= assert
    . ." not found" cr
    -1 cells allot
;

9 0 do
    i run-test-expect-success
loop

111    run-test-expect-failure
1      run-test-expect-failure
200000 run-test-expect-failure
