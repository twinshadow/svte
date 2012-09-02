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

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

svte: ${OBJ}

install: all
	@echo installing executable file to ${prefix}/bin
	@install -d ${prefix}/bin
	@install -m 755 svte ${prefix}/bin/svte
