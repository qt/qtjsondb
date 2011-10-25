#!/bin/sh

rm -f bin/bits32 bin/bits64
rm -f btree.o main.o
qmake TARGET=bits64 && make
rm -f btree.o main.o
qmake TARGET=bits32 -spec linux-g++-32 && make
