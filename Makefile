#CFLAGS = -Wall -O2
CFLAGS = -Wall -g
LFLAGS = -Wall -lcrypt

OBJS = mooproxy.o misc.o config.o daemon.o world.o network.o command.o \
	mcp.o log.o accessor.o timer.o resolve.o crypt.o line.o

all: mooproxy

mooproxy: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o mooproxy
#	strip mooproxy

clean:
	rm -f *.o core *.core mooproxy
