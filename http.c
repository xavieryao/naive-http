
/*
Copyright 2018 Xavier Yao <xavieryao@me.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <sys/stat.h>
#include <printf.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iso646.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/errno.h>
#include <sys/sendfile.h>
#include "http.h"
#include "error_handler.h"
#include "rio.h"
#include "socket_util.h"
#include "transaction.h"


/* protocol related event-handlers */
void handle_protocol_event(int efd, transaction_t* trans);
void serve_download(int efd, transaction_t* trans);
void serve_upload(int efd, transaction_t* trans);
void finish_transaction(int efd, transaction_t* trans);
void accept_connection(int fd, int efd);
void read_request_header(transaction_t* trans, int efd);
void send_resp_header(int efd, transaction_t* trans);
void client_error(int efd, transaction_t* trans, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);


/* transmission related event-handlers */
void handle_transmission_event(int efd, transaction_t* trans);
void write_n(int efd, transaction_t* trans);
void write_file(int efd, transaction_t* trans);
void read_n(int efd, transaction_t* trans);

/* utility functions */
void parse_uri(char *uri, char *filename);
void get_filetype(char *filename, char *filetype);

/* data structure related functions */

void destroy_headers(http_headers_t *hdrs);
void destroy_header_item(http_header_item_t *item);
void append_header(http_headers_t *hdrs, http_header_item_t *item);

/*
 * Handle HTTP/1.0 transactions
 * Event-based using epoll.
 */
void handle_request(int fd, int listenfd, int efd) {
    printf("handle request.\n");
    if (fd == listenfd) {
        accept_connection(fd, efd);
        // Accept socket
        return;
    }
    int i;
    transaction_t* trans = find_transaction_for_fd(fd);
    if (trans == NULL) {
        app_error("transaction not found.");
        return;
    }
    handle_transmission_event(efd, trans);
    return;
}

void accept_connection(int fd, int efd) {
    printf("accept connection.\n");
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    int connfd;

    clientlen = sizeof(clientaddr);
    while (true) { // edge-trigger mode, poll until accept succeeds
        connfd = accept(fd, (SA*) &clientaddr, &clientlen);
        if (connfd < 0) { /* not ready */
            if (not (errno == EAGAIN || errno == EWOULDBLOCK)) {
                unix_error("accept");
            }
            break;
        }

        if (set_nonblocking(connfd) == ERROR) {
            unix_error("set conn socket nonblocking");
            close(connfd);
            return;
        }
        /* add to epoll */
        epoll_event_t event;
        event.data.fd = connfd;
        event.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &event) == ERROR) {
            unix_error("epoll add conn socket");
            close(connfd);
            return;
        }

        transaction_t* slot = find_empty_transaction_for_fd(connfd);
        if (slot == NULL) {
            close(connfd); /* Reached transaction limit */
            return;
        }
        init_transaction(slot);
        slot->fd = connfd;
        slot->state = S_READ_REQ_HEADER;
    }
}



