#define _POSIX_C_SOURCE 200112L
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================== Low level networking stuff ==========================
 * Here you will find basic socket stuff that should be part of
 * a decent standard C library, but you know... there are other
 * crazy goals for the future of C: like to make the whole language an
 * Undefined Behavior.
 * =========================================================================== */

/* Set the specified socket in non-blocking mode, with no delay flag. */
int socketSetNonBlockNoDelay(int fd) {
    int flags, yes = 1;

    /* Set the socket nonblocking.
     * Note that fcntl(2) for F_GETFL and F_SETFL can't be
     * interrupted by a signal. */
    if ((flags = fcntl(fd, F_GETFL)) == -1) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return -1;

    /* This is best-effort. No need to check for errors. */
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    return 0;
}

/* Create a TCP socket listening to 'port' ready to accept connections. */
int createTCPServer(int port) {
    int s, yes = 1;
    struct sockaddr_in sa;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) return -1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)); // Best effort.

    memset(&sa,0,sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s,(struct sockaddr*)&sa,sizeof(sa)) == -1 ||
        listen(s, 511) == -1)
    {
        close(s);
        return -1;
    }
    return s;
}

/* Create a TCP socket and connect it to the specified address.
 * On success the socket descriptor is returned, otherwise -1.
 *
 * If 'nonblock' is non-zero, the socket is put in nonblocking state
 * and the connect() attempt will not block as well, but the socket
 * may not be immediately ready for writing. */
int TCPConnect(char *addr, int port, int nonblock) {
    int s, retval = -1;
    struct addrinfo hints, *servinfo, *p;

    char portstr[6]; /* Max 16 bit number string length. */
    snprintf(portstr,sizeof(portstr),"%d",port);
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(addr,portstr,&hints,&servinfo) != 0) return -1;

    for (p = servinfo; p != NULL; p = p->ai_next) {
        /* Try to create the socket and to connect it.
         * If we fail in the socket() call, or on connect(), we retry with
         * the next entry in servinfo. */
        if ((s = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
            continue;

        /* Put in non blocking state if needed. */
        if (nonblock && socketSetNonBlockNoDelay(s) == -1) {
            close(s);
            break;
        }

        /* Try to connect. */
        if (connect(s,p->ai_addr,p->ai_addrlen) == -1) {
            /* If the socket is non-blocking, it is ok for connect() to
             * return an EINPROGRESS error here. */
            if (errno == EINPROGRESS && nonblock) return s;

            /* Otherwise it's an error. */
            close(s);
            break;
        }

        /* If we ended an iteration of the for loop without errors, we
         * have a connected socket. Let's return to the caller. */
        retval = s;
        break;
    }

    freeaddrinfo(servinfo);
    return retval; /* Will be -1 if no connection succeded. */
}

/* If the listening socket signaled there is a new connection ready to
 * be accepted, we accept(2) it and return -1 on error or the new client
 * socket on success. */
int acceptClient(int server_socket) {
    int s;

    while(1) {
        struct sockaddr_in sa;
        socklen_t slen = sizeof(sa);
        s = accept(server_socket,(struct sockaddr*)&sa,&slen);
        if (s == -1) {
            if (errno == EINTR)
                continue; /* Try again. */
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
void *chatMalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        perror("Out of memory");
        exit(1);
    }
    return ptr;
}

/* Also aborting realloc(). */
void *chatRealloc(void *ptr, size_t size) {
    ptr = realloc(ptr,size);
    if (ptr == NULL) {
        perror("Out of memory");
        exit(1);
    }
    return ptr;
}
