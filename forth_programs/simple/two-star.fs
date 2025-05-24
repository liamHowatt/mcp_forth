: print-assert ( actual expect -- )
    swap
    dup . cr
    = assert
;

4 2* 8 print-assert
depth 0 print-assert
