EXEC = proxy-load
CFLAGS = -Wall
CC = gcc

all: libraries obj/server.o obj/client.o obj/net_util.o obj/net_compat.o main

main: obj/client.o obj/server.o obj/net_util.o obj/tuntap.o obj/net_workers.o
	$(CC) $(CFLAGS) -o $(EXEC) main.c obj/server.o obj/client.o obj/net_util.o obj/net_compat.o -lssl -lcrypto -pthread -L./lib -lsubgetopt

client.c: client.h net/net_util.c net/net_compat.c
obj/client.o: client.c
	$(CC) $(CFLAGS) -c -o obj/client.o client.c

server.c: server.h net/net_util.c net/net_compat.c
obj/server.o: server.c
	$(CC) $(CFLAGS) -c -o obj/server.o server.c

net/net_util.c: net/net_util.h
obj/net_util.o: net/net_util.c
	$(CC) $(CFLAGS) -c -o obj/net_util.o net/net_util.c

net/net_compat.c: net/net_compat.h net/net_util.c
obj/net_compat.o: net/net_compat.c
	$(CC) $(CFLAGS) -c -o obj/net_compat.o net/net_compat.c

libraries:
	$(CC) $(CFLAGS) -c -o obj/subgetopt.o lib/subgetopt.c
	ar rc lib/libsubgetopt.a obj/subgetopt.o
	ranlib lib/libsubgetopt.a

clean:
	-rm -f $(EXEC) obj/*.o lib/*.a
