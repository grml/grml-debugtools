CXX = g++
CC = gcc
LD = $(CXX)

CPPFLAGS = -pipe -Wall -O2 -g3 -D_REENTRANT
#CPPFLAGS = -pipe -Wall -O2 -s -DNDEBUG -funroll-loops -floop-optimize -march=i686 -mtune=i686 -pedantic
CFLAGS = -std=c99 $(CPPFLAGS)
#LDFLAGS = -s

SUBDIRS=$(patsubst %/Makefile,%,$(wildcard */Makefile))
.PHONY: default clean $(SUBDIRS)

#%.so : %.cpp ;
#	$(CC) -shared $*.cpp $(CPPFLAGS) $(LDFLAGS) -o $*.so

PROGS = sh sh-debug
default: $(SUBDIRS) $(PROGS)
all: sh

sh: sh.c
CPPFLAGS += `pkg-config --cflags glib-2.0`
LDFLAGS += `pkg-config --libs glib-2.0`
sh-debug: sh-debug.cpp

clean: $(SUBDIRS)
	rm -f $(PROGS) *~ *.o *.so sh

#main: $(patsubst %.cpp,%.o,$(wildcard *.cpp))

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

