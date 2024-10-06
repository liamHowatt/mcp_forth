
: factorial ( num -- result )

	1
	swap

	1+
	2 do
		i *

	loop

;


5

factorial . cr
