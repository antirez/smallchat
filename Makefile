all: smallchat

smallchat: smallchat.c
	$(CC) smallchat.c -o smallchat -O2 -Wall -W -std=c99

test: smallchat
	python3 -m unittest tests.py -v

clean:
	rm -f smallchat
