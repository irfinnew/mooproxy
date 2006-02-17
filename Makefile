CFLAGS += -Wall -g
LFLAGS = -Wall -lcrypt
BINDIR = /usr/local/bin
MANDIR = /usr/local/share/man/man1

OBJS = mooproxy.o misc.o config.o daemon.o world.o network.o command.o \
	mcp.o log.o accessor.o timer.o resolve.o crypt.o line.o panic.o

all: mooproxy

mooproxy: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o mooproxy
#	strip mooproxy

clean:
	rm -f *.o core *.core mooproxy

install: mooproxy
	install -d $(BINDIR) $(MANDIR)
	install mooproxy $(BINDIR)
	install mooproxy.1 $(MANDIR)