void read_request_header(transaction_t* trans, int efd) {
    printf("read request header.\n");
    ssize_t count;
    while (trans->read_pos <= MAXBUF-1) {
        count = read(trans->fd, trans->read_buf+trans->read_pos, MAXBUF-trans->read_pos);
        if (count < 0) {
            if (errno != EAGAIN) {
                unix_error("failed to read");
                client_error(efd, trans, "", "400", "Bad Request", "Failed to read request line & header");
                return;
            } else { /* EAGAIN: done reading */
                break;
            }
        } else if (count == 0) { /* Client closed connection */
            printf("client closed.\n");
            finish_transaction(efd, trans);
            return;
        } else {
            printf("read %d bytes.\n", count);
            trans->read_pos += count;
        }
    }

    if (trans->read_pos > MAXBUF - 1) { /* Buffer full */
        client_error(efd, trans, "", "400", "Bad Request", "Request header too long");
        return;
    }

    /* Search for end of header "\r\n\r\n" */
    printf("looking for end-of-header\n");
    const char header_tail[] = "\r\n\r\n";
    int header_tail_len = 4;
    int i;
    bool read_header_tail = false;
    for (trans->parse_pos=0; trans->parse_pos <= trans->read_pos - header_tail_len; trans->parse_pos++) {
        read_header_tail = true;
        for (i = 0; i < header_tail_len; i ++) {
            if (trans->read_buf[trans->parse_pos+i] != header_tail[i]) {
                read_header_tail = false;
                continue;
            }
        }
        if (read_header_tail) break;
    }
    if (not (read_header_tail)) {
        printf("Haven't read entire header.\n");
        return; /* haven't read the entire header */
    }

    printf("read entire header at %d.\n", trans->parse_pos);
    /* parse request line and header */
    if (sscanf(trans->read_buf, "%s %s %s", trans->method, trans->uri, trans->version) != 3) {
        client_error(efd, trans, "", "400", "Bad Request", "Invalid request line");
        return;
    }
    printf("Request line: [%s] [%s] [%s]\n", trans->method, trans->uri, trans->version);
    char* tofree, *remain, *value_s, *key_s;
    int header_len = trans->parse_pos + header_tail_len;
    tofree = remain = calloc(sizeof(char), header_len + 1);
    strncpy(tofree, trans->read_buf, header_len);

    strsep(&remain, "\r\n"); /* skip request line */
    while ((value_s = strsep(&remain, "\r\n")) != NULL) {
        if (strlen(value_s) == 0) continue;

        key_s = strsep(&value_s, ": ");

        size_t value_len = strlen(value_s);
        if (value_len < 1 || value_s[0] != ' ') {
            free(tofree);
            client_error(efd, trans, "", "400", "Bad Request", "Invalid request header");
            return;
        }
        value_s += 1; // value_s points to the sp of ': ', move to the beginning.

        http_header_item_t *item = (http_header_item_t *) malloc(sizeof(http_header_item_t));
        strncpy(item->key, key_s, MAXLINE);
        strncpy(item->value, value_s, MAXLINE);

        printf("KEY[%s] VALUE[%s]\n", item->key, item->value);

        append_header(&trans->headers, item);
    }
    free(tofree);

    if (strcasecmp(trans->method, "GET") == 0) trans->methodtype = GET;
    else if (strcasecmp(trans->method, "POST") == 0) trans->methodtype = POST;
    else if (strcasecmp(trans->method, "HEAD") == 0) trans->methodtype = HEAD;
    else {
        client_error(efd, trans, trans->method, "501", "Not Implemented",
                    "Naive server does not implement this method");
        return;
    }

    /* Parse URI from request */
    parse_uri(trans->uri, trans->filename);

    /* Check file */
    struct stat sbuf;
    if (trans->methodtype == GET || trans->methodtype == HEAD) {
        if (stat(trans->filename, &sbuf) < 0) {
            client_error(efd, trans, trans->filename, "404", "Not found",
                        "Naive server couldn't find this file");
            return;
        }
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            client_error(efd, trans, trans->filename, "403", "Forbidden",
                        "Naive server couldn't read the file");
            return;
        }
        trans->filesize = sbuf.st_size;
    }
    /* check post header */
    int content_len = -1;
    if (trans->methodtype == POST) {
        http_header_item_t* hdr_item = trans->headers.head;
        while (hdr_item != NULL) {
            if (strcmp(hdr_item->key, "Content-Length") == 0) {
                printf("value [%s]\n", hdr_item->value);
                content_len = strtol(hdr_item->value, NULL, 10);
                if (content_len == 0) {
                    unix_error("strtol failed");
                    content_len = -1;
                }
                break;
            }
            hdr_item = hdr_item->next;
        }
        if (content_len <= 0) {
            client_error(efd, trans, trans->filename, "400", "Bad Request", "Content-Length must be provided and be positive.");
            return;
        }
        if (content_len > MAX_FILE_SIZE) {
            client_error(efd, trans, trans->filename, "400", "Bad Request", "File larger than limit.");
            return;
        }
        trans->filesize = content_len;
    }

    /* transfer state */
    epoll_event_t event;
    int pos_i, pos_j;
    switch (trans->methodtype) {
        case GET:
        case HEAD:
            trans->state = S_WRITE;
            trans->next_stage = P_SEND_RESP_HEADER;
            event.data.fd = trans->fd;
            event.events = EPOLLOUT | EPOLLET;
            if (epoll_ctl(efd, EPOLL_CTL_MOD, trans->fd, &event) < 0) {
                unix_error("epoll ctl");
            }
            break;
        case POST:
            /* copy remaining part */
            /* use for-loop instead of memcpy to avoid overlap */
            for (pos_i = 0, pos_j = trans->parse_pos + header_tail_len; pos_j < trans->read_pos; pos_i ++, pos_j++) {
                trans->read_buf[pos_i] = trans->read_buf[pos_j];
            }
            trans->read_pos = pos_i;
            trans->state = S_READ;
            trans->next_stage = P_READ_REQ_BODY;
            break;
    }
    handle_protocol_event(efd, trans);
}
/*
 * parse_uri - parse URI into filename
 */
