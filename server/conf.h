/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_CONF_H_
#define _X_CONF_H_
#include <stdint.h>

typedef struct __conf_data{
    char* key;
    char* path;
    uint32_t max_len;

}_conf_data;

int conf_to_file(_conf_data* c, const char* value);
char* conf_from_file(_conf_data* c);
#endif