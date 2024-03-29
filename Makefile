CXX = g++
CC = gcc

install_ = install
name_ = grml-debugtools

etc = ${DESTDIR}/etc/
bin = ${DESTDIR}/bin
usr = ${DESTDIR}/usr
usrbin = $(usr)/bin
usrsbin = $(usr)/sbin
usrshare = $(usr)/share/$(name_)
usrdoc = $(usr)/share/doc/$(name_)

#CPPFLAGS = -pipe -Wall -O2 -s -DNDEBUG -funroll-loops -floop-optimize -march=i686 -mtune=i686 -pedantic

WFLAGS += -Wall -Wshadow `pkg-config --cflags glib-2.0`
CFLAGS += -std=c99 $(WFLAGS)
CPPFLAGS += $(WFLAGS)
LDFLAGS += -s

ifdef DEBUG
CPPFLAGS += -pipe -Wall -O2 -g3 -D_REENTRANT
CFLAGS += $(CPPFLAGS)
LDFLAGS =
else
CPPFLAGS += -pipe -O2
CFLAGS += $(CPPFLAGS)
endif

%.html : %.txt ;
	asciidoc -b xhtml11 $^

%.gz : %.txt ;
	a2x -f manpage $^ 2>&1 |grep -v '^Note: ' >&2
	gzip -f --best $(patsubst %.txt,%, $^)

PROGS = sh-wrapper \
		event-viewer \
		bench


all: bin doc
bin: $(PROGS)

doc: doc_man doc_html
doc_html:
	make -C doc $^
doc_man:
	make -C doc $^

bench: bench.cpp
#	$(CXX) $(CPPFLAGS) $(LDFLAGS) bench.cpp -o bench

sh-wrapper: sh-wrapper.c
	$(CC) $(CFLAGS) $(LDFLAGS) sh-wrapper.c -o sh-wrapper

process.o: process.h process.cpp

event-viewer: event-viewer.cpp process.o
	$(CXX) $(CPPFLAGS) `pkg-config --cflags glib-2.0` $(LDFLAGS) `pkg-config --libs glib-2.0` $^ -o event-viewer


install: all
	$(install_) -d -m 755 $(bin)
	$(install_) -m 755 sh-wrapper $(bin)

	$(install_) -d -m 755 $(usrbin)
	$(install_) -m 755 bench $(usrbin)
	$(install_) -m 755 upgrade-bloatscanner $(usrbin)
	$(install_) -m 755 grml-kdiff $(usrbin)

	$(install_) -d -m 755 $(usrsbin)
	$(install_) -m 755 event-viewer $(usrsbin)
	$(install_) -m 755 grml-kerneltest $(usrsbin)

	$(install_) -d -m 755 $(usrdoc)
	$(install_) -m 644 $(wildcard doc/*.html) $(usrdoc)

clean:
	rm -f $(PROGS) *.o *.so
	make -C doc clean

