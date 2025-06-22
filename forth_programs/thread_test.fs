5 constant last

: count ( id -- id )
	last 1+ 1 do
		." thread " dup . ." says " i . cr
		i last < if 1000 ms then
	loop
;

2 0 ' count thread-create
500 ms
1 count drop
thread-join drop

