/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_LOCAL_H_
#define _X_LOCAL_H_
#include <stdint.h>
#include <stdlib.h>

#include "block.h"





int set(const uint32_t nkey, const char* key, const uint32_t nvalue, const char* value);
// not change the return ptr
char* get(const uint32_t nkey, const char* key, int* nvalue);

int del(const uint32_t nkey, const char* key);
void set_lru(uint32_t min, uint32_t max, uint32_t count, uint32_t sleep_time, uint32_t switcher);

int expand(size_t size);

char* stats_all();

void* get_block_start(const uint32_t nkey, const char* key);
void get_block_end(block* b);


char* get_backup_start(uint64_t* len);
void get_backup_end(int reset);


#endif

