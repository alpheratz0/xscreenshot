VERSION = 1.0.0-rev+${shell git rev-parse --short=16 HEAD}
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man
LDLIBS = -lxcb
LDFLAGS = -s ${LDLIBS}
INCS = -I/usr/include
CFLAGS = -std=c99 -pedantic -Wall -Wextra -Os ${INCS} -DVERSION="\"${VERSION}\""
CC = cc

SRC = src/xscreenshot.c \
	  src/debug.c

OBJ = ${SRC:.c=.o}

all: xscreenshot

${OBJ}:	src/debug.h

xscreenshot: ${OBJ}
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

install: all
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f xscreenshot ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/xscreenshot
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@cp -f man/xscreenshot.1 ${DESTDIR}${MANPREFIX}/man1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/xscreenshot.1

dist: clean
	@mkdir -p xscreenshot-${VERSION}
	@cp -R LICENSE Makefile README man src xscreenshot-${VERSION}
	@tar -cf xscreenshot-${VERSION}.tar xscreenshot-${VERSION}
	@gzip xscreenshot-${VERSION}.tar
	@rm -rf xscreenshot-${VERSION}

uninstall:
	@rm -f ${DESTDIR}${PREFIX}/bin/xscreenshot
	@rm -f ${DESTDIR}${MANPREFIX}/man1/xscreenshot.1

clean:
	@rm -f xscreenshot xscreenshot-${VERSION}.tar.gz ${OBJ}

.PHONY: all clean install uninstall dist
