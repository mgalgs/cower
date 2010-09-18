# cower - a simple AUR downloader

include config.mk

SRC = ${wildcard *.c}
OBJ = ${SRC:.c=.o}

all: buildopts cower doc

buildopts:
	@echo build options:
	@echo "CC        = ${CC}"
	@echo "CFLAGS    = ${CFLAGS}"
	@echo "LDFLAGS   = ${LDFLAGS}"

installopts:
	@echo install options:
	@echo "PREFIX    = ${PREFIX}"
	@echo "MANPREFIX = ${MANPREFIX}"

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
	@pod2man --section=1 --center="Cower Manual" --name="COWER" --release="cower ${VERSION}" README.pod > cower.1

install: installopts cower cower.1
	@printf "   %-8s %s\n" INSTALL ${DESTDIR}${PREFIX}/bin/cower
	@install -D -m755 cower ${DESTDIR}${PREFIX}/bin/cower
	@printf "   %-8s %s\n" INSTALL ${DESTDIR}${MANPREFIX}/man1/cower.1
	@install -D -m644 cower.1 ${DESTDIR}${MANPREFIX}/man1/cower.1
	@printf "   %-8s %s\n" INSTALL ${DESTDIR}/etc/bash_completion.d/cower
	@install -D -m644 bash_completion ${DESTDIR}/etc/bash_completion.d/cower

dist: clean
	@mkdir -p cower-${VERSION}
	@cp -R ${SRC} *.h Makefile config.mk README.pod bash_completion cower-${VERSION}
	@sed "s/VERSION =.*/VERSION = $(shell git describe)/" < config.mk > cower-${VERSION}/config.mk
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
	@echo removing bash completion
	@rm -f ${DESTDIR}/etc/bash_completion.d/cower

clean:
	@printf "   %-8s %s\n" CLEAN "*.o cower cower.1"
	@rm -f *.o cower cower.1

.PHONY: all clean dist doc install buildopts installopts uninstall

