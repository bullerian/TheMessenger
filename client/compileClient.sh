#!/bin/bash
LIBS='-pthread'
INC='includes/'
OPT='-Wall -O0'
OUTPUT='output/TheMessenger_client'
SRC='main.c'

gcc -o$OUTPUT $SRC -I$INC $LIBS $OPT
