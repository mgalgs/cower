# version
VERSION = $(shell git describe)

# paths
PREFIX ?= /usr/local
MANPREFIX ?= ${PREFIX}/share/man

# compiler flags
CC = c99
CPPFLAGS = -DCOWER_VERSION=\"${VERSION}\" ${PMCHECK}
CFLAGS += -g -pedantic -Wall -Wextra ${CPPFLAGS}
LDFLAGS += -lcurl -lalpm -lyajl -larchive -pthread

