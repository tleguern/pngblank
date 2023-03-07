include Makefile.configure

PROG= pngblank
SRCS= lgpng.c compats.c ${PROG}.c
OBJS= ${SRCS:.c=.o}

LDADD+= -lz -ldeflate
LDFLAGS+= -L/usr/local/lib/
CFLAGS+= -Wall -Wextra -I/usr/local/include
CFLAGS+= -Wimplicit-fallthrough -Wno-write-strings

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
	mkdir -p ${BINDIR}
	mkdir -p ${MANDIR}/man1
	${INSTALL_PROGRAM} ${PROG} ${BINDIR}
	${INSTALL_MAN} ${PROG}.1 ${MANDIR}/man1
