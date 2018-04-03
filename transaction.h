/*
Copyright 2018 Xavier Yao <xavieryao@me.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef NAIVE_HTTP_TRANS_H
#define NAIVE_HTTP_TRANS_H
#include <stdio.h>
#include <stdbool.h>
#include "http.h"
#include "misc.h"

/* which state of transmission */
typedef enum {S_INVALID, S_READ_REQ_HEADER, S_READ, S_WRITE, S_WRITE_FILE} trans_state_e;
/* which stage of the protocol */
typedef enum {P_INVALID, P_SEND_RESP_HEADER, P_SEND_RESP_BODY, P_READ_REQ_BODY, P_DONE} stage_e;

typedef struct {
    /* common field */
    int fd;
    trans_state_e state;
    stage_e next_stage;
    int response_code;
    /* read from socket */
    char read_buf[MAXBUF];
    long read_len;
    long read_pos;
    long parse_pos;
    int write_fd;
    int saved_pos;
    FILE* dest_file;
    /* write to socket */
    char write_buf[MAXBUF];
    long write_len;
    long write_pos;
    int read_fd;
    /* request header */
    long filesize;
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE];
    enum {
        GET, POST, HEAD
    } methodtype;
    http_headers_t headers;
    /* */
} transaction_t;

typedef struct _transaction_node {
    transaction_t transaction;
    struct _transaction_node* next;
} transaction_node_t; /* Linked-list node */

typedef struct {
    int n;
    transaction_node_t* transactions[MAXHASH];
} transaction_slots_t; 

/* transaction context management */
void init_transaction(transaction_t* trans);
void init_transaction_slots();
void increment_transaction_count();
transaction_t* find_empty_transaction_for_fd(int fd);
transaction_t* find_transaction_for_fd(int fd);
void remove_transaction_from_slots(transaction_t* trans);
void init_headers(http_headers_t *headers);

#endif //NAIVE_HTTP_TRANS_H