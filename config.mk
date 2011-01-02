# version
VERSION = $(shell git describe)

# paths
PREFIX ?= /usr/local
MANPREFIX ?= ${PREFIX}/share/man

# compiler flags
CC ?= gcc
CPPFLAGS = -DCOWER_VERSION=\"${VERSION}\" ${PMCHECK}
CFLAGS += --std=c99 -g -pedantic -Wall -Wextra ${CPPFLAGS}
LDFLAGS += -lcurl -lalpm -lyajl -larchive -pthread

