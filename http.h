
/*
Copyright 2018 Xavier Yao <xavieryao@me.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef NAIVE_HTTP_HTTP_H
#define NAIVE_HTTP_HTTP_H

#include <sys/epoll.h>
#include "misc.h"

void handle_request(int fd, int listenfd, int efd);
void handle_epoll_error(int fd, int efd);

/*
 * entity of request header
 */
typedef struct _http_header_item_t {
    char key[MAXLINE];
    char value[MAXLINE];
    struct _http_header_item_t *next;
} http_header_item_t;

/*
 * list of entities of request header
 */
typedef struct {
    int len;
    http_header_item_t* head, *tail;
} http_headers_t;

#endif //NAIVE_HTTP_HTTP_H
