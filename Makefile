
GIT_VERSION:=\"`git rev-parse HEAD`\"
CC=gcc
CFLAGS=-g -Wall `pkg-config evas ecore ecore-file eina --cflags` -DVERSION=$(GIT_VERSION)
LIBS=-lsqlite3 `pkg-config evas ecore ecore-file eina --libs`

OBJS=main.o volume.o database.o sha1.o

all: rage_indexer

rage_indexer: $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) $(OBJS) -o $@

unknown_eets: unknown_eets.o volume.o database.o sha1.o
	$(CC) $(CFLAGS) $(LIBS) unknown_eets.o volume.o database.o sha1.o -o $@

clean:
	rm -f rage_indexer unknown_eets $(OBJS)
