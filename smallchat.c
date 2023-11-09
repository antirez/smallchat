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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/select.h>

/* ============================ Data structures =================================
 * The minimal stuff we can afford to have. This example must be simple
 * even for people that don't know a lot of C.
 * =========================================================================== */

#define MAX_CLIENTS 1000 // This is actually the higher file descriptor.
#define MAX_CHANNELS 1000
#define SERVER_PORT 7711

/* This structure represents a connected client. There is very little
 * info about it: the socket descriptor and the nick name, if set, otherwise
 * the first byte of the nickname is set to 0 if not set.
 * The client can set its nickname with /nick <nickname> command. */
struct client {
    int fd;     // Client socket.
    char *nick; // Nickname of the client.
    char *currentchannel; //Current channel of the client
};

struct channel
{   
    int numclients;
    char *name; // Channel name.
    struct client *clients[MAX_CLIENTS]; // Clients subscribe to the channel.
};


/* This global structure encapsulates the global state of the chat. */
struct chatState {
    int serversock;     // Listening server socket.
    int numclients;     // Number of connected clients right now.
    int maxclient;      // The greatest 'clients' slot populated.
    int numchannels;    // Number of created channels right now.
    struct client *clients[MAX_CLIENTS]; // Clients are set in the corresponding
                                         // slot of their socket descriptor.
    struct channel *channels[MAX_CHANNELS];
    
};

struct chatState *Chat; // Initialized at startup.

/* ======================== Low level networking stuff ==========================
 * Here you will find basic socket stuff that should be part of
 * a decent standard C library, but you know... there are other
 * crazy goals for the future of C: like to make the whole language an
 * Undefined Behavior.
 * =========================================================================== */

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

/* ====================== Small chat core implementation ========================
 * Here the idea is very simple: we accept new connections, read what clients
 * write us and fan-out (that is, send-to-all) the message to everybody
 * with the exception of the sender. And that is, of course, the most
 * simple chat system ever possible.
 * =========================================================================== */

/* Create a new client bound to 'fd'. This is called when a new client
 * connects. As a side effect updates the global Chat state. */
struct client *createClient(int fd) {
    char nick[32]; // Used to create an initial nick for the user.
    int nicklen = snprintf(nick,sizeof(nick),"user:%d",fd);
    struct client *c = chatMalloc(sizeof(*c));
    socketSetNonBlockNoDelay(fd); // Pretend this will not fail.
    c->fd = fd;
    c->nick = chatMalloc(nicklen+1);
    memcpy(c->nick,nick,nicklen);
    assert(Chat->clients[c->fd] == NULL); // This should be available.
    Chat->clients[c->fd] = c;
    /* We need to update the max client set if needed. */
    if (c->fd > Chat->maxclient) Chat->maxclient = c->fd;
    Chat->numclients++;
    c->currentchannel = NULL;
    return c;
}

/* Free a client, associated resources, and unbind it from the global
 * state in Chat. */
void freeClient(struct client *c) {
    free(c->nick);
    close(c->fd);
    Chat->clients[c->fd] = NULL;
    Chat->numclients--;
    if (Chat->maxclient == c->fd) {
        /* Ooops, this was the max client set. Let's find what is
         * the new highest slot used. */
        int j;
        for (j = Chat->maxclient-1; j >= 0; j--) {
            if (Chat->clients[j] != NULL) Chat->maxclient = j;
            break;
        }
        if (j == -1) Chat->maxclient = -1; // We no longer have clients.
    }
    free(c);
}

struct channel *createChannel(char *name){
    if (Chat->numchannels >= MAX_CHANNELS){
        perror("Max channel num reached.");
        exit(-1);
    }
    struct channel *ch = chatMalloc(sizeof(*ch));
    ch->name = chatMalloc(strlen(name)+1);
    memcpy(ch->name, name, strlen(name));
    ch->numclients = 0;
    Chat->channels[Chat->numchannels] = ch;
    Chat->numchannels++;
    return ch;
}

