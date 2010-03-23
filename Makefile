CC=gcc
CFLAGS=-O2 -pipe -Wall -pedantic -std=c99
DEBUG=
OBJ=alpmhelper.o aur.o conf.o fetch.o util.o
LINKOPT=-ljansson -lcurl -lalpm

all: cower

cower: cower.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LINKOPT) $(DEBUG)

alpmhelper.o: alpmhelper.c alpmhelper.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

aur.o: aur.c aur.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

conf.o: conf.c conf.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

fetch.o: fetch.c fetch.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

util.o: util.c util.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

clean:
	@rm *.o

