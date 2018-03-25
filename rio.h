
/*
Copyright 2018 Xavier Yao <xavieryao@me.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
 * Robust I/O. Slightly modified version of CS:APP3e example code. Used for socket read/write.
 */

#ifndef NAIVE_HTTP_RIO_H
#define NAIVE_HTTP_RIO_H

#include <unistd.h>
/*
 * Unbuffered read/write
 * Only return a short count if it encounters EOF.
 */
ssize_t rio_readn(int fd, void *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);

/*
 * Buffered input functions
 * Provide line buffer and binary buffer. Can be used interchangeably.
 * Can not be mixed with unbuffered version.
 */

#define RIO_BUFSIZE 8192
typedef struct {
    int rio_fd; /* Descriptor for this internal buf */
    ssize_t rio_cnt; /* Unread bytes in internal buf */
    char *rio_bufptr; /* Next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE]; /* Internal buffer */
} rio_t; /* RIO internal buffer */

void rio_readinitb(rio_t *rp, int fd); /* Initialize rio internal buffer */

ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen); /* Buffered read, line */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n); /* Buffered read, binary */

#endif //NAIVE_HTTP_RIO_H
