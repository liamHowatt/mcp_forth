: print-assert ( actual expect -- )
    swap
    dup . cr
    = assert
;

1 ?dup depth 2 print-assert 1 print-assert 1 print-assert
0 ?dup depth 1 print-assert 0 print-assert
depth 0 print-assert
