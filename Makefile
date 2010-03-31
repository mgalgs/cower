VERSION=-DVERSION=\"$(shell git describe)\"
MACROS=-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
CFLAGS=-pipe -Wall -pedantic -std=c99 -O2
LDFLAGS=-ljansson -lcurl -lalpm
OBJ=alpmutil.o conf.o depends.o download.o package.o search.o util.o

all: cower

cower: cower.c $(OBJ)
	$(CC) $(CFLAGS) $(MACROS) $(VERSION) $(LDFLAGS) $< $(OBJ) -o $@

%.o: %.c %.h
	$(CC) $(CFLAGS) $(MACROS) $< -c

clean:
	@rm *.o