struct channel *getChannel(char *name){
    for(int i = 0; i < Chat->numchannels; i++){
        if (Chat->channels[i]&&!strcmp(name, Chat->channels[i]->name)){
            return Chat->channels[i];
        }
    }
    return createChannel(name);
}

void freeChannel(struct channel *ch){
    free(ch->name);
    for(int i = 0; i < Chat->numchannels; i++){
        // remove from Chat->channels
        if (Chat->channels[i]&&!strcmp(ch->name, Chat->channels[i]->name)){
            Chat->channels[i] = NULL;
            Chat->numchannels--;
        }
    }
    free(ch);
}

void recycleChannel(){
    for (int i = 0; i< Chat->numchannels;i++){
        if (Chat->channels[i] && Chat->channels[i]->numclients == 0){
            freeChannel(Chat->channels[i]);
        }
    }
}

/* Allocate and init the global stuff. */
void initChat(void) {
    Chat = chatMalloc(sizeof(*Chat));
    memset(Chat,0,sizeof(*Chat));
    /* No clients at startup, of course. */
    Chat->maxclient = -1;
    Chat->numclients = 0;
    Chat->numchannels = 0;

    /* Create our listening socket, bound to the given port. This
     * is where our clients will connect. */
    Chat->serversock = createTCPServer(SERVER_PORT);
    if (Chat->serversock == -1) {
        perror("Creating listening socket");
        exit(1);
    }
}

/* Send the specified string to all connected clients but the one
 * having as socket descriptor 'excluded'. If you want to send something
 * to every client just set excluded to an impossible socket: -1. */
void sendMsgToAllClientsBut(int excluded, char *s, size_t len) {
    for (int j = 0; j <= Chat->maxclient; j++) {
        if (Chat->clients[j] == NULL ||
            Chat->clients[j]->fd == excluded || Chat->clients[j]->currentchannel != NULL) continue;

        /* Important: we don't do ANY BUFFERING. We just use the kernel
         * socket buffers. If the content does not fit, we don't care.
         * This is needed in order to keep this program simple. */
        if (write(Chat->clients[j]->fd,s,len) == -1){
                perror("Sending message");
        }
    }
}

void writeChannelNameToClient(int fd, char *channelname){
    if (!channelname){
        return;
    }
    char *channelmsg = chatMalloc(strlen(channelname)+3);
    sprintf(channelmsg, "(%s) ", channelname);
    if (write(fd,channelmsg,strlen(channelmsg)) == -1){
            perror("Writing error message");
    }
}

void sendMsgToChannelClientsBut(int excluded, char *channelname, char *s, size_t len) {
    struct channel *ch = getChannel(channelname);

    for (int j = 0; j <= Chat->maxclient; j++) {
        if (ch->clients[j] == NULL ||
            ch->clients[j]->fd == excluded) continue;

        /* Important: we don't do ANY BUFFERING. We just use the kernel
         * socket buffers. If the content does not fit, we don't care.
         * This is needed in order to keep this program simple. */
        if (write(ch->clients[j]->fd,s,len) == -1){
                perror("Sending message");
        }
        writeChannelNameToClient(ch->clients[j]->fd, channelname);
    }
}

/* The main() function implements the main chat logic:
 * 1. Accept new clients connections if any.
 * 2. Check if any client sent us some new message.
 * 3. Send the message to all the other clients. */
