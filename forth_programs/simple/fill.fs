variable x
6 allot

: dump ( c-addr u -- )
    0 do
        dup c@ .
        1+
    loop
    drop
;

x 10 dump cr
x 10 42 fill
x 10 dump cr
x 2 + 6 100 fill
x 10 dump cr

cr

here
5 allot
dup 5 dump cr
1- 6 dump cr
