/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_EVENT_H_
#define _X_EVENT_H_
#include <stdint.h>


typedef void (*handler)(void*, void*, int);


typedef struct _event_base{
    int epfd;
    int timeout;
}event_base;


typedef struct _event{
    int fd;
    uint32_t event_flags;
    handler handler;
}event;

event_base* event_init();
int event_add(event_base* event, void* arg);
int event_mod(event_base* event, void* arg);
int event_delete(event_base* event, void* arg);
int event_base_loop(const event_base* event);
void event_base_free(event_base* event);
int event_clear(int fd_to_clear, void* ep_event, int len);


#endif

