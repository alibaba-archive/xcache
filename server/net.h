/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_NET_H_
#define _X_NET_H_

#include "event.h"

enum net_type{
    local,
    tcp,
    udp
};


typedef struct _net{
    event event;
    struct _net* next;
    struct _net* prev;
}net;

void net_init(char* host, int port, enum net_type type);
void net_add(int sfd);
void net_delete(net* n);
void net_switch(int on);




#endif