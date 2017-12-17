# Makefile
# Project maintenance for saucy
#
# by Paul T. Darga <pdarga@umich.edu>
# and Mark Liffiton <liffiton@umich.edu>
# and Hadi Katebi <hadik@umich.edu>
#
# Copyright (C) 2004, The Regents of the University of Michigan
# See the LICENSE file for details.

CC=gcc
#CFLAGS=-ansi -pedantic -Wall -O0 -ggdb
CFLAGS=-ansi -pedantic -Wall -O3
LDLIBS=-lz

.PHONY : all clean

all : saucy shatter

saucy : main.o saucy.o saucyio.o util.o
shatter : shatter.o saucy.o saucyio.o util.o

main.o shatter.o saucy.o saucyio.o : saucy.h
main.o shatter.o saucyio.o : amorph.h
main.o shatter.o util.o : util.h
main.o shatter.o saucyio.o : platform.h

clean :
	rm -f saucy shatter *.o
