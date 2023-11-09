/* smallchat.c -- Read clients input, send to all the other connected clients.
 *
 * Copyright (c) 2023, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the project name of nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
/* for windows socket use */
//#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <Winsock2.h>
#include <WS2tcpip.h>
#include <assert.h>
/* Need to link with Ws2_32.lib */
#pragma comment(lib, "Ws2_32.lib")

/* ============================ Data structures =================================
 * The minimal stuff we can afford to have. This example must be simple
 * even for people that don't know a lot of C.
 * =========================================================================== */

#define MAX_CLIENTS 1000
#define SERVER_PORT "7711"

/* This structure represents a connected client. There is very little
 * info about it: the socket descriptor and the nick name, if set, otherwise
 * the first byte of the nickname is set to 0 if not set.
 * The client can set its nickname with /nick <nickname> command. */
typedef struct client {
    int     fd;     // Client socket.
    char *nick;     // Nickname of the client.
} client;

/* This global structure encapsulates the global state of the chat. */
typedef struct chatState {
    SOCKET serversock;     // Listening server socket.
    int numclients;     // Number of connected clients right now.
    int maxclient;      // The greatest 'clients' slot populated.
    struct client *clients[MAX_CLIENTS]; // Clients are set in the corresponding
                                         // slot of their socket descriptor.
} chatState;

chatState* Chat;        // Initialized at startup.

/* ======================== Low level networking stuff ==========================
 * Here you will find basic socket stuff that should be part of
 * a decent standard C library, but you know... there are other
 * crazy goals for the future of C: like to make the whole language an
 * Undefined Behavior.
 * =========================================================================== */

/* Create a TCP socket listening to 'port' ready to accept connections. */
SOCKET createTCPServer(const char* port) {
    SOCKET s;
    int yes = 1;
    /* in win sock, `addrinfo` is a best choice */
    struct addrinfo sa, *result = NULL;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) return -1;
    /* in win sock, the type of optval is `const char*` */
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    memset(&sa, 0, sizeof(sa));
    sa.ai_family    = AF_INET;
    sa.ai_socktype  = SOCK_STREAM;
    sa.ai_protocol  = IPPROTO_TCP;
    sa.ai_flags     = AI_PASSIVE;

    /* Resolve the server address and port */
    if (getaddrinfo(NULL, port, &sa, &result)) {
        perror("getaddrinfo error");
        closesocket(s);
        return -1;
    }

    s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (s == INVALID_SOCKET) {
        freeaddrinfo(result);
        return -1;
    }
    if (bind(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR ||
            listen(s, 511) == SOCKET_ERROR) {
        perror("Create server error");
        freeaddrinfo(result);
        closesocket(s);
        return -1;
    }
    freeaddrinfo(result);
    return s;
}

/* Set the specified socket in non-blocking mode, with no delay flag. */
int socketSetNonBlockNoDelay(int fd) {
    int yes = 1;

    /* Set the socket nonblocking.
     * Note that fcntl(2) for F_GETFL and F_SETFL can't be
     * interrupted by a signal.
    if ((flags = fcntl(fd, F_GETFL)) == -1) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return -1; */

    /* Set the socket nonblocking
     * use `ioctlsocket` just ok
     * if arg  = 0, blocking is enabled
     *    arg != 0, non-blocking is enabled */
    if (ioctlsocket(fd, FIONBIO, (u_long*)&yes) == SOCKET_ERROR) return -1;
    /* This is best-effort. No need to check for errors. */
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof(yes));
    return 0;
}

/* If the listening socket signaled there is a new connection ready to
 * be accepted, we accept(2) it and return -1 on error or the new client
 * socket on success. */
int acceptClient(SOCKET server_socket) {
    SOCKET s;

    while (1) {
        s = accept(server_socket, NULL, NULL);
        printf("accept sock: %llu\n", s);
        if (s == SOCKET_ERROR) {
            if (errno == EINTR)
                continue;
            else
                return -1;
        }
        break;
    }
    return s;
}

/* We also define an allocator that always crashes on out of memory: you
 * will discover that in most programs designed to run for a long time, that
 * are not libraries, trying to recover from out of memory is often futile
 * and at the same time makes the whole program terrible. */
void* chatMalloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr == NULL) {
        perror("Out of memory");
        exit(1);
    }
    return ptr;
}

/* Free a client, associated resources, and unbind it from the global
 * state in Chat. */
void freeClient(client *c) {
    free(c->nick);
    _close(c->fd);
    Chat->clients[c->fd] = NULL;
    Chat->numclients--;
    if (Chat->maxclient == c->fd) {
        /* Ooops, this was the max client set. Let's find what is
         * the new highest slot used. */
        int j;
        for (j = Chat->maxclient - 1; j >= 0; j--) {
            if (Chat->clients[j] != NULL) Chat->maxclient = j;
            break;
        }
        if (j == -1) Chat->maxclient = -1;  // We no longer have clients.
    }
    free(c);
}

/* Allocate and init the global stuff. */
void initChat(void) {
    Chat = chatMalloc(sizeof(*Chat));
    memset(Chat, 0, sizeof (*Chat));
    /* No clients at startup, of course. */
    Chat->maxclient  = -1;
    Chat->numclients = 0;

    /* Create our listening socket, bound to the given port. This
     * is where our clients will connect. */
    Chat->serversock = createTCPServer(SERVER_PORT);
    printf("gain serversock: %llu\n", Chat->serversock);
    if (Chat->serversock == -1) {
        perror("Creating listening socket");
        WSACleanup();
        exit(1);
    }
}

