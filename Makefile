
CC=gcc
CFLAGS=-g -Wall `pkg-config evas ecore ecore-file eina-0 --cflags`
LIBS=-lsqlite3 `pkg-config evas ecore ecore-file eina-0 --libs`

all: rage_indexer

rage_indexer: main.o volume.o database.o
	$(CC) $(CFLAGS) $(LIBS) main.o database.o volume.o -o $@

unknown_eets: unknown_eets.o volume.o database.o sha1.o
	$(CC) $(CFLAGS) $(LIBS) unknown_eets.o volume.o database.o sha1.o -o $@

clean:
	rm -f rage_indexer main.o volume.o database.o unknown_eets.o sha1.o
