#pragma once

#define MAX_QUEUE_SIZE 10000

struct Msg;

struct queue_node {
    struct Msg * msg;
    struct queue_node * nxt;
};
struct queue_node * queue_node_new (struct Msg * msg, struct queue_node * nxt) {
    struct queue_node * qnode = malloc (sizeof (struct queue_node));
    qnode->msg = msg;
    qnode->nxt = nxt;

    return qnode;
}

struct queue {
    struct queue_node * head, * tail;
    pthread_mutex_t lock;
    pthread_cond_t not_full, not_empty;
    int full, empty;
    size_t size;
} queue;

struct queue * queue_new () {
    struct queue * q = malloc (sizeof (struct queue));
    q->size = 0;
    q->full = 0;
    q->empty = 1;
    pthread_mutex_init (&q->lock, NULL);
    pthread_cond_init (&q->not_full, NULL);
    pthread_cond_init (&q->not_empty, NULL);

    return q;
}

void queue_push (struct queue * q, struct Msg * msg) {
    struct queue_node * qnode = queue_node_new (msg, NULL);
    while (q->full) {
        pthread_cond_wait (&q->not_full, &q->lock);
    }

    pthread_mutex_lock (&q->lock);
    if (q->tail) {
        q->tail->nxt = qnode;
    } else {
        q->tail = qnode;
        q->head = q->tail;
    }
    q->size++;
    q->full = (q->size == MAX_QUEUE_SIZE);
    pthread_mutex_unlock (&q->lock);
    pthread_cond_signal (&q->not_empty);
}

struct Msg * queue_pop (struct queue * q) {
    while (q->empty) {
        pthread_cond_wait (&q->not_empty, &q->lock);
    }
    pthread_mutex_lock (&q->lock);
    struct Msg * ret = q->head->msg;
    struct queue_node * head = q->head;
    q->head = q->head->nxt;
    free (head);
    q->empty = (q->size == 0);
    pthread_mutex_unlock (&q->lock);
    pthread_cond_signal (&q->not_full);

    return ret;

}
