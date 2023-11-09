all: smallchat-server smallchat-client
CLAGS=-O2 -Wall -W -std=c99

smallchat-server: smallchat-server.c chatlib.c
	$(CC) smallchat-server.c chatlib.c -o smallchat-server $(CFLAGS)

smallchat-client: smallchat-client.c chatlib.c
	$(CC) smallchat-client.c chatlib.c -o smallchat-client $(CFLAGS)

clean:
	rm -f smallchat-server
