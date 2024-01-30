CC = gcc
CFLAGS = -Wall -ggdb
LDFLAGS = -lwebsockets -ljansson -lsqlite3

rrccon: main.c config.h
	sudo mkdir -p /var/lib/rrccon
	$(CC) $(CFLAGS) -o rrccon main.c $(LDFLAGS)

release: main.c config.h
	sudo mkdir -p /var/lib/rrccon
	$(CC) $(CFLAGS) -o rrccon main.c $(LDFLAGS) -O2

install: release
	sudo cp rrccon /usr/local/bin

all: rrccon