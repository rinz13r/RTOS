#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>

#include "common.h"
#include "ntp.h"

typedef long long ll;

#define LOG(...) printf (__VA_ARGS__); printf ("\n");
#define LOGI(x) printf ("%s=%d\n", #x, x);
#define CLOGI(x) printf ("%d\n", x);
#define CALL(n, msg) if ((n) < 0) {printf ("%s (%s:%d)\n", msg, __FILE__, __LINE__); exit (EXIT_FAILURE);}

struct Socket {
    struct sockaddr_in serv_addr;
    int listenfd;
};

int ClientSocket_init (struct Socket * sock, int port, char const* serv_ip) {
    CALL (sock->listenfd = socket (AF_INET, SOCK_STREAM, 0), "socket creation");
    memset(&sock->serv_addr, '0', sizeof(typeof (sock->serv_addr)));
    sock->serv_addr.sin_family = AF_INET;
    sock->serv_addr.sin_addr.s_addr = inet_addr (serv_ip);
    sock->serv_addr.sin_port = htons(port);
}

struct Client {
    struct Socket sock;
};

void Client_init (struct Client * client, int port, char const* serv_ip) {
    ClientSocket_init (&client->sock, port, serv_ip);
}

int grp;


struct t_format gettime ();

int usr_num, usr_grp;
char * who;

double time_diff (struct t_format ts1, struct t_format ts2) { // in ms
    static ll one = 1;
    double t1 = ts1.s*1000 + ((double)(ts1.f))/(1000);
    double t2 = ts2.s*1000 + ((double)(ts2.f))/(1000);
    return t2-t1;
}

int client_sock;
void * read_handler (void * arg) {
    // LOG("created new thread %s", __func__);
    struct Msg msg;
    struct Notification notif;
    int serv_op;
    while (1) {
        read (*(int*)arg, &serv_op, sizeof (serv_op));
        if (serv_op == MSG) {
            read (*(int*)arg, &msg, sizeof (msg));
            printf ("\n%s: %s [(%lf)ms ago]\n", msg.who, msg.msg, time_diff (msg.ts, gettime()));
        } else if (serv_op == KILL) {
            close (client_sock);
            exit (0);
            break;
        } else if (serv_op == NOTIF) {
            read (*(int*)arg, &notif, sizeof (notif));
            printf ("Notification[%s]\n", notif.msg);
        }
    }
}


void * write_handler (void * arg) {
    // LOG("created new thread %s", __func__);
    LOGI(usr_grp);
    while (1) {
        struct Msg msg;
        strcpy (msg.who, who);
        msg.grp = usr_grp;
        fgets (msg.msg, 255, stdin);
        int req = SEND_MSG;
        write (*(int*)arg, &req, sizeof (req));
        struct t_format t = gettime ();
        msg.ts = t;
        write (*(int*)arg, &msg, sizeof (msg));
    }
}


#define WRITE_REQ(sock, req) write (sock, &req, sizeof (req));

// char * serv_ip;
int sock;
void signal_handler (int signum) {
    int req = LEAVE_ROOM;
    write (sock, &req, sizeof (req));
    printf ("Exiting...\n");
    exit (0);
}

void func (int port, char * serv_ip, int room_id) {
    struct Client client;
    Client_init (&client, port, serv_ip);
    CALL (connect (client.sock.listenfd, (struct sockaddr *)&client.sock.serv_addr, sizeof(client.sock.serv_addr)),
        "connect");
    LOG ("Connected to server");
    int op; sock = client.sock.listenfd;
    int req = JOIN_ROOM;
    WRITE_REQ (sock, req);
    write (sock, &room_id, sizeof (room_id));
    int srm;
    usr_grp = room_id;
    read (sock, &srm, sizeof (srm));
    if (srm == room_id) {LOG ("Joined room %d", room_id);}
    else {
        LOG ("Error connecting");
        exit (0);
    }
    int writing = 1;
    client_sock = client.sock.listenfd;
    pthread_t tidr, tidw;
    pthread_create (&tidr, NULL, read_handler, &client.sock.listenfd);
    if (writing) pthread_create (&tidw, NULL, write_handler, &client.sock.listenfd);

    pthread_join (tidr, NULL);
    if (writing) pthread_join (tidw, NULL);

    close (client.sock.listenfd);
}

int main (int argc, char ** argv) {
    if (argc != 5) {
        printf ("Usage: %s <ip> <port> <name> <gnum>\n", argv[0]);
        exit (0);
    }
    printf ("Client Started!\n");
    ntp_init ();
    signal (SIGINT, signal_handler);
    char * serv_ip = argv[1];
    int port = atoi (argv[2]);

    who = argv[3];
    func (port, serv_ip, atoi (argv[4]));
    return 0;
}
