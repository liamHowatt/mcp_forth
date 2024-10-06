
: main ( n -- )

    dup 0= if drop cr exit then
    1-
    0 .

    dup 0= if drop cr exit then
    1-
    1 .

    dup 0= if drop cr exit then

    0 1
    rot 0 do
        swap
        over
        +
        dup .
    loop

    drop cr
;

45

main
