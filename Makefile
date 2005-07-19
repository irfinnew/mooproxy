#CFLAGS = -Wall -O2
CFLAGS = -Wall -g
LFLAGS = -Wall

OBJS = mooproxy.o misc.o config.o daemon.o world.o network.o command.o \
	mcp.o log.o accessor.o timer.o

all: mooproxy

mooproxy: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o mooproxy
#	strip mooproxy

clean:
	$(RM) *.o core *.core mooproxy
