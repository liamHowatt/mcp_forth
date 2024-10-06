
: decode_hex_nibble ( c -- nibble )

	dup 97 >= if
		87
	else
		dup 65 >= if
			55
		else
			48
		then
	then

	-
;

: read_hex_byte ( -- b )

	key dup emit
	decode_hex_nibble
	4 lshift

	key dup emit
	decode_hex_nibble

	or
;


: encode_hex_nibble ( nibble -- c )

	dup

	9 > if
		87
	else
		48
	then

	+
;

: print_hex_byte ( b -- )

	dup
	4 rshift
	encode_hex_nibble
	emit
	15 and
	encode_hex_nibble
	emit
;


variable hash_array
19 allot


: main

	20 0 do
		read_hex_byte
		hash_array i + c!
	loop
	cr

	20 0 do
		hash_array i +
		dup c@
		read_hex_byte
		xor
		swap c!
	loop
	cr

	20 0 do
		hash_array i + c@
		print_hex_byte
	loop
	cr
;

main
