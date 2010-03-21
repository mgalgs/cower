CC=gcc
CFLAGS=-O2 -pipe -Wall -pedantic -std=c99
DEBUG=
OBJ=util.o json.o aur.o alpmhelper.o
LINKOPT=-ljansson -lcurl -lalpm

all: cower

cower: cower.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LINKOPT) $(DEBUG)

util.o: util.c util.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

json.o: json.c json.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

aur.o: aur.c aur.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

alpmhelper.o: alpmhelper.c alpmhelper.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

clean:
	@rm *.o

