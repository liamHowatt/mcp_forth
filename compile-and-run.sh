#!/bin/bash

BASENAME=$(basename $1 .fs)
BINNAME="${BASENAME}.bin"
echo $1
./compile-exe vm $1 $BINNAME
./execute-exe vm $BINNAME
echo
