# Makefile for the CS:APP Shell Lab

VERSION = 1
CC = gcc
CFLAGS = -Wall -O2

tsh: tsh.o util.o jobs.o
	$(CC) $(CFLAGS) tsh.o util.o jobs.o -o tsh

psh: psh.o util.o 
	$(CC) $(CFLAGS) psh.o util.o -o psh

handle: handle.o util.o


# clean up
clean:
	rm -f $(FILES) *.o *~ *.bak *.BAK



