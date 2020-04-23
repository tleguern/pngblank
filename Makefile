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

all: ${PROG} README.md

.c.o:
	${CC} ${CFLAGS} -c $<

${PROG}: ${OBJS}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

README.md: pngblank.1
	mandoc -T markdown pngblank.1 > README.md

clean:
	rm -f -- ${OBJS} ${PROG}

install:
	mkdir -p ${PREFIX}/bin/
	mkdir -p ${DESTDIR}${MANDIR}/man1
	${INSTALL_PROGRAM} ${PROG} ${PREFIX}/bin/${PROG}
	${INSTALL_MAN} ${PROG}.1 ${DESTDIR}${MANDIR}/man1
