#!/bin/bash

BASENAME=$(basename $2 .fs)
BINNAME="${BASENAME}.bin"
echo $2
./compile-exe $1 $2 $BINNAME
./execute-exe $1 $BINNAME
echo
