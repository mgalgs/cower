CC=gcc -std=c99 -Wall -pedantic -g
VERSION=$(shell git describe)
CFLAGS=-pipe -O2 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DVERSION=\"${VERSION}\"
LDFLAGS=-ljansson -lcurl -lalpm
OBJ=alpmutil.o conf.o depends.o download.o package.o search.o util.o

all: cower

cower: cower.c ${OBJ}
	${CC} ${CFLAGS} ${LDFLAGS} $< ${OBJ} -o $@

%.o: %.c %.h
	${CC} ${CFLAGS} $< -c

install: all
	@echo installing executable to ${DESTDIR}/usr/bin
	mkdir -p ${DESTDIR}/usr/bin
	cp -f cower ${DESTDIR}/usr/bin
	chmod 755 ${DESTDIR}/usr/bin/cower

clean:
	@rm *.o cower

