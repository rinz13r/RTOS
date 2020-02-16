#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "common.h"
#include "queue.h"

#define LOG(...) printf (__VA_ARGS__); printf ("\n");
#define LOGI(x) printf ("%s=%d\n", #x, x);
#define CALL(n, msg) if ((n) < 0) {printf ("%s (%s:%d)\n", msg, __FILE__, __LINE__); exit (EXIT_FAILURE);}

struct Socket {
    struct sockaddr_in serv_addr;
    int listenfd;
};

int serv_sock;

int ServerSocket_init (struct Socket * sock, int port) {
    CALL (sock->listenfd = socket (AF_INET, SOCK_STREAM, 0), "socket")
    serv_sock = sock->listenfd;
    memset(&sock->serv_addr, '0', sizeof(typeof (sock->serv_addr)));
    sock->serv_addr.sin_family = AF_INET;
    sock->serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
    sock->serv_addr.sin_port = htons(port);

    CALL (bind(sock->listenfd, (struct sockaddr*)&sock->serv_addr,sizeof(sock->serv_addr)), "bind");
    CALL (listen(sock->listenfd, 30), "listen");
}

#define MAX_CONNECTIONS 100

struct queue * request_q;

struct Server {
    struct Socket server_sock;
    int numc;
};

void Server_init (struct Server * server, int port) {
    ServerSocket_init (&server->server_sock, port);
    server->numc = 0;
}

#define READ_REQ(sock, req) read (sock, &req, sizeof (req))

struct node {
    int val;
    struct node * nxt;
};
struct list {
    struct node * head;
} groups[100];
pthread_mutex_t lock[100], gsize_lock;
int gsize = 0;

void list_init () {
    for (int i = 0; i < 100; ++i) {
        pthread_mutex_init (&lock[i], NULL);
    }
    pthread_mutex_init (&gsize_lock, NULL);
}

void list_insert (struct list * list, int val) {
    struct node * nd = malloc (sizeof (struct node));
    nd->val = val;

    pthread_mutex_lock (&lock[list-groups]);
    if (list->head == NULL) {
        list->head = nd;
    } else {
        nd->nxt = list->head;
        list->head = nd;
    }
    pthread_mutex_unlock (&lock[list-groups]);
}

void list_remove (struct list * list, int val) {
    pthread_mutex_lock (&lock[list-groups]);
    struct node * curr = list->head, * prev = NULL;
    // while (curr) {
    //     printf ("%d->", curr->val);
    //     if (curr->val == val) {
    //         break;
    //     }
    //     prev = curr;
    //     curr = curr->nxt;
    // }
    // printf ("\n");
    if (curr != NULL) { // found
        if (prev != NULL) { // not first;
            //...prev->curr->next...
            prev->nxt = curr->nxt;
            free (curr);
        }
        else {
            // curr->nxt->...
            list->head = curr->nxt;
            free (curr);
        }
    }
    pthread_mutex_unlock (&lock[list-groups]);
}

void send_group_message (int grp, struct Msg msg) {
    // struct Msg * cpy = malloc (sizeof (struct Msg));
    // *cpy = msg;
    // printf ("in send_group_message\n");
    // queue_push (request_q, cpy);
    // printf ("out send_group_message\n");
    pthread_mutex_lock (&lock[grp]);
    struct node * curr = groups[grp].head;
    int serv_op = MSG;
    while (curr) {
        write (curr->val, &serv_op, sizeof (int));
        write (curr->val, &msg, sizeof (msg));
        curr = curr->nxt;
    }
    pthread_mutex_unlock (&lock[grp]);
}

void * broadcast_handler (void * arg) {
    while (1) {
        printf ("In broadcast handler\n");
        struct Msg * msg = queue_pop (request_q);
        printf ("Got msg\n");
        int grp = msg->grp;
        pthread_mutex_lock (&lock[grp]);
        struct node * curr = groups[grp].head;
        int serv_op = MSG;
        while (curr) {
            write (curr->val, &serv_op, sizeof (int));
            write (curr->val, msg, sizeof (struct Msg));
            curr = curr->nxt;
        }
        pthread_mutex_unlock (&lock[grp]);
    }
}

void * connection_handler (void * arg) {
    int connfd = *((int*)arg);
    int flag = 0, grp;
    while (!flag) {
        int req; READ_REQ (connfd, req);
        switch (req) {
            case NUM_ROOMS : {
                LOGI (gsize);
                write (connfd, &gsize, sizeof(gsize));
                break;
            }
            case JOIN_ROOM : {
                read (connfd, &grp, sizeof (grp));
                list_insert (&groups[grp], connfd);
                flag = 1;
                break;
            }
            case CREATE_ROOM : {
                pthread_mutex_lock (&gsize_lock);
                ++gsize;
                grp = gsize;
                pthread_mutex_unlock (&gsize_lock);
                grp -= 1;
                write (connfd, &grp, sizeof (grp));
                list_insert (&groups[grp], connfd);
                flag = 1;
                break;
            }
            case LEAVE_ROOM : {
                close (connfd);
                return NULL;
            }
        }
    }
    write (connfd, &grp, sizeof (grp));
    printf ("New usr in grp[%d]\n", grp);
    struct Msg msg;
    int req;
    while (read (connfd, &req, sizeof (req))) {
        // read (connfd, &req, sizeof (req));
        // LOG ("GOT req : %d", req);
        if (req == LEAVE_ROOM) {
            goto leave;
        }
        read (connfd, &msg, sizeof (msg));
        // queue_push (request_q, &msg);
        send_group_message (grp, msg);
    }
    leave:
    list_remove (&groups[grp], connfd);
    close (connfd);
}

void Server_run (struct Server * server) {
    LOG ("Server running at %d", server->server_sock.serv_addr.sin_port);
    pthread_t btid;
    pthread_create (&btid, NULL, broadcast_handler, NULL);
    while (1) {
        int connfd;
        CALL (connfd = accept (server->server_sock.listenfd, (struct sockaddr*)NULL ,NULL), "accept");
        // LOG ("New connection accepted.")
        pthread_t tid;
        pthread_create (&tid, NULL, connection_handler, &connfd);
        usleep (20*1000);
    }
    LOG ("Server quit");
}

void signal_handler (int signum) {
    int serv_op = KILL;
    printf ("IN SigHandler\n");
    for (int i = 0; i < 100; ++i) {
        struct node * curr = groups[i].head;
        while (curr) {
            write (curr->val, &serv_op, sizeof (int));
            close (curr->val);
            curr = curr->nxt;
        }
    }
    close (serv_sock);
    exit (0);
}

void init () {
    printf ("Done init\n");
    request_q = queue_new ();
    list_init ();
}


int main (int argc, char ** argv) {
    if (argc < 2) {
        printf ("Usage: %s <port>\n", argv[0]);
        exit (EXIT_FAILURE);
    }
    init ();
    signal (SIGINT, signal_handler);
    int port = atoi (argv[1]);
    struct Server main_server;
    Server_init (&main_server, port);
    Server_run (&main_server);
    return 0;
}