int main(void) {
    initChat();

    while(1) {
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
        tv.tv_sec = 1; // 1 sec timeout
        tv.tv_usec = 0;

        /* Select wants as first argument the maximum file descriptor
         * in use plus one. It can be either one of our clients or the
         * server socket itself. */
        int maxfd = Chat->maxclient;
        if (maxfd < Chat->serversock) maxfd = Chat->serversock;
        retval = select(maxfd+1, &readfds, NULL, NULL, &tv);
        if (retval == -1) {
            perror("select() error");
            exit(1);
        } else if (retval) {

            /* If the listening socket is "readable", it actually means
             * there are new clients connections pending to accept. */
            if (FD_ISSET(Chat->serversock, &readfds)) {
                int fd = acceptClient(Chat->serversock);
                struct client *c = createClient(fd);
                /* Send a welcome message. */
                char *welcome_msg =
                    "Welcome to Simple Chat! "
                    "Use /nick <nick> to set your nick.\n";
                if (write(c->fd,welcome_msg,strlen(welcome_msg)) == -1){
                        perror("Writing welcome message");
                }
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
                    int nread = read(j,readbuf,sizeof(readbuf)-1);

                    if (nread <= 0) {
                        /* Error or short read means that the socket
                         * was closed. */
                        printf("Disconnected client fd=%d, nick=%s\n",
                            j, Chat->clients[j]->nick);
                        freeClient(Chat->clients[j]);
                        recycleChannel();
                    } else {
                        /* The client sent us a message. We need to
                         * relay this message to all the other clients
                         * in the chat. */
                        struct client *c = Chat->clients[j];
                        readbuf[nread] = 0;

                        /* If the user message starts with "/", we
                         * process it as a client command. So far
                         * only the /nick <newnick> command is implemented. */
                        if (readbuf[0] == '/') {
                            /* Remove any trailing newline. */
                            char *p;
                            p = strchr(readbuf,'\r'); if (p) *p = 0;
                            p = strchr(readbuf,'\n'); if (p) *p = 0;
                            /* Check for an argument of the command, after
                             * the space. */
                            char *arg = strchr(readbuf,' ');
                            if (arg) {
                                *arg = 0; /* Terminate command name. */
                                arg++; /* Argument is 1 byte after the space. */
                            }

                            if (!strcmp(readbuf,"/nick") && arg) {
                                free(c->nick);
                                int nicklen = strlen(arg);
                                c->nick = chatMalloc(nicklen+1);
                                memcpy(c->nick,arg,nicklen+1);
                            }else if (!strcmp(readbuf, "/channel") && arg){
                                struct channel *ch = getChannel(arg);
                                int channelnamelen = strlen(ch->name);
                                c->currentchannel = chatMalloc(channelnamelen+1);
                                memcpy(c->currentchannel,ch->name,channelnamelen+1);
                                ch->clients[c->fd] = c;
                                ch->numclients++;
                                char *welcomemsg = chatMalloc(channelnamelen+1);
                                sprintf(welcomemsg, "Welcome to channel %s.\n(%s) ", c->currentchannel, c->currentchannel);
                                if (write(c->fd,welcomemsg,strlen(welcomemsg)) == -1){
                                        perror("Writing error message");
                                }
                                recycleChannel();
                            }else if (!strcmp(readbuf, "/public")){
                                struct channel *ch = getChannel(c->currentchannel);
                                ch->clients[c->fd] = NULL;
                                c->currentchannel = NULL;
                                ch->numclients--;
                                char *welcomemsg = "Back to public channel.\n";
                                if (write(c->fd,welcomemsg,strlen(welcomemsg)) == -1){
                                        perror("Writing error message");
                                }
                                recycleChannel();
                            }
                            else {
                                /* Unsupported command. Send an error. */
                                char *errmsg = "Unsupported command\n";
                                if (write(c->fd,errmsg,strlen(errmsg)) == -1){
                                        perror("Writing error message");
                                }
                                if (c->currentchannel){
                                    writeChannelNameToClient(c->fd, c->currentchannel);
                                }
                            }
                        } else {
                            /* Create a message to send everybody (and show
                             * on the server console) in the form:
                             *   nick> some message. */
                            char msg[256];
                            int msglen = snprintf(msg, sizeof(msg),
                                "%s> %s", c->nick, readbuf);
                            if (msglen >= (int)sizeof(msg))
                                msglen = sizeof(msg)-1;
                            printf("%s",msg);
                            if (c->currentchannel){
                                writeChannelNameToClient(c->fd, c->currentchannel);
                                sendMsgToChannelClientsBut(j, c->currentchannel, msg, msglen);
                            }else{
                                /* Send it to all the other clients. */
                                sendMsgToAllClientsBut(j,msg,msglen);
                            }
                                                  
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
    return 0;
}