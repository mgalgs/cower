CC=gcc
CFLAGS=-O2 -pipe -Wall -pedantic
DEBUG=-g
OBJ=util.o
LINKOPT=-ljansson -lcurl -lalpm

all: cower

cower: cower.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LINKOPT) $(DEBUG)

util.o: util.c util.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

#aur.o: aur.c aur.h
#	$(CC) $(CFLAGS) $< -c $(DEBUG)
#
#package.o: package.c package.h
#	$(CC) $(CFLAGS) $< -c $(DEBUG)
#
#curlhelper.o: curlhelper.c curlhelper.h
#	$(CC) $(CFLAGS) $< -c $(DEBUG)

clean:
	@rm *.o

