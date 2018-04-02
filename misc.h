
/*
Copyright 2018 Xavier Yao <xavieryao@me.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef NAIVE_HTTP_CONST_H
#define NAIVE_HTTP_CONST_H

/* miscellaneous constants */
#define MAXLINE 1024 /* maximum line length */
#define MAXBUF 10240 /* maximum buffer size */
#define MAXEVENT 64 /* maximum epoll event */
#define MAXTRANSACTION 1024 /* maximum transaction */

#define OKAY 0
#define ERROR -1

#define MAX_FILE_SIZE 1073741824 /* Only accept files smaller than 1GB */

#define MIN(X,Y) X<Y ? X : Y
#define MAX(X,Y) X>=Y ? X : Y

/* struct aliases */
typedef struct sockaddr SA;
typedef struct epoll_event epoll_event_t;

#endif //NAIVE_HTTP_CONST_H
