
CC=gcc
CFLAGS=-g -Wall `pkg-config evas ecore ecore-file eina-0 --cflags`
LIBS=-lsqlite3 `pkg-config evas ecore ecore-file eina-0 --libs`

all: rage_indexer

rage_indexer: main.o volume.o database.o
	$(CC) $(CFLAGS) $(LIBS) main.o database.o volume.o -o $@

clean:
	rm -f rage_indexer main.o volume.o database.o
