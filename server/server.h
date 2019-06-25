/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_SERVER_H_
#define _X_SERVER_H_

#include <stdint.h>


//ip = "--SERVER=ip:port weight --SERVER=ip:port weight"
int server_init(char* conf);
void server_add(char* ip, int port, int weight);
void server_get(uint32_t key, char* ip, int* port);
void server_stats();


#endif

