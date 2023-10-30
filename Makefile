all: smallchat

smallchat: smallchat.c
	$(CC) smallchat.c -o smallchat -O2 -Wall -W -std=c99

clean:
	rm -f smallchat
