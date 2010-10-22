# cower version
REF ?= master
VERSION = $(shell git describe ${REF})

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
CPPFLAGS = -DCOWER_VERSION=\"${VERSION}\"
CFLAGS += -pipe -std=c99 -pedantic -Wall -Wextra ${INCS} ${CPPFLAGS} -fno-builtin -g
LDFLAGS += ${LIBS} -pthread

# compiler and linker
CC = gcc
