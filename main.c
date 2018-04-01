#include <stdio.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <unistd.h>
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
    printf("Server up and running at port %s\n", argv[1]);

    int rc;
    while (true) {
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
    }
    return 0;
}
