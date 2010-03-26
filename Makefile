CC=gcc
CFLAGS=-O2 -pipe -Wall -pedantic -std=c99
MACROS=-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
DEBUG=
OBJ=alpmhelper.o aur.o conf.o fetch.o package.o util.o
LINKOPT=-ljansson -lcurl -lalpm

all: cower

cower: cower.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LINKOPT) $(MACROS) $(DEBUG) 

alpmhelper.o: alpmhelper.c alpmhelper.h
	$(CC) $(CFLAGS) $< -c $(MACROS) $(DEBUG)

aur.o: aur.c aur.h
	$(CC) $(CFLAGS) $< -c $(MACROS) $(DEBUG)

conf.o: conf.c conf.h
	$(CC) $(CFLAGS) $< -c $(MACROS)$(DEBUG)

fetch.o: fetch.c fetch.h
	$(CC) $(CFLAGS) $< -c $(MACROS)$(DEBUG)

package.o: package.c package.h
	$(CC) $(CFLAGS) $< -c $(MACROS)$(DEBUG)

util.o: util.c util.h
	$(CC) $(CFLAGS) $< -c $(MACROS)$(DEBUG)

clean:
	@rm *.o