void parse_uri(char *uri, char *filename) {
    strcpy(filename, ".");
    strcat(filename, uri);
    /*
    if (uri[strlen(uri) - 1] == '/')
        strcat(filename, "home.html");
        */
}

void send_resp_header(int efd, transaction_t* trans) {
    printf("send_resp_header\n");
    char filetype[MAXLINE];
    int header_len;

    /* Send response headers to client */
    get_filetype(trans->filename, filetype);
    header_len = snprintf(trans->write_buf, sizeof(trans->write_buf), "HTTP/1.0 200 OK\r\n");
    header_len += snprintf(trans->write_buf + header_len, sizeof(trans->write_buf) - header_len, "Server: Naive HTTP Server\r\n");
    header_len += snprintf(trans->write_buf + header_len, sizeof(trans->write_buf) - header_len, "Connection: close\r\n");
    header_len += snprintf(trans->write_buf + header_len, sizeof(trans->write_buf) - header_len, "Content-length: %d\r\n", trans->filesize);
    header_len += snprintf(trans->write_buf + header_len, sizeof(trans->write_buf) - header_len, "Content-type: %s\r\n\r\n", filetype);

    trans->write_len = strlen(trans->write_buf);
    trans->next_stage = P_SEND_RESP_BODY;
    handle_transmission_event(efd, trans);
}

void write_n(int efd, transaction_t* trans) {
    printf("write all %d\n", trans->write_len);
    ssize_t count;
    while (trans->write_pos < trans->write_len) {
        count = write(trans->fd, trans->write_buf, trans->write_len-trans->write_pos);
        if (count < 0) {
            if (count == EAGAIN) return; /* no more can be written */
            else {
                unix_error("write");
                finish_transaction(efd, trans);
                return;
            }
        } else if (count == 0) { /* client closed socket */
            printf("client closed.\n");
            finish_transaction(efd, trans);
        } else {
            printf("%d bytes written.\n", count);
            trans->write_pos += count;
        }
    }
    /* write task done! */
    printf("write task done\n");
    handle_protocol_event(efd, trans);
}

void write_file(int efd, transaction_t* trans) {
    printf("write file to socket\n");
    int rc;
    while (trans->write_pos < trans->filesize) {
        rc = sendfile(trans->fd, trans->read_fd, &trans->write_pos, trans->filesize - trans->write_pos);
        if (rc < 0) {
            if (errno != EAGAIN) {
                unix_error("sendfile");
                finish_transaction(efd, trans);
            }
            return;
        }
    }
    /* write done */
    printf("whole file wrote to socket\n");
    handle_protocol_event(efd, trans);
}

