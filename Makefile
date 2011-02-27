
GIT_VERSION:=\"`git rev-parse HEAD`\"
CC=gcc
CFLAGS=-g -Wall `pkg-config ecore ecore-file ecore-ipc eina --cflags` -DVERSION=$(GIT_VERSION)
LIBS=`pkg-config  ecore ecore-file ecore-ipc eina --libs`

OBJS=main.o volume.o sha1.o rage_ipc.o fs_mon.o

all: rage_indexer

rage_indexer: $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) $(OBJS) -o $@

unknown_eets: unknown_eets.o volume.o database.o sha1.o
	$(CC) $(CFLAGS) $(LIBS) unknown_eets.o volume.o database.o sha1.o -o $@

test_client: $(OBJS) test_client.o
	$(CC) $(CFLAGS) $(LIBS) rage_ipc.o test_client.o -o $@

clean:
	rm -f rage_indexer unknown_eets test_client test_client.o $(OBJS)
