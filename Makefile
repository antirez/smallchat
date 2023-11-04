all: smallchat

smallchat: smallchat.c
	$(CC) smallchat.c -o smallchat -O2 -Wall -W -std=c99

test: smallchat
	@echo please, make sure smallchat is running
	python3 -m unittest tests.py

clean:
	rm -f smallchat
