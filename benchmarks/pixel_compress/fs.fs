: pre_mask_value ( hw -- hw )
	0xffdf and
;

: compute_value ( hw -- hw )
	dup 0xffc0 and 2/
	swap 0x001f and
	or
;

: pre_inc2 ( addr -- addr+2 addr+2 )
	2 + dup
;

: store ( dst val -- )
	2dup swap 1+ c!
	8 rshift swap c!
;

align here constant codes
	0xaaab , 0x80ff , 0x8cff , 0x8ccf , 0x98c9 , 0xaabf , 0xaaaf ,
variable last_value
variable repeated
variable start

: send_repeat_codes
	repeated @ 7 / dup 0<> if 0 do
		pre_inc2 0xaaab store
	loop else drop then

	repeated @ 7 mod dup 0<> if
		>r pre_inc2 r> cells codes + @ store
	else drop then
;

: lcdpi_compress ( src dst src_len -- out_len )
	>r
	dup start !
	1+
	swap pre_inc2 w@ pre_mask_value last_value ! swap
	pre_inc2 last_value @ compute_value store
	0 repeated !
	swap
	r> 1 do
		\ inline this: pre_inc2 w@ pre_mask_value
			           1+ 1+ dup w@ 0xffdf and
		dup last_value @ <> if
			last_value !
			swap

			send_repeat_codes

			pre_inc2 last_value @ compute_value store

			0 repeated !
			swap
		else
			drop
			1 repeated +!
		then
	loop
	swap

	send_repeat_codes

	swap drop
	start @ -
;

: check_error ( wior -- )
	0<> if ." error" cr bye then
;

variable byte_read
: read_file
	s" /home/liam/Downloads/rgb565data.bin" r/o open-file check_error
	begin byte_read 1 2 pick read-file check_error 0> while byte_read @ c, repeat
	close-file check_error
;

: main
	here 1024 dup * allot
	66 over c!
	dup align here read_file here over - 2/ >r 2 - swap 2 - r>
	-1
	1000 0 do
		drop
		2 pick 2 pick 2 pick lcdpi_compress
	loop
	4 pick swap
	type
;

main
