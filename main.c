#include <stdio.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include "misc.h"
#include "socket_util.h"
#include "error_handler.h"
#include "http.h"


int main(int argc, char** argv) {
    printf("Hello, World!\n");

    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command-line arguments */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return -1;
    }

    if ((listenfd = open_listenfd(argv[1])) < 0) {
        app_error("Fatal. Cannot open listen socket.");
        return -1;
    }

    /* setup epoll */
    int efd = epoll_create1(0);
    if (efd < 0) {
        unix_error("Fatal. Failed to setup epoll");
        return -1;
    }

    epoll_event_t event;
    event.data.fd = listenfd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &event) < 0) {
        unix_error("Fatal. Failed to add listen fd to epoll");
        return -1;
    }
    epoll_event_t events[MAXEVENT];

    printf("Server up and running at port %s\n", argv[1]);

    return 0;
    int rc;
    int n, i;
    while (true) {
        n = epoll_wait(efd, events, MAXEVENT, -1);
        if (n == -1) {
            unix_error("Fatal. epoll wait failed");
            return -1;
        }
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                app_error("epoll error");
                close(events[i].data.fd); // TODO error handler
                continue;
            }
            handle_request(events[i].data.fd, listenfd);
        }
/*
        clientlen = sizeof(clientaddr);

        connfd = accept(listenfd, (SA*) &clientaddr, &clientlen);
        if (connfd < 0) {
            unix_error("Failed to accept new connection.");
            continue;
        }

        if ((rc = getnameinfo((SA*) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0)) != 0) {
            gai_error(rc, "Failed to get name info");
        } else {
            printf("Accepted connection from (%s, %s)\n", hostname, port);
        }

        handle_conn(connfd);

        if (close(connfd) == -1) {
            unix_error("Failed to connect connection socket.");
        }
*/
    }
    return 0;
}
