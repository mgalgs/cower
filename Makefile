CC=gcc -std=c99 -Wall -pedantic# -g
VERSION=-DVERSION=\"$(shell git describe)\"
CFLAGS=-pipe -O2 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
LDFLAGS=-ljansson -lcurl -lalpm
OBJ=alpmutil.o conf.o depends.o download.o package.o search.o util.o

all: cower

cower: cower.c $(OBJ)
	$(CC) $(CFLAGS) $(VERSION) $(LDFLAGS) $< $(OBJ) -o $@

%.o: %.c %.h
	$(CC) $(CFLAGS) $< -c

clean:
	@rm *.o cower

