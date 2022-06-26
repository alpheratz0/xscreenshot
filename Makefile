VERSION = 0.1.0

CC      = cc
CFLAGS  = -std=c99 -pedantic -Wall -Wextra -Os -DVERSION=\"${VERSION}\"
LDLIBS  = -lxcb
LDFLAGS = -s ${LDLIBS}

PREFIX    = /usr/local
MANPREFIX = ${PREFIX}/share/man

SRC = xscreenshot.c
OBJ = xscreenshot.o

all: xscreenshot

.c.o:
	${CC} -c $< ${CFLAGS}

xscreenshot: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f xscreenshot ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/xscreenshot
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -f xscreenshot.1 ${DESTDIR}${MANPREFIX}/man1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/xscreenshot.1

dist: clean
	mkdir -p xscreenshot-${VERSION}
	cp -R LICENSE Makefile README xscreenshot.1 xscreenshot.c xscreenshot-${VERSION}
	tar -cf xscreenshot-${VERSION}.tar xscreenshot-${VERSION}
	gzip xscreenshot-${VERSION}.tar
	rm -rf xscreenshot-${VERSION}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/xscreenshot
	rm -f ${DESTDIR}${MANPREFIX}/man1/xscreenshot.1

clean:
	rm -f xscreenshot xscreenshot-${VERSION}.tar.gz ${OBJ}

.PHONY: all clean install uninstall dist
