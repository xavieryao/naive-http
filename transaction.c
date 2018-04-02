/*
Copyright 2018 Xavier Yao <xavieryao@me.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "transaction.h"

static transaction_slots_t slots;

void init_transaction(transaction_t* trans) {
    trans->fd = INVALID_FD;
    trans->read_fd = INVALID_FD;
    trans->write_fd = INVALID_FD;
    trans->dest_file = NULL;
    trans->filesize = 0;
    trans->state = S_INVALID;
    trans->next_stage = P_INVALID;
    trans->read_pos = 0;
    trans->write_pos = 0;
    trans->parse_pos = 0;
    trans->saved_pos = 0;
    init_headers(&trans->headers);
}

void init_transaction_slots() {
    int i;
    for (i = 0; i < MAXHASH; i++) {
        slots.transactions[i] = NULL;
    }
}

void remove_transaction_from_slots(transaction_t* trans) {
    if (trans->fd < 0) return;
    transaction_node_t* node = NULL, *prev = NULL;
    node = slots.transactions[trans->fd % MAXHASH];
    while (node && node->fd != trans->fd) {
        prev = node;
        node = node->next;
    }
    if (node) {
        if (prev) prev->next = node->next;
        free(node);
        slots.n -= 1;
    }
}

transaction_t* find_empty_transaction_for_fd(int fd) {
    if (slots.n > MAXTRANSACTION) return NULL;
    transaction_node_t* node = slots.transactions[fd % MAXHASH];
    transaction_node_t* prev = node;
    while (node != NULL) {
        prev = node;
        node = node->next;
    } 
    transaction_node_t* new_node = malloc(sizeof(transaction_node_t));
    if (prev) prev->next = new_node;
    else slots.transactions[fd % MAXHASH] = new_node;
    new_node->next = NULL;
    return &new_node->transaction; 
}

transaction_t* find_transaction_for_fd(int fd) {
    if (trans->fd < 0) return;
    transaction_node_t* node = NULL;
    node = slots.transactions[trans->fd % MAXHASH];
    while (node && node->fd != trans->fd) {
        node = node->next;
    }
    return node;
}
