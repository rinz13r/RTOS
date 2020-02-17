#pragma once

#define MAX_QUEUE_SIZE 1000

struct Msg;

struct queue {
    struct Msg * msg[MAX_QUEUE_SIZE];
    size_t l, r, size;
};

struct queue * queue_new () {
    struct queue * q = malloc (sizeof (struct queue));
    q->l = q->r = 0;
    q->size = MAX_QUEUE_SIZE + 1;

    return q;
}

size_t next (size_t pos) {
    return (pos+1)%(MAX_QUEUE_SIZE + 1);
}

void queue_push (struct queue * q, struct Msg * msg) {
    q->msg[q->r] = msg;
    q->r = next (q->r);
}

struct Msg * queue_pop (struct queue * q) {
    struct Msg * ret = q->msg[q->l];
    q->l = next (q->l);
    return ret;
}

int queue_empty (struct queue * q) {
    return (q->l == q->r);
}
int queue_full (struct queue * q) {
    return next (q->r) == q->l;
}
