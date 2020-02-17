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

    int true = 1;
    CALL (setsockopt(sock->listenfd, SOL_SOCKET, SO_REUSEADDR, &true, sizeof (int)), "setsockopt");
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
int groups[100][1000];
pthread_mutex_t lock[100], gsize_lock;
int gsize = 0;

void list_init () {
    for (int i = 0; i < 100; ++i) {
        pthread_mutex_init (&lock[i], NULL);
        for (int j = 0; j < 1000; ++j) {
            groups[i][j] = -1;
        }
    }
    pthread_mutex_init (&gsize_lock, NULL);
}

void list_insert (int grp, int val) {
    pthread_mutex_lock (&lock[grp]);
    for (int j = 0; j < 1000; ++j) {
        if (groups[grp][j] == -1) {
            groups[grp][j] = val;
            break;
        }
    }
    pthread_mutex_unlock (&lock[grp]);
}

void list_remove (int grp, int val) {
    pthread_mutex_lock (&lock[grp]);
    for (int j = 0; j < 1000; ++j) {
        if (groups[grp][j] == val) {
            groups[grp][j] = -1;
            break;
        }
    }
    pthread_mutex_unlock (&lock[grp]);
}


pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

void send_group_message (int grp, struct Msg msg) {
    // struct Msg * cpy = malloc (sizeof (struct Msg));
    // *cpy = msg;
    // printf ("in send_group_message\n");
    // queue_push (request_q, cpy);
    // printf ("out send_group_message\n");
    pthread_mutex_lock (&lock[grp]);
    int serv_op = MSG;
    for (int j = 0; j < 1000; ++j) {
        if (groups[grp][j] != -1) {
            int val = groups[grp][j];
            write (val, &serv_op, sizeof (int));
            write (val, &msg, sizeof (msg));
        }
    }
    pthread_mutex_unlock (&lock[grp]);
}

void send_group_notif (int grp, char * name) {
    // struct Msg * cpy = malloc (sizeof (struct Msg));
    // *cpy = msg;
    // printf ("in send_group_message\n");
    // queue_push (request_q, cpy);
    // printf ("out send_group_message\n");
    // pthread_mutex_lock (&lock[grp]);
    // struct node * curr = groups[grp].head;
    // int serv_op = NOTIF;
    // struct Notification notif;
    // char msg[256];
    // sprintf (msg, "%s has left", name);
    // strcpy (notif.msg, msg);
    // while (curr) {
    //     write (curr->val, &serv_op, sizeof (int));
    //     write (curr->val, &notif, sizeof (notif));
    //     curr = curr->nxt;
    // }
    // pthread_mutex_unlock (&lock[grp]);
}

// cons = notempty
void * broadcast_handler (void * arg) {
    while (1) {
        pthread_mutex_lock (&qlock);
        while (queue_empty (request_q)) {
            pthread_cond_wait (&not_empty, &qlock);
        }
        struct Msg * msg = queue_pop (request_q);
        int grp = msg->grp;
        LOGI(grp);
        pthread_mutex_lock (&lock[grp]);
        int serv_op = MSG;
        int tot = 0;
        for (int j = 0; j < 1000; ++j) {
            if (groups[grp][j] != -1) {
                ++tot;
                int val = groups[grp][j];
                write (val, &serv_op, sizeof (int));
                write (val, msg, sizeof (struct Msg));
            }
        }

        LOG ("Message sent to %d clients\n", tot);
        // while (curr) {
        //     write (curr->val, &serv_op, sizeof (int));
        //     write (curr->val, msg, sizeof (struct Msg));
        //     curr = curr->nxt;
        // }
        pthread_mutex_unlock (&lock[grp]);
        pthread_mutex_unlock (&qlock);
        pthread_cond_signal (&not_full);
    }
}

void * connection_handler (void * arg) {
    int connfd = *((int*)arg);
    int flag = 0, grp;
    int req;
    while (READ_REQ (connfd, req)) {
        // int req; READ_REQ (connfd, req);
        switch (req) {
            case JOIN_ROOM : {
                read (connfd, &grp, sizeof (grp));
                list_insert (grp, connfd);
                flag = 1;
                break;
            }
            case LEAVE_ROOM : {
                close (connfd);
                return NULL;
            }
        }
        if (flag) break;
    }
    write (connfd, &grp, sizeof (grp));
    printf ("New usr in grp[%d]\n", grp);
    while (read (connfd, &req, sizeof (req))) {
        // read (connfd, &req, sizeof (req));
        // LOG ("GOT req : %d", req);
        if (req == LEAVE_ROOM) {
            printf ("%s left\n", "[unknwn]");
            // send_group_notif (grp, msg.who);
            goto leave;
        }
        struct Msg * msg = malloc (sizeof (struct Msg));
        read (connfd, msg, sizeof (struct Msg));
        // queue_push (request_q, &msg);
        // printf ("\n----\n");
        // printf ("msg:[%d] %s\n", msg->grp, msg->msg);
        // printf ("\n----\n");
        pthread_mutex_lock (&qlock);
        while (queue_full (request_q)) {
            pthread_cond_wait (&not_full, &qlock);
        }
        queue_push (request_q, msg);
        // LOG ("Written message to q");
        pthread_mutex_unlock (&qlock);
        pthread_cond_signal(&not_empty);

        // send_group_message (grp, msg);
    }
    leave:
    list_remove (grp, connfd);
    LOGI(connfd);
    close (connfd);
}

void Server_run (struct Server * server) {
    LOG ("Server running at %d", server->server_sock.serv_addr.sin_port);
    pthread_t btid;
    pthread_create (&btid, NULL, broadcast_handler, NULL);
    while (1) {
        int *connfd = malloc (sizeof (int));
        CALL (*connfd = accept (server->server_sock.listenfd, (struct sockaddr*)NULL ,NULL), "accept");
        // LOG ("New connection accepted.")
        pthread_t tid;
        pthread_create (&tid, NULL, connection_handler, connfd);
        usleep (20*1000);
    }
    LOG ("Server quit");
}

void signal_handler (int signum) {
    int serv_op = KILL;
    printf ("IN SigHandler\n");
    printf ("\nQuit [Y/n] ");
    char c = getchar ();
    if (c == 'Y') {
        LOGI(serv_sock);
        for (int i = 0; i < 100; ++i) {
            for (int j = 0; j < 1000; ++j) {
                if (groups[i][j] != -1) {
                    write (groups[i][j], &serv_op, sizeof (int));
                }

            }
        }
        close (serv_sock);
        exit (0);
    }
}

void init () {
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
