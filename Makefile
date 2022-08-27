.POSIX:
.PHONY: all clean install uninstall dist

include config.mk

all: xscreenshot

xscreenshot: xscreenshot.o
	$(CC) $(LDFLAGS) -o xscreenshot xscreenshot.o $(LDLIBS)

clean:
	rm -f xscreenshot xscreenshot.o xscreenshot-$(VERSION).tar.gz

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f xscreenshot $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/xscreenshot
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	cp -f xscreenshot.1 $(DESTDIR)$(MANPREFIX)/man1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/xscreenshot.1

dist: clean
	mkdir -p xscreenshot-$(VERSION)
	cp -R COPYING config.mk Makefile README config.h xscreenshot.1 \
		xscreenshot.c xscreenshot-$(VERSION)
	tar -cf xscreenshot-$(VERSION).tar xscreenshot-$(VERSION)
	gzip xscreenshot-$(VERSION).tar
	rm -rf xscreenshot-$(VERSION)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/xscreenshot
	rm -f $(DESTDIR)$(MANPREFIX)/man1/xscreenshot.1
