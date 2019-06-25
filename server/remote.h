/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_REMOTE_H_
#define _X_REMOTE_H_
#include <stdint.h>
#include <stddef.h>


int r_touch(char* host, int port);

int r_set(char* host, int port, const uint32_t nkey, const char* key, const uint32_t nvalue, const char* value, int detal, int sleeptime, int block);
int r_update(char* host, int port, const uint32_t nkey, const char* key, const uint32_t nvalue, const char* value, int detal, int sleeptime, int block);

char* r_get(char* host, int port, const uint32_t nkey, const char* key, int* nvalue, int detal, int sleeptime, int block);
int r_del(char* host, int port, const uint32_t nkey, const char* key, int detal, int sleeptime, int block);
int r_expand(char* host, int port, const size_t size, int detal, int sleeptime, int block);
int r_stats(char* host, int port, int full, int detal, int sleeptime, int block);
int r_lru(char* host, int port, int min, int max, int recycle_count, int interval, int switcher,int detal, int sleeptime, int block);

int r_mem(char* host, int port, int detal, int sleeptime, int block);


void r_backup(char* f_host, int f_port, char* t_host, int t_port, uint32_t reset, uint32_t thread_num,int detal, int sleeptime, int block);


#endif

