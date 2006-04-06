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
man8 = $(usr)/share/man/man8/
man1 = $(usr)/share/man/man1/

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
	asciidoc -d manpage -b docbook $^
	sed -i 's/<emphasis role="strong">/<emphasis role="bold">/g' `echo $^ |sed -e 's/.txt/.xml/'`
	xsltproc /usr/share/xml/docbook/stylesheet/nwalsh/manpages/docbook.xsl `echo $^ |sed -e 's/.txt/.xml/'`
	gzip -f --best `echo $^ |sed -e 's/.txt//'`


PROGS = sh-wrapper event-viewer bench
bin: $(PROGS)

default: bin doc

doc: doc_man doc_html

doc_html: sh-wrapper.8.html event-viewer.8.html bench.1.html grml-kerneltest.8.html
sh-wrapper.8.html: sh-wrapper.8.txt
event-viewer.8.html: event-viewer.8.txt
bench.1.html: bench.1.txt
grml-kerneltest.8.html: grml-kerneltest.8.txt

doc_man: sh-wrapper.8.gz event-viewer.8.gz bench.1.gz grml-kerneltest.8.gz
sh-wrapper.8.gz: sh-wrapper.8.txt
event-viewer.8.gz: event-viewer.8.txt
bench.1.gz: bench.1.txt
grml-kerneltest.8.gz: grml-kerneltest.8.txt


bench: bench.cpp
#	$(CXX) $(CPPFLAGS) $(LDFLAGS) bench.cpp -o bench

sh-wrapper: sh-wrapper.c
	$(CC) $(CFLAGS) $(LDFLAGS) sh-wrapper.c -o sh-wrapper

process.o: process.h process.cpp

event-viewer: event-viewer.cpp process.o
	$(CXX) $(CPPFLAGS) `pkg-config --cflags glib-2.0` $(LDFLAGS) `pkg-config --libs glib-2.0` $^ -o event-viewer


install: default
	$(install_) -d -m 755 $(bin)
	$(install_) -m 755 sh-wrapper $(bin)

	$(install_) -d -m 755 $(usrbin)
	$(install_) -m 755 bench $(usrbin)

	$(install_) -d -m 755 $(usrsbin)
	$(install_) -m 755 event-viewer $(usrsbin)
	$(install_) -m 755 grml-kerneltest $(usrsbin)

	$(install_) -d -m 755 $(usrdoc)
	$(install_) -m 644 sh-wrapper.8.html $(usrdoc)
	$(install_) -m 644 event-viewer.8.html $(usrdoc)
	$(install_) -m 644 bench.1.html $(usrdoc)
	$(install_) -m 644 grml-kerneltest.8.html $(usrdoc)
	
	$(install_) -d -m 755 $(man8)
	$(install_) -m 644 sh-wrapper.8.gz $(man8)
	$(install_) -m 644 event-viewer.8.gz $(man8)
	$(install_) -m 644 grml-kerneltest.8.gz $(man8)

	$(install_) -d -m 755 $(man1)
	$(install_) -m 644 bench.1.gz $(man1)


clean: $(SUBDIRS)
	rm -f $(PROGS) *.o *.so \
		sh-wrapper.8.html sh-wrapper.8.xml sh-wrapper.8.gz \
		event-viewer.8.html event-viewer.8.xml event-viewer.8.gz \
		bench.1.html bench.1.xml bench.1.gz \
		grml-kerneltest.8.html grml-kerneltest.8.xml grml-kerneltest.8.gz
