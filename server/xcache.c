/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "xcache.h"
#include "remote.h"
#include "server.h"

#include <stdlib.h>
#include <stdio.h>

#include <string.h>



//conf == --SERVER=127.0.0.1 20190 10--SERVER=127.0.0.1:20191 10
//conf == --PROXY=127.0.0.1:66666

int x_init(char* conf){
    return server_init(conf);
}
int x_set(const uint32_t nkey, const char* key, const uint32_t nvalue, const char* value){
    return r_set(NULL,-1,nkey,key,nvalue,value,0,0,0);
}
char* x_get(const uint32_t nkey, const char* key, int* nvalue){
    return r_get(NULL,-1,nkey,key,nvalue,0,0,0);
}
int x_del(const uint32_t nkey, const char* key){
    return r_del(NULL,-1,nkey,key,0,0,0);
}