void read_n(int efd, transaction_t* trans) {
    printf("read_n %d\n", trans->read_len);
    ssize_t count = 0;
    while (trans->read_pos < trans->read_len) {
        count = read(trans->fd, trans->read_buf, MIN(trans->read_len, MAXBUF-trans->read_pos));
        if (count < 0) {
            if (errno != EAGAIN) {
                unix_error("read");
                finish_transaction(efd, trans);
            }
            return; /* EAGAIN: no more */
        } else if (count == 0) {
            /* client closed socket. */
            printf("client closed.\n");
            finish_transaction(efd, trans);
        } else {
            printf("%d bytes read.\n", count);
            trans->read_pos += count;
        }
    }
    /* no more or buffer full */
    handle_protocol_event(efd, trans);
}

/*
 * serve_download - copy a file back to the client
 */
void serve_download(int efd, transaction_t*trans) {
    printf("serve download\n");
    int fd;
    fd = open(trans->filename, O_RDONLY, 0);
    if (fd < 0) {
        unix_error("open file");
        client_error(efd, trans, trans->filename, "500", "Internal Server Error", "Cannot open file");
        return;
    }
    trans->write_pos = 0;
    trans->read_fd = fd;
    trans->state = S_WRITE_FILE;
    trans->next_stage = P_DONE;
    handle_transmission_event(efd, trans);
}

