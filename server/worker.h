/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_WORKER_H_
#define _X_WORKER_H_

#include <pthread.h>

#include "event.h"
#include "connect.h"





typedef struct _worker{
    event event;
    event_base* ev_base;
    con* con_head;
    con* con_tail;
    pthread_t thread_id;
    pthread_mutex_t lock;
    int *pipefd;
    con* con_head_used;
    con* con_tail_used;

}worker;

void worker_init();
int worker_add(con* con);
void worker_delete(con* con);
int worker_mod(con* con);

void worker_run_recycle();



#endif
