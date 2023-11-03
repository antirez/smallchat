all: smallchat

smallchat: smallchat.c
	$(CC) chat.c -o smallchat -O2 -Wall -W

clean:
	rm -f smallchat
