ifeq ($(OS), Windows_NT)
	RM = del /Q
else
	RM = rm -f
endif
all: smallchat

smallchat: smallchat.c
	$(CC) smallchat.c -o smallchat -O2 -Wall -W -std=c99

smallchat_win: smallchat_win.c
	$(CC) smallchat.c /Fe:smallchat_win /O2 /Wall /std:c99

clean:
	$(RM) smallchat
