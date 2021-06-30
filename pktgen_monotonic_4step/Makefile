CFLAGS=-Wall -Werror -std=gnu11 -O2
LDFLAGS=-lpthread
CC=gcc

all: pktgen

OBJS = main.o pktgen_common.o
pktgen: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	rm -rf *.o pktgen

install:
	cp pktgen /usr/local/bin/
