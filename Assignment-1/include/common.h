#pragma once

enum Request {
    NUM_ROOMS,
    CREATE_ROOM,
    JOIN_ROOM,
    LEAVE_ROOM,
    SEND_MSG,
};

enum ServerOps {
    MSG,
    KILL,
    NOTIF,
};

typedef long long ll;

struct t_format {
    ll s;
    ll f;
    time_t now;
};

struct Msg {
    int grp;
    char who[20];
    // int from;
    struct t_format ts;
    char msg[256];
};

struct Notification {
    char msg[256];
};
