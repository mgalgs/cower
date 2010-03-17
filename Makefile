CC=gcc
CFLAGS=-O2 -pipe
DEBUG=-g
OBJECTS=curlhelper.o package.o

all: cower

cower: cower.c package.h $(OBJECTS)
	$(CC) $(CFLAGS) $< $(OBJECTS) -o $@ $(DEBUG) -ljansson -lcurl

curlhelper.o: curlhelper.c
	$(CC) $(CFLAGS) $< -c $(DEBUG) -lcurl

util.o: util.c
	$(CC) $(CFLAGS) $< -c $(DEBUG)

package.o: package.c
	$(CC) $(CFLAGS) $< -c $(DEBUG)

clean:
	rm *.o

