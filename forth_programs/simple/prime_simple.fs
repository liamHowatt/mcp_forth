
: prime ( num -- isprime )

	dup
	1 -

	begin
		dup 1 >
		while
		
		over over mod

		0 = if
			drop drop
			0
			exit
		then

		1 -

	repeat

	drop drop
	-1
;



: main ( count -- )

	4
	prime
	if   ."     prime"
	else ." not prime"
	then
	cr

;

main

