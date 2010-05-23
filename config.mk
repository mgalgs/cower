# cower version
VERSION = $(shell git describe)

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# includes and libs
ALPMINC = /usr/include
CURLINC = /usr/include/curl
YAJLINC = /usr/include/yajl

INCS = -I. -I/usr/include -I${CURLINC} -I${YAJLINC}
LIBS = -L/usr/lib -lc -lcurl -lalpm -lyajl

# flags
CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\"
CFLAGS = -std=c99 -pedantic -Wall -O2 ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}

# compiler and linker
CC = gcc -pipe -g
