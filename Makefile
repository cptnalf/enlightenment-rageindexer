
GIT_VERSION:=\"`git rev-parse HEAD`\"
CC=gcc
CFLAGS=-g -Wall `pkg-config evas ecore ecore-file --cflags` -DVERSION=$(GIT_VERSION)
LIBS=-lsqlite3 `pkg-config evas ecore ecore-file --libs`

OBJS=main.o volume.o database.o sha1.o

all: rage_indexer

rage_indexer: $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) $(OBJS) -o $@

clean:
	rm -f rage_indexer $(OBJS)
