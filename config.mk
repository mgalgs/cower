# version
VERSION = $(shell git describe)

# paths
PREFIX ?= /usr/local
MANPREFIX ?= ${PREFIX}/share/man

# compiler flags
CC = c99 -pipe
CPPFLAGS = -DCOWER_VERSION=\"${VERSION}\" -D_FORTIFY_SOURCE=2 ${PMCHECK}
CFLAGS += -g -pedantic -Wall -Wextra ${CPPFLAGS}
LDFLAGS += -lcurl -lalpm -lyajl -larchive -pthread

