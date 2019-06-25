/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_STATS_H_
#define _X_STATS_H_
#include <stdint.h>
#include <pthread.h>

typedef struct _stats{
    uint64_t get_ok;
    uint64_t get_failed;
    uint64_t set_ok;
    uint64_t set_failed;
    uint64_t del_ok;
    uint64_t del_failed;
    uint64_t fd_cleared;
    uint64_t op_lru;
    int64_t thread_run;
    int64_t max_cons;
    pthread_mutex_t lock;
    pthread_cond_t cond;
}stats;


void stats_init();
char* stats_show();
void stats_lock();
void stats_unlock();
void stats_wait();
void stats_signal();



#endif
