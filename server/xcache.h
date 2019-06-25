/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/
#ifndef _X_CACHE_H_
#define _X_CACHE_H_

#include <stdint.h>
int x_init(char* conf);
int x_set(const uint32_t nkey, const char* key, const uint32_t nvalue, const char* value);
char* x_get(const uint32_t nkey, const char* key, int* nvalue);
int x_del(const uint32_t nkey, const char* key);


#endif


