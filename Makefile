CFLAGS += -Wall -g
LFLAGS = -Wall -lcrypt
BINDIR = /usr/local/bin
MANDIR = /usr/local/share/man/man1

OBJS = mooproxy.o misc.o config.o daemon.o world.o network.o command.o \
	mcp.o log.o accessor.o timer.o resolve.o crypt.o line.o panic.o \
	recall.o

all: mooproxy

mooproxy: $(OBJS)
	$(CC) $(OBJS) $(LFLAGS) -o mooproxy
#	strip mooproxy

# If a header file changed, maybe some data formats changed, and all object
# files using it must be recompiled.
# Rather than mapping the actual header-usage relations, we just recompile
# all object files when any header file changed.
*.o: *.h

clean:
	rm -f *.o core *.core mooproxy

install: mooproxy
	install -d $(BINDIR) $(MANDIR)
	install mooproxy $(BINDIR)
	install mooproxy.1 $(MANDIR)
