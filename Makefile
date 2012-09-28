VERSION = 1.4

CC ?= cc
RM = rm -f
INSTALL = /usr/bin/install
STRIP = strip

CFLAGS ?= -g -O2
CFLAGS := -DVERSION=\"$(VERSION)\" $(CFLAGS)
CPPFLAGS ?=
LDFLAGS ?=

SRCS = cd-discid.c
OBJS = $(SRCS:.c=.o)

PREFIX ?= /usr/local
BINDIR = ${PREFIX}/bin
MANDIR = ${PREFIX}/share/man/man1

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

all: cd-discid

cd-discid: $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $(OBJS)

install: cd-discid
	$(INSTALL) -D cd-discid $(DESTDIR)$(BINDIR)/cd-discid
	$(STRIP) $(DESTDIR)$(BINDIR)/cd-discid
	$(INSTALL) -D -m 644 cd-discid.1 $(DESTDIR)$(MANDIR)/cd-discid.1

clean:
	$(RM) $(OBJS) cd-discid
