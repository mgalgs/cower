CC=gcc -std=c99 -Wall -pedantic -g
VERSION=$(shell git describe)
CFLAGS=-pipe -O2 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DVERSION=\"${VERSION}\"
LDFLAGS=-ljansson -lcurl -lalpm
SRC = alpmutil.c conf.c depends.c download.c package.c search.c util.c
OBJ = ${SRC:.c=.o}

all: cower doc
doc: cower.1

cower: cower.c ${OBJ}
	${CC} ${CFLAGS} ${LDFLAGS} $< ${OBJ} -o $@

%.o: %.c %.h
	${CC} ${CFLAGS} $< -c

cower.1: cower.pod
	pod2man --section=1 --center=" " --release=" " --name="COWER" --date="cower-VERSION" cower.pod > cower.1

install: all
	@echo installing executable to ${DESTDIR}/usr/bin
	@mkdir -p ${DESTDIR}/usr/bin
	@cp -f cower ${DESTDIR}/usr/bin
	@chmod 755 ${DESTDIR}/usr/bin/cower
	@echo installing man page to ${DESTDIR}${MANPREFIX}
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < cower.1 > ${DESTDIR}${MANPREFIX}/man1/cower.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/cower.1

dist: clean
	@echo creating dist tarball
	@mkdir -p cower-${VERSION}
	@cp -R ${SRC} Makefile cower.pod cower-${VERSION}
	@tar -cf cower-${VERSION}.tar cower-${VERSION}
	@gzip cower-${VERSION}.tar
	@rm -rf cower-${VERSION}

uninstall:
	@echo removing executable file from ${DESTDIR}/usr/bin
	@rm -f ${DESTDIR}/usr/bin/cower
	@echo removing man page from ${DESTDIR}${MANPREFIX}/man1/cower.1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/cower.1

clean:
	@rm -f *.o cower cower.1

.PHONY: all clean install uninstall doc
