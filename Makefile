CC=gcc -std=c99 -Wall -pedantic -g
VERSION=$(shell git describe)
CFLAGS=-pipe -O2 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DVERSION=\"${VERSION}\"
LDFLAGS=-ljansson -lcurl -lalpm
OBJ=alpmutil.o conf.o depends.o download.o package.o search.o util.o

default: cower
all: cower doc

cower: cower.c ${OBJ}
	${CC} ${CFLAGS} ${LDFLAGS} $< ${OBJ} -o $@

%.o: %.c %.h
	${CC} ${CFLAGS} $< -c

doc:
	pod2man --section=1 --center=" " --release=" " --name="COWER" --date="burp-VERSION" cower.pod > cower.1

install: all
	@echo installing executable to ${DESTDIR}/usr/bin
	@mkdir -p ${DESTDIR}/usr/bin
	@cp -f cower ${DESTDIR}/usr/bin
	@chmod 755 ${DESTDIR}/usr/bin/cower
	@echo installing man page to ${DESTDIR}${MANPREFIX}
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < cower.1 > ${DESTDIR}${MANPREFIX}/man1/cower.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/cower.1

clean:
	@rm *.o cower cower.1

.PHONY: all cower doc install clean
