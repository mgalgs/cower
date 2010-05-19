# dmenu version
VERSION = $(shell git describe)

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

CURLINC = /usr/include/curl
CURLLIB = /usr/lib/

# includes and libs
INCS = -I. -I/usr/include -I${CURLINC}
LIBS = -L/usr/lib -lc -lcurl -lalpm -ljansson

# flags
CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\"
CFLAGS = -std=c99 -pedantic -Wall -O2 ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}

# compiler and linker
CC = gcc -pipe -g