void serve_upload(int efd, transaction_t* trans) {
    printf("serve upload %s\n", trans->filename);
    if (trans->write_fd == INVALID_FD) {
        /*
        * Create new file.
        * Use exclusive file lock to deal with consistency.
        * Use chroot to restrict access.
        * Permission: only owner can read/write.
        *
        */
        // TODO defer clean file
        trans->write_fd = open(trans->filename, O_WRONLY | O_CREAT /*| O_EXLOCK*/, S_IWUSR | S_IRUSR); /* TODO: Use chroot for security */
        // FIXME O_EXLOCK not available on Linux
        if (trans->write_fd <= 0) {
            unix_error("Could not open file.");
            client_error(efd, trans, trans->filename, "503", "Service Unavailable", "Cannot create the requested file.");
            return;
        }

        /* write file */
        trans->dest_file = fdopen(trans->write_fd, "w");
        if (not trans->dest_file) {
            unix_error("failed to open dest_file");
            client_error(efd, trans, trans->filename, "503", "Service Unavailable", "Cannot create the requested file.");
            // FIXME cleanup
            return;
        }
    }
    /* read buffer->file */
    if (fwrite(trans->read_buf, sizeof(char), trans->read_pos, trans->dest_file) < trans->read_pos) {
        unix_error("fwrite");
        client_error(efd, trans, trans->filename, "500", "Server Internal Error", "Cannot write to the requested file.");
        return;
    }
    printf("%d bytes wrote to file.\n", trans->read_pos);
    trans->saved_pos += trans->read_pos;
    trans->read_pos = 0;
    if (trans->saved_pos < trans->filesize) { /* Read more */
        trans->read_len = MIN(MAXBUF, trans->filesize - trans->saved_pos);
    } else { /* whole file uploaded */
        printf("file uploaded!\n");
        trans->next_stage = P_DONE;
        trans->read_len = 0;
    }
    handle_transmission_event(efd, trans);
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

void finish_transaction(int efd, transaction_t* trans) {
    printf("finish transaction\n");
    epoll_event_t event;
    event.data.fd = trans->fd;
    epoll_ctl(efd, EPOLL_CTL_DEL, trans->fd, NULL);

    /* It's fine to close fd more than once. Just ignore the error. */
    if (close(trans->fd) < 0) {
        unix_error("close socket");
    }
    if (trans->read_fd > 0 && close(trans->read_fd) < 0) {
        unix_error("close read fd");
    }
    if (trans->dest_file != NULL) {
        if (fclose(trans->dest_file) != 0) {
            unix_error("fclose failed");
        }
        if (trans->saved_pos != trans->filesize && remove(trans->filename) == ERROR) { /* Remove created file */
            unix_error("remove failed"); /* Just ignore. */
        }
    }
    if (trans->write_fd > 0 && close(trans->write_fd) < 0) {
        unix_error("close write fd");
    }
    remove_transaction_from_slots(trans);
}

void handle_protocol_event(int efd, transaction_t* trans) {
    switch(trans->next_stage) {
        case P_READ_REQ_BODY:
            serve_upload(efd, trans);
            break;
        case P_SEND_RESP_BODY:
            serve_download(efd, trans);
            break;
        case P_SEND_RESP_HEADER:
            send_resp_header(efd, trans);
            break;
        case P_DONE:
            finish_transaction(efd, trans);
            break;
    }
}

void handle_transmission_event(int efd, transaction_t* trans) {
    switch (trans->state) {
        case S_READ_REQ_HEADER:
            read_request_header(trans, efd);
            break;
        case S_READ:
            read_n(efd, trans);
            break;
        case S_WRITE:
            write_n(efd, trans);
            break;
        case S_WRITE_FILE:
            write_file(efd, trans);
            break;
    }
}

/*
 * init_headers Initialize a http_headers_t struct
 */
void init_headers(http_headers_t *hdrs) {
    hdrs->len = 0;
    hdrs->head = NULL;
    hdrs->tail = NULL;
}

/*
 * append_header Append an entity
 */
void append_header(http_headers_t *hdrs, http_header_item_t *item) {
    item->next = NULL;
    if (hdrs->len == 0) {
        hdrs->len = 1;
        hdrs->head = item;
        hdrs->tail = item;
    } else {
        hdrs->len += 1;
        hdrs->tail->next = item;
        hdrs->tail = item;
    }
}

/*
 * destroy_headers Destroy a http_headers_t struct
 */
void destroy_headers(http_headers_t *hdrs) {
    destroy_header_item(hdrs->head);
}

/*
 * destroy_header_item Destroy a http_header_item_t struct
 */
void destroy_header_item(http_header_item_t *item) {
    if (item == NULL) return;
    if (item->next != NULL) destroy_header_item(item->next);
    free(item);
}

void client_error(int efd, transaction_t* trans, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    int n;
    int body_len;
    char body[MAXBUF];
    epoll_event_t event;
    printf("client error %s %s %s\n", errnum, shortmsg, longmsg);
    /* Build the HTTP response body */
    body_len = snprintf(body+body_len, sizeof(body) - body_len, "<html><title>Tiny Error</title>");
    body_len += snprintf(body+body_len, sizeof(body) - body_len, "<body bgcolor=""ffffff"">\r\n");
    body_len += snprintf(body+body_len, sizeof(body) - body_len, "%s: %s\r\n", errnum, shortmsg);
    body_len += snprintf(body+body_len, sizeof(body) - body_len, "<p>%s: %s\r\n", longmsg, cause);
    body_len += snprintf(body+body_len, sizeof(body) - body_len, "<hr><em>The Tiny Web server</em>\r\n");
    /* Print the HTTP response */
    n = snprintf(trans->write_buf, sizeof(trans->write_buf), "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    n += snprintf(trans->write_buf + n, sizeof(trans->write_buf) - n, "Content-Type: text/html\r\n");
    n += snprintf(trans->write_buf + n, sizeof(trans->write_buf) - n, "Content-Length: %d\r\n\r\n", body_len);
    n += snprintf(trans->write_buf + n, sizeof(trans->write_buf) - n, "%s", body);

    event.data.fd = trans->fd;
    event.events = EPOLLOUT | EPOLLET;
    if (epoll_ctl(efd, EPOLL_CTL_MOD, trans->fd, &event) < 0) {
        unix_error("epoll ctl");
    }

    trans->write_len = n;
    trans->state = S_WRITE;
    trans->next_stage = P_DONE;
    trans->write_pos = 0;
    handle_transmission_event(efd, trans);
}

