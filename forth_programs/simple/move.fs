variable x
16 allot

: dump ( c-addr u -- )
    0 do
        dup c@ .
        1+
    loop
    drop
;

: dump-x ( -- )
    x 20 dump cr
;

dump-x
x 20 0 fill
dump-x

x 1+ 1 over c! 1+
     2 over c! 1+
     3 over c! 1+
     4 over c! 1+
     5 over c! 1+
     6 over c! 1+
drop
dump-x

: move-wrapper move ;

x 3 + x 10 + 3 move-wrapper
dump-x

x 10 + dup 1- 3 move-wrapper
dump-x

x 2 + dup 2 + 4 move-wrapper
dump-x

cr

: print-assert ( actual expect -- )
    swap
    dup . cr
    = assert
;

s" foobar" s" foobar" compare 0 print-assert
s" foobar" s" foobaz" compare -1 print-assert
s" foobar" s" foobaa" compare 1 print-assert
s" foo"    s" foobaa" compare -1 print-assert
s" foobar" s" fooba"  compare 1 print-assert
s" foobar" s" foooo"  compare -1 print-assert
