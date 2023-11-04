all: smallchat-server

smallchat-server: smallchat-server.c chatlib.c
	$(CC) smallchat-server.c chatlib.c -o smallchat-server -O2 -Wall -W -std=c99

clean:
	rm -f smallchat-server
