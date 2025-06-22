

: recur
    dup .
    ?dup if
        1-
        recurse
    then
;

10
recur
cr
