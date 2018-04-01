
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
#include <copyfile.h>
#include "http.h"
#include "error_handler.h"
#include "rio.h"


int read_requesthdrs(rio_t *rp, http_headers_t *hdrs);

void parse_uri(char *uri, char *filename);

void serve_download(int fd, char *filename, int filesize);

void serve_upload(int fd, rio_t* rio, char *filename, http_headers_t* headers);

void get_filetype(char *filename, char *filetype);

void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);

void init_headers(http_headers_t *headers);

void destroy_headers(http_headers_t *hdrs);

void destroy_header_item(http_header_item_t *item);

void append_header(http_headers_t *hdrs, http_header_item_t *item);


/*
 * Iteratively handle HTTP/1.0 transactions
 */
void handle_conn(int fd) {
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE];
    enum {
        GET, POST, HEAD
    } methodtype;
    rio_t rio;

    /* Read request line */
    rio_readinitb(&rio, fd);
    if (!rio_readlineb(&rio, buf, MAXLINE)) // FIXME: deal with overflow
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET") == 0) methodtype = GET;
    else if (strcasecmp(method, "POST") == 0) methodtype = POST;
    else if (strcasecmp(method, "HEAD") == 0) methodtype = HEAD;
    else {
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }

    /* Parse URI from request */
    parse_uri(uri, filename);

    /* Parse request headers */
    http_headers_t headers;
    init_headers(&headers);
    if (read_requesthdrs(&rio, &headers) < 0) {
        destroy_headers(&headers);
        clienterror(fd, filename, "400", "Bad Request", "Could not parse request header.");
        return;
    }

    if (methodtype == POST) { /* Upload file */
        serve_upload(fd, &rio, filename, &headers);
    } else {
        if (stat(filename, &sbuf) < 0) {
            destroy_headers(&headers);
            clienterror(fd, filename, "404", "Not found",
                        "Tiny couldn't find this file");
            return;
        }
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            destroy_headers(&headers);
            clienterror(fd, filename, "403", "Forbidden",
                        "Tiny couldn't read the file");
            return;
        }

        /*
         * Download file. First check whether the file exists.
         */
        if (methodtype == GET) {
            serve_download(fd, filename, sbuf.st_size);
        } else if (methodtype == HEAD) {
            // FIXME not implemented
        }
    }
    /* Release resources */
    destroy_headers(&headers);
}

/*
 * read_requesthdrs - read HTTP request headers
 * return: 0 ok  -1 error
 */
int read_requesthdrs(rio_t *rp, http_headers_t *hdrs) {
    printf("Read request hdrs\n");

    char buf[MAXLINE];
    char *key_s, *value_s, *to_free;
    http_header_item_t *item = NULL;

    while (true) {
        if (rio_readlineb(rp, buf, MAXLINE) <= 0) return ERROR;
        if (strcmp(buf, "\r\n") == 0) break;

        to_free = value_s = strdup(buf);
        key_s = strsep(&value_s, ": ");

        size_t value_len = strlen(value_s);
        if (value_len < 3 || !(value_s[0] == ' ' && value_s[value_len - 1] == '\n' && value_s[value_len - 2] == '\r')) {
            free(to_free);
            return ERROR;
        }
        value_s += 1; // value_s points to the sp of ': ', move to the beginning.

        value_s[value_len - 3] = '\0'; // trim '\r\n'

        item = (http_header_item_t *) malloc(sizeof(http_header_item_t));
        strncpy(item->key, key_s, MAXLINE);
        strncpy(item->value, value_s, MAXLINE);

        printf("KEY[%s] VALUE[%s]\n", item->key, item->value);

        append_header(hdrs, item);
        free(to_free);
    }
    printf("okay\n");
    return OKAY;
}

/*
 * parse_uri - parse URI into filename
 */
void parse_uri(char *uri, char *filename) {
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/')
        strcat(filename, "home.html");
}

/*
 * serve_download - copy a file back to the client
 */
void serve_download(int fd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    /* Send response body to client */
    srcfd = open(filename, O_RDONLY, 0);
    srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close(srcfd);
    rio_writen(fd, srcp, filesize);
    munmap(srcp, filesize);
}

void serve_upload(int fd, rio_t* rio, char *filename, http_headers_t* headers) {
    printf("serve upload %s\n", filename);
    long content_len = -1;
    int dest_fd;
    bool ok = false;

    http_header_item_t* hdr_item = headers->head;
    while (hdr_item != NULL) {
        if (strcmp(hdr_item->key, "Content-Length") == 0) {
            content_len = strtol(hdr_item->value, NULL, 10);
            if (errno) {
                unix_error("strtol failed");
                content_len = -1;
            }
            break;
        }
        hdr_item = hdr_item->next;
    }
    if (content_len <= 0) {
        clienterror(fd, filename, "400", "Bad Request", "Content-Length must be provided and be positive.");
        return;
    }
    if (content_len > MAX_FILE_SIZE) {
        clienterror(fd, filename, "400", "Bad Request", "File larger than limit.");
        return;
    }

    /*
     * Use exclusive file lock to deal with consistency.
     * Use chroot to restrict access.
     * Permission: only owner can read/write.
     *
     * DEFER: close/clean file
     */
    dest_fd = open(filename, O_WRONLY | O_CREAT | O_EXLOCK, S_IWUSR | S_IRUSR); /* TODO: Use chroot for security */
    if (dest_fd <= 0) {
        unix_error("Could not open file.");
        clienterror(fd, filename, "503", "Service Unavailable", "Cannot create the requested file.");
        return;
    }
#if defined(__APPLE__)
    char buffer[MAXBUF];
    long transferred = 0;
    long to_read, read_n;
    FILE* dest_file = fdopen(dest_fd, "w");
    if (not dest_file) {
        unix_error("failed to open dest_file");
        clienterror(fd, filename, "503", "Service Unavailable", "Cannot create the requested file.");
        // FIXME cleanup
        return;
    }
    while (transferred < content_len) {
        to_read = MIN(MAXBUF, content_len - transferred);
        read_n = rio_readnb(rio, buffer, to_read);
        if (read_n < 0) {
            unix_error("read from net socket failed.");
            break;
        }
        if (read_n < to_read) {
            printf("%d %d\n", read_n, to_read);
            app_error("not long enough!");
            break;
        }
        if (fwrite(buffer, sizeof(char), to_read, dest_file) != to_read) {
            unix_error("failed to fwrite");
            break;
        }
        transferred += to_read;
    }
    ok = transferred == content_len;

    if (fclose(dest_file) != 0) {
        unix_error("fclose failed");
    }
#elif defined(__linux__)
    // FIXME Content not long enough
    if (sendfile(dest_fd, in_fd, NULL, content_len) == ERROR) {
        unix_error("sendfile failed.");
    } else {
        ok = true;
    }
#else
#error "Only Darwin or Linux is supported!"
#endif
    /* Clean up */
    if (ok) printf("Upload done!\n");
    else { // clean up
        if (close(dest_fd) == ERROR) {
            unix_error("close failed"); /* Just ignore. */
        }
        if (remove(filename) == ERROR) { /* Remove created file */
            unix_error("remove failed"); /* Just ignore. */
        }
    }
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

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg) {
    printf("client error %s %s %s\n", errnum, shortmsg, longmsg);
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int) strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
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