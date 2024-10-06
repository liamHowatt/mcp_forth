: foo1 begin dup 0 >                   while dup . 1- repeat drop ;
: foo2 begin dup 0 > if -1 else 0 then while dup . 1- repeat drop ;
: foo3 begin 0 while ." unreachable" repeat ." done" ;

." foo1 " 5 foo1 cr
." foo2 " 5 foo2 cr
." foo3 "   foo3 cr
