#!/usr/bin/make

CC=gcc
CFLAGS=	-g -O0 -Wall -ansi -pedantic 
LIBS = Lib/libexpat.a

OBJS = allegrex.o analyser.o decoder.o switches.o subroutines.o liveness.o \
       structures.o cfg.o dataflow.o operations.o ssa.o graph.o outgraph.o \
       outcode.o output.o prx.o nids.o hash.o lists.o alloc.o utils.o      \
       constants.o relocs.o module.o main.o
TARGET = pspdecompiler

all:	$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< 


.PHONY: clean

clean:
	rm -f $(OBJS) $(TARGET)
