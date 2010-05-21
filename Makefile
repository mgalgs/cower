# cower - a simple AUR downloader

include config.mk

SRC = conf.c cower.c curl.c depends.c download.c package.c pacman.c search.c util.c update.c yajl.c
OBJ = ${SRC:.c=.o}

all: options cower doc

options:
	@echo cower build options:
	@echo "PREFIX    = ${PREFIX}"
	@echo "MANPREFIX = ${MANPREFIX}"
	@echo "CC        = ${CC}"
	@echo "CFLAGS    = ${CFLAGS}"
	@echo "LDFLAGS   = ${LDFLAGS}"

.c.o:
	@printf "   %-8s %s\n" CC $@
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

cower: ${OBJ}
	@printf "   %-8s %s\n" LD $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

doc: cower.1
cower.1: README.pod
	@printf "   %-8s %s\n" DOC cower.1
	@pod2man --section=1 --center=" " --release=" " --name="COWER" --date="cower-VERSION" README.pod > cower.1

install: cower cower.1
	@printf "   %-8s %s\n" INSTALL cower
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f cower ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/cower
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@printf "   %-8s %s\n" INSTALL cower.1
	@sed "s/VERSION/${VERSION}/g" < cower.1 > ${DESTDIR}${MANPREFIX}/man1/cower.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/cower.1

dist: clean
	@mkdir -p cower-${VERSION}
	@cp -R ${SRC} *.h Makefile cower.pod cower-${VERSION}
	@printf "   %-8s %s\n" TAR cower-${VERSION}.tar
	@tar -cf cower-${VERSION}.tar cower-${VERSION}
	@printf "   %-8s %s\n" GZIP cower-${VERSION}.tar.gz
	@gzip cower-${VERSION}.tar
	@rm -rf cower-${VERSION}

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/cower
	@echo removing man page from ${DESTDIR}${PREFIX}/man1/cower.1
	@rm -f ${DESTDIR}/${PREFIX}/man1/cower.1

clean:
	@printf "   %-8s %s\n" CLEAN "*.o cower cower.1"
	@rm -f *.o cower cower.1

.PHONY: all clean dist doc install options uninstall

