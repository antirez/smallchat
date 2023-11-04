#ifndef CHATLIB_H
#define CHATLIB_H

int createTCPServer(int port);
int socketSetNonBlockNoDelay(int fd);
int acceptClient(int server_socket);
void *chatMalloc(size_t size);
void *chatRealloc(void *ptr, size_t size);

#endif // CHATLIB_H
