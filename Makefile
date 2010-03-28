CC=gcc
MACROS=-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
CFLAGS=-O2 -pipe -Wall -pedantic -std=c99
LDFLAGS=-ljansson -lcurl -lalpm
OBJ=alpmhelper.o aur.o conf.o fetch.o package.o util.o

all: cower

cower: cower.c $(OBJ)
	$(CC) $(CFLAGS) $(MACROS) $(LDFLAGS) $< $(OBJ) -o $@

alpmhelper.o: alpmhelper.c alpmhelper.h
	$(CC) $(CFLAGS) $(MACROS) $< -c

aur.o: aur.c aur.h
	$(CC) $(CFLAGS) $(MACROS) $< -c

conf.o: conf.c conf.h
	$(CC) $(CFLAGS) $(MACROS) $< -c

fetch.o: fetch.c fetch.h
	$(CC) $(CFLAGS) $(MACROS) $< -c

package.o: package.c package.h
	$(CC) $(CFLAGS) $(MACROS) $< -c

util.o: util.c util.h
	$(CC) $(CFLAGS) $(MACROS) $< -c

clean:
	@rm *.o

