BUILD_DIR = build

all: $(BUILD_DIR) $(BUILD_DIR)/smallchat-server $(BUILD_DIR)/smallchat-client
CFLAGS=#-O2 -Wall -W -std=c99

$(BUILD_DIR)/smallchat-server: smallchat-server.c chatlib.c
	$(CC) smallchat-server.c chatlib.c -o $(BUILD_DIR)/smallchat-server $(CFLAGS)

$(BUILD_DIR)/smallchat-client: smallchat-client.c chatlib.c
	$(CC) smallchat-client.c chatlib.c -o $(BUILD_DIR)/smallchat-client $(CFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

