

: recur
    dup .
    dup 0 =
    if
        drop
        exit
    else
        1 -
        recurse
    then
;

10
recur
cr
