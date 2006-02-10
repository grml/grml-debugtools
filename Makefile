CXX = g++
CC = gcc

install_ = install

etc = ${DESTDIR}/etc/
bin = ${DESTDIR}/bin
usr = ${DESTDIR}/usr
usrbin = $(usr)/bin
usrsbin = $(usr)/sbin
usrshare = $(usr)/share/$(name_)
usrdoc = $(usr)/share/doc/$(name_)
man8 = $(usr)/share/man/man8/


#CPPFLAGS = -pipe -Wall -O2 -g3 -D_REENTRANT
#CPPFLAGS = -pipe -Wall -O2 -s -DNDEBUG -funroll-loops -floop-optimize -march=i686 -mtune=i686 -pedantic

WFLAGS += -Wall -Wshadow
CFLAGS += -std=c99 $(WFLAGS)
CPPFLAGS += $(WFLAGS)
LDFLAGS = -s


PROGS = sh-wrapper event-viewer
default: $(SUBDIRS) $(PROGS)

sh-wrapper: sh-wrapper.c
	$(CC) $(CFLAGS) $(LDFLAGS) sh-wrapper.c -o sh-wrapper

event-viewer: event-viewer.cpp
	$(CXX) $(CPPFLAGS) `pkg-config --cflags glib-2.0` $(LDFLAGS) `pkg-config --libs glib-2.0` event-viewer.cpp -o event-viewer

clean: $(SUBDIRS)
	rm -f $(PROGS) *~ *.o *.so


install: $(PROGS)
	$(install_) -d -m 755 $(bin)
	$(install_) -m 755 sh-wrapper $(bin)

	$(install_) -d -m 755 $(usrsbin)
	$(install_) -m 755 event-viewer $(usrsbin)

