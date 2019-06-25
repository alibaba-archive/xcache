/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_LRU_H_
#define _X_LRU_H_


#include "slab.h"
#include "block.h"



void lru_init();
int lru_recycle(slab* s, uint32_t try_times, int update);
void lru_update(slab* s, uint32_t count);
void lru_run();
void lru_switch(int on);

void lru_del(block* b);
void lru_add(block* b);

void lru_replace_onlock(block* old_b, block* new_b);

int lru_try_lock_mutex(uint64_t lru_id);

void lru_lock_mutex(uint64_t lru_id);
void lru_unlock_mutex(uint64_t lru_id);

#endif

