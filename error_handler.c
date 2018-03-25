
/*
Copyright 2018 Xavier Yao <xavieryao@me.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
 * Error handlers and wrappers. Inspired by csapp3e.
 * Report specific error. In csapp error handlers terminate the app when error occurs.
 * But our program needs to be robust. So just print the error message and manually recover.
 */

#include "error_handler.h"

void unix_error(char *msg) { /* Unix-style error */
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}

void posix_error(int code, char *msg) { /* Posix-style error */
    fprintf(stderr, "%s: %s\n", msg, strerror(code));
}

void gai_error(int code, char *msg) { /* Getaddrinfo-style error */
    fprintf(stderr, "%s: %s\n", msg, gai_strerror(code));
}

void app_error(char *msg) { /* Application error */
    fprintf(stderr, "%s\n", msg);
}