client* createClient(int fd) {
    char nick[32];                              // Used to create an initial nick for the user.
    int nicklen = snprintf(nick, sizeof (nick), "user:%d", fd);
    client *c = chatMalloc(sizeof (*c));
    socketSetNonBlockNoDelay(fd);               // Pretend this will not fail.
    c->fd   = fd;
    c->nick = nick;
    memcpy(c->nick, nick, nicklen);
    assert(Chat->clients[c->fd] == NULL);       // This should be available.
    if (c->fd > Chat->maxclient) Chat->maxclient = c->fd;
    Chat->numclients++;
    return c;
}

/* Send the specified string to all connected clients but the one
 * having as socket descriptor 'excluded'. If you want to send something
 * to every client just set excluded to an impossible socket: -1. */
void sendMsgToAllClientsBut(int excluded, char* s, size_t len) {
    for (int j = 0; j <= Chat->maxclient; j++) {
        if (Chat->clients[j] == NULL || Chat->clients[j]->fd == excluded)
            continue;

        /* Important: we don't do ANY BUFFERING. We just use the kernel
         * socket buffers. If the content does not fit, we don't care.
         * This is needed in order to keep this program simple. */
        _write(Chat->clients[j]->fd,s,len);
    }
}

int __cdecl main() {
    /* init windows socket data */
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata)) {
        perror("WSAStartup failed");
        WSACleanup();
        exit(1);
    }
    initChat();

    while (1) {
        fd_set readfds;
        struct timeval tv;
        int retval;

        FD_ZERO(&readfds);
        /* When we want to be notified by select() that there is
         * activity? If the listening socket has pending clients to accept
         * or if any other client wrote anything. */
        FD_SET(Chat->serversock, &readfds);

        for (int j = 0; j <= Chat->maxclient; j++) {
            if (Chat->clients[j]) FD_SET(j, &readfds);
        }

        /* Set a timeout for select(), see later why this may be useful
         * in the future (not now). */
        tv.tv_sec = 1;  // 1 sec timeout
        tv.tv_usec = 0;

        /* Select wants as first argument the maximum file descriptor
         * in use plus one. It can be either one of our clients or the
         * server socket itself. */
        int maxfd = Chat->maxclient;
        if (maxfd < Chat->serversock) maxfd = Chat->serversock;
        retval = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (retval == -1) {
            perror("select() error");
            exit(1);
        } else if (retval) {
             /* If the listening socket is "readable", it actually means
             * there are new clients connections pending to accept. */
             if (FD_ISSET(Chat->serversock, &readfds)) {
                 int fd = acceptClient(Chat->serversock);
                 client *c = createClient(fd);
                 /* Send a welcome message. */
                 char *welcome_msg =
                         "Welcome to Simple Chat! "
                         "Use /nick <nick> to set your nick.\r\n";
                 // write(c->fd, welcome_msg, strlen(welcome_msg));
                 /* can't use _write */
                 send(c->fd, welcome_msg, strlen(welcome_msg), 0);
                 printf("Connected client fd=%d\n", fd);
             }

             /* Here for each connected client, check if there are pending
             * data the client sent us. */
             char readbuf[256];
             for (int j = 0; j <= Chat->maxclient; j++) {
                 if (Chat->clients[j] == NULL) continue;
                 if (FD_ISSET(j, &readfds)) {
                     /* Here we just hope that there is a well formed
                     * message waiting for us. But it is entirely possible
                     * that we read just half a message. In a normal program
                     * that is not designed to be that simple, we should try
                     * to buffer reads until the end-of-the-line is reached. */
                     // int nread = read(j, readbuf, sizeof(readbuf) - 1);
                     int nread = recv(j, readbuf, sizeof(readbuf) - 1, 0);

                     if (nread <= 0) {
                         /* Error or short read means that the socket
                         * was closed. */
                         printf("Disconnected client fd=%d, nick=%s\n", j, Chat->clients[j]->nick);
                         freeClient(Chat->clients[j]);
                     } else {
                         /* The client sent us a message. We need to
                         * relay this message to all the other clients
                         * in the chat. */
                         client *c = Chat->clients[j];
                         readbuf[nread] = 0;

                         /* If the user message starts with "/", we
                         * process it as a client command. So far
                         * only the /nick <newnick> command is implemented. */
                         if (readbuf[0] == '/') {
                             char* p;
                             p = strchr(readbuf, '\r'); if (p) *p = 0;
                             p = strchr(readbuf, '\n'); if (p) *p = 0;
                             /* Check for an argument of the command, after
                             * the space. */
                             char* arg = strchr(readbuf, ' ');
                             if (arg) {
                                 *arg = 0;  /* Terminate command name. */
                                 arg++;     /* Argument is 1 byte after the space. */
                             }
                             printf("arg: %s", arg);

                             if (!strcmp(readbuf, "/nick") && arg) {
                                 free(c->nick);
                                 int nicklen = strlen(arg);
                                 c->nick = chatMalloc(nicklen + 1);
                                 memcpy(c->nick, arg, nicklen + 1);
                             } else {
                                 /* Unsupported command. Send an error. */
                                char *errmsg = "Unsupported command\n";
                                _write(c->fd,errmsg,strlen(errmsg));
                             }
                         } else {
                             /* Create a message to send everybody (and show
                             * on the server console) in the form:
                             *   nick> some message. */
                             char msg[256];
                             int msglen = snprintf(msg, sizeof msg, "%s> %s", c->nick, readbuf);
                             if (msglen >= (int) sizeof(msg))
                                 msglen = sizeof(msg) - 1;
                             printf("%s", msg);

                             /* Send it to all the other clients. */
                             sendMsgToAllClientsBut(j,msg,msglen);
                         }
                     }
                 }
             }
        } else {
            /* Timeout occurred. We don't do anything right now, but in
             * general this section can be used to wakeup periodically
             * even if there is no clients activity. */
        }
    }

    WSACleanup();
    /* drop windows socket */
    return 0;
}