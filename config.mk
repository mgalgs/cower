# cower version
VERSION = $(shell git describe)

# paths
PREFIX ?= /usr/local
MANPREFIX ?= ${PREFIX}/share/man

# includes and libs
ALPMINC = /usr/include
CURLINC = /usr/include/curl
YAJLINC = /usr/include/yajl

INCS = -I. -I/usr/include -I${CURLINC} -I${YAJLINC}
LIBS = -L/usr/lib -lc -lcurl -lalpm -lyajl -larchive

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS += -std=c99 -pedantic -Wall ${INCS} ${CPPFLAGS}
LDFLAGS += ${LIBS}

# compiler and linker
CC = gcc -pipe -g
