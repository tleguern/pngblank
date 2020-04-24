include Makefile.configure

PROG= pngblank
SRCS= lgpng.c compats.c ${PROG}.c
OBJS= ${SRCS:.c=.o}

LDADD+= -lz
LDFLAGS+=
CFLAGS+= -std=c99 -Wpointer-sign
CFLAGS= -Wall -Wextra

.SUFFIXES: .c .o .1 .md
.PHONY: clean install

all: ${PROG} pngblank.md

.1.md:
	mandoc -T markdown $< > $@

.c.o:
	${CC} ${CFLAGS} -c $<

${PROG}: ${OBJS}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

pngblank.md: pngblank.1

clean:
	rm -f -- ${OBJS} ${PROG}

install:
	mkdir -p ${PREFIX}/bin/
	mkdir -p ${DESTDIR}${MANDIR}/man1
	${INSTALL_PROGRAM} ${PROG} ${PREFIX}/bin/${PROG}
	${INSTALL_MAN} ${PROG}.1 ${DESTDIR}${MANDIR}/man1
