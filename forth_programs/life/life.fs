

variable inbuf
variable width
variable height
variable grid1
variable grid2
variable grid-old
variable grid-new


: valid ( i j -- flag )
	dup      0< if drop drop 0 exit then
	height @ >= if drop 0 exit then
	dup      0< if drop 0 exit then
	width @  >= if 0 exit then
	-1
;

: read-old-grid ( x y -- v )
	width @ * +
	grid-old @ +
	c@
;

: cn-helper over over valid if read-old-grid + else drop drop then ;
: count-neighbors ( x y -- n )
	0
	2 pick    1-    2 pick    1-    cn-helper
	2 pick          2 pick    1-    cn-helper
	2 pick    1+    2 pick    1-    cn-helper
	2 pick    1-    2 pick    1+    cn-helper
	2 pick          2 pick    1+    cn-helper
	2 pick    1+    2 pick    1+    cn-helper
	2 pick    1-    2 pick          cn-helper
	2 pick    1+    2 pick          cn-helper
	swap drop swap drop
;


: life ( -- )

	-1
	height @ 0 do
		width @ 0 do
			1+

			dup grid-old @ + c@

			\ mid offset, old value

			i j count-neighbors

			\ mid offset, old value, neighbor count
			swap 0= if
				3 = if
					1
				else
					0
				then
			else
				dup 3 > if
					drop
					0
				else
					1 > if
						1
					else
						0
					then
				then
			then

			\ mid offset, new value
			over grid-new @ + c!
		loop
	loop
	drop

;


: dump-grid ( grid_ptr -- )

	height @ 0 do
		width @ 0 do
			dup
			j width @ * i +
			+
			c@
			0= if
				\ 46                \ .
				96 dup emit emit    \ `
			else
				\ 35                \ #
				60 emit 62 emit   \ <>
				\ 91 emit 93 emit     \ []
			then
			\ emit
		loop
		cr
	loop

	drop

;


: main

	\ store pointer to start
	here inbuf !

	\ read in the board until two newlines
	2
	begin
		key

		dup	c,

		10 = if
			1-
		else
			drop 2
		then

	dup 0= until
	drop

	0 \ longest line length
	0 \ current line length

	here inbuf @ do
		i c@
		10 = if
			max
			0
		else
			1+
		then
	loop
	max

	." grid width is  " dup . cr
	width !

	-1 \ line sum

	here inbuf @ do
		i c@
		10 = if
			1+
		then
	loop

	." grid height is " dup . cr
	height !

	width @ height @ *
	here grid1 !
	dup allot
	here grid2 !
	dup allot

	grid1 @
	swap
	0
	fill

	grid1 @
	inbuf @
	height @ 0 do

		0
		\ stack is grid1 row pointer, inbuf pointer, and column index
		begin
			over c@
			\ stack is grid1 row pointer, inbuf pointer, column index, and character

			rot 1+ rot rot

			dup 35 = if
				1
				4 pick
				3 pick
				+
				c!
			then

			swap 1+ swap
		10 = until
		drop

		\ stack is grid1 row pointer and inbuf pointer
		swap width @ + swap
	loop
	drop drop

	grid1 @ dump-grid cr
	400 ms


	grid1 @ grid-old !
	grid2 @ grid-new !

	begin
		life

		grid-new @ dump-grid cr

		grid-old @ grid-new @
		grid-old ! grid-new !

		400 ms
	again
;



main


