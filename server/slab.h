/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_SLAB_H_
#define _X_SLAB_H_

#include <stdint.h>
#include <pthread.h>
#include "block.h"
#include "page.h"


typedef struct __slab_stats{
    pthread_mutex_t lock_stats;
    uint64_t lru_cold;
    uint64_t lru_warm;
    uint64_t lru_hot;
    uint64_t count_recycled;
} _slab_stats;

typedef struct _slab{
    struct _slab* next;
    uint32_t fixed_block_size;
    uint32_t fixed_block_count;
    uint32_t block_count_free;
    uint32_t page_count;
    block* block_list_free;
    page* page_list;
    pthread_mutex_t lock;
    _slab_stats stats;
    uint64_t real_mem_used;
    uint8_t id;

} slab;


int slab_init( );
void slab_switch(int on);

block* slab_alloc(const int size);
void slab_free_nolock(block* b, slab* s);
int slab_free(block* b);
slab* slab_get(uint8_t id);
void slab_recycle(slab* s);
char* slab_stats(uint64_t* r_total);
void slab_lock();
void slab_unlock();


#endif

