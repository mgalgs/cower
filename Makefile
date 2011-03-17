# cower - a simple AUR downloader

include config.mk

SRC = cower.c
OBJ = ${SRC:.c=.o}

DISTFILES = Makefile README.pod bash_completion config cower.c

all: cower doc

35:
	${MAKE} PMCHECK="-D_HAVE_ALPM_FIND_SATISFIER -D_HAVE_ALPM_DB_GET_PKGCACHE_LIST" all

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.mk
cower: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

doc: cower.1
cower.1: README.pod
	pod2man --section=1 --center="Cower Manual" --name="COWER" --release="cower ${VERSION}" $< > $@

install: cower cower.1
	install -D -m755 cower ${DESTDIR}${PREFIX}/bin/cower
	install -D -m644 cower.1 ${DESTDIR}${MANPREFIX}/man1/cower.1
	install -D -m644 bash_completion ${DESTDIR}/etc/bash_completion.d/cower
	install -D -m644 config ${DESTDIR}/usr/share/cower/config

dist: clean
	mkdir cower-${VERSION}
	cp ${DISTFILES} cower-${VERSION}
	sed "s/^VERSION = .*/VERSION = ${VERSION}/" config.mk > cower-${VERSION}/config.mk
	tar cf - cower-${VERSION} | gzip -9 > cower-${VERSION}.tar.gz
	rm -rf cower-${VERSION}

strip: cower
	strip --strip-all cower

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	rm -f ${DESTDIR}${PREFIX}/bin/cower
	@echo removing man page from ${DESTDIR}${PREFIX}/man1/cower.1
	rm -f ${DESTDIR}/${PREFIX}/man1/cower.1
	@echo removing bash completion
	rm -f ${DESTDIR}/etc/bash_completion.d/cower
	@echo removing sample config
	rm -f ${DESTDIR}/usr/share/cower/config

cscope: cscope.out
cscope.out:
	cscope -b

clean:
	rm -f *.o cower cower.1 cscope.out

.PHONY: all clean dist doc install uninstall

