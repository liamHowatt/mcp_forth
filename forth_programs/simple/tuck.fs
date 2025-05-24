: print-assert ( actual expect -- )
    swap
    dup . cr
    = assert
;

1 2 tuck

2 print-assert
1 print-assert
2 print-assert
depth 0 print-assert
