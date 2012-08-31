include config.mk

SRC = svte.c
OBJ = ${SRC:.c=.o}

all: options svte

options:
	@echo svte build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

${OBJ}: config.h config.mk

svte: ${OBJ}
