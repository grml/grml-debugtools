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
	#sed -i 's/<emphasis role="strong">/<emphasis role="bold">/g' `echo $^ |sed -e 's/.txt/.xml/'`
	xsltproc -nonet /usr/share/xml/docbook/stylesheet/nwalsh/manpages/docbook.xsl `echo $^ |sed -e 's/.txt/.xml/'` >/dev/null
	# ugly hack to avoid '.sp' at the end of a sentence or paragraph:
	sed -i 's/\.sp//' `echo $^ |sed -e 's/.txt//'`
	gzip -f --best `echo $^ |sed -e 's/.txt//'`

PROGS = sh-wrapper \
		event-viewer \
		bench
MANPAGES = sh-wrapper.8 \
		   event-viewer.8 \
		   bench.1 \
		   grml-kerneltest.8 \
		   upgrade-bloatscanner.1 \
		   grml-kernelconfig.1

all: bin doc
bin: $(PROGS)

doc: doc_man doc_html
doc_html: $(addsuffix .html, $(MANPAGES))
doc_man: $(addsuffix .gz, $(MANPAGES))

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
	$(install_) -m 755 grml-kernelconfig $(usrbin)

	$(install_) -d -m 755 $(usrsbin)
	$(install_) -m 755 event-viewer $(usrsbin)
	$(install_) -m 755 grml-kerneltest $(usrsbin)

	$(install_) -d -m 755 $(usrdoc)
	$(install_) -m 644 sh-wrapper.8.html $(usrdoc)
	$(install_) -m 644 event-viewer.8.html $(usrdoc)
	$(install_) -m 644 bench.1.html $(usrdoc)
	$(install_) -m 644 grml-kerneltest.8.html $(usrdoc)
	$(install_) -m 644 upgrade-bloatscanner.1.html $(usrdoc)
	$(install_) -m 644 grml-kernelconfig.1.html $(usrdoc)
	
	$(install_) -d -m 755 $(man8)
	$(install_) -m 644 sh-wrapper.8.gz $(man8)
	$(install_) -m 644 event-viewer.8.gz $(man8)
	$(install_) -m 644 grml-kerneltest.8.gz $(man8)

	$(install_) -d -m 755 $(man1)
	$(install_) -m 644 bench.1.gz $(man1)
	$(install_) -m 644 upgrade-bloatscanner.1.gz $(man1)
	$(install_) -m 644 grml-kernelconfig.1.gz $(man1)


clean:
	rm -f $(PROGS) *.o *.so
	@for i in $(MANPAGES); do \
		rm -f $$i.html $$i.xml $$i.gz; done

