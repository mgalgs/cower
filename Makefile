CC=gcc
CFLAGS=-O2 -pipe
DEBUG=
OBJECTS=aur.o package.o curlhelper.o linkedList.o util.o

all: cower

cower: cower.c $(OBJECTS)
	$(CC) $(CFLAGS) $< $(OBJECTS) -o $@ $(DEBUG) -ljansson -lcurl

aur.o: aur.c aur.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

util.o: util.c util.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

package.o: package.c package.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

curlhelper.o: curlhelper.c curlhelper.h
	$(CC) $(CFLAGS) $< -c $(DEBUG) -lcurl

linkedList.o: linkedList.c linkedList.h
	$(CC) $(CFLAGS) $< -c $(DEBUG)

clean:
	rm *.o

