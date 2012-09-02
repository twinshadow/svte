VERSION = 0.1
prefix = ${HOME}

VTE_INC = `pkg-config --cflags vte-2.90 2> /dev/null`
VTE_LIBS = `pkg-config --libs vte-2.90 2> /dev/null`

# System Flags
CFLAGS = -Os -fomit-frame-pointer -pipe
LDFLAGS = -Wl,-O1 -Wl,--sort-common -Wl,--as-needed

# User Flags
CFLAGS += -g -std=c99 -pedantic -Wall -DVERSION=\"${VERSION}\" ${VTE_INC}
LDFLAGS += -g ${VTE_LIBS}
CC=clang
