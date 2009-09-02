VERSION = 0.8
CC = gcc
CFLAGS = -g -O2
LDFLAGS =
LIBS = 
DEFS =  
INSTALL = /usr/bin/install -c

# Installation directories
prefix = ${DESTDIR}/usr
exec_prefix = ${prefix}
mandir = ${prefix}/share/man/man1
bindir = ${exec_prefix}/bin
etcdir = ${DESTDIR}/etc

INCL = 
SRCS = cd-discid.c
OBJS = $(SRCS:.c=.o)

.SUFFIXES: .c .o

.c.o:
	$(CC) $(DEFS) $(CFLAGS) -c $<

all: cd-discid

cd-discid: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f *~ *.o core cd-discid

install: cd-discid
	$(INSTALL) -d -m 755 $(bindir)
	$(INSTALL) -m 755 cd-discid $(bindir)
	$(INSTALL) -d -m 755 $(mandir)
	$(INSTALL) -m 644 cd-discid.1 $(mandir)

tarball:
	@cd .. && tar czvf cd-discid_$(VERSION).orig.tar.gz \
		cd-discid-$(VERSION)/{COPYING,README,Makefile,cd-discid.1,cd-discid.c,changelog}
