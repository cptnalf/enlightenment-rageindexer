
CC=gcc
CFLAGS=-g -Wall -I/data/0/opt/include
LIBS=-L/data/0/opt/lib -lecore -levas -lsqlite3

all: sqlite_test

sqlite_test: main.o
	$(CC) $(CFLAGS) $(LIBS) main.o -o $@

clean:
	rm -f sqlite_test main.o

