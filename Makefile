CC=gcc
MACROS=-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
CFLAGS=-O2 -pipe -Wall -pedantic -std=c99
LDFLAGS=-ljansson -lcurl -lalpm
OBJ=alpmhelper.o aur.o conf.o fetch.o package.o util.o

all: cower

cower: cower.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LDFLAGS) $(MACROS)

alpmhelper.o: alpmhelper.c alpmhelper.h
	$(CC) $(CFLAGS) -c $(MACROS) $<

aur.o: aur.c aur.h
	$(CC) $(CFLAGS) -c $(MACROS) $<

conf.o: conf.c conf.h
	$(CC) $(CFLAGS) -c $(MACROS) $<

fetch.o: fetch.c fetch.h
	$(CC) $(CFLAGS) -c $(MACROS) $<

package.o: package.c package.h
	$(CC) $(CFLAGS) -c $(MACROS) $<

util.o: util.c util.h
	$(CC) $(CFLAGS) -c $(MACROS) $<

clean:
	@rm *.o

