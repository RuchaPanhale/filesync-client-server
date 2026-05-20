CC      = gcc
CFLAGS  = -Wall -Wextra
LDFLAGS =
SRCS    = server.c client.c
OBJS    = $(SRCS:.c=.o)
BINS    = server client

.PHONY: all clean

all: $(BINS)

server: server.o
	$(CC) $(LDFLAGS) $^ -o $@

client: client.o
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJS): common.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BINS)
