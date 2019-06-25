/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "local.h"
#include "config.h"
#include "log.h"
#include "block.h"
#include "slab.h"
#include "bklist.h"
#include "stats.h"
#include "lru.h"
#include "page.h"
#include "backup.h"
#include "connect.h"



#include <assert.h>

extern config g_config;
extern stats g_stats;





int set(const uint32_t nkey, const char* key, const uint32_t nvalue, const char* value){
    assert(0 < nkey && NULL != key && 0 < nvalue && NULL != value );
    LOG_I("begin set!\n");
    uint32_t hv = 0;
    bklist_lock(nkey,key,&hv);
    if(NULL != bklist_find_no_lock(nkey, key,hv)){
        LOG_B_K(nkey,key,"is already in bklist[%10x] with hv(%10x)\n",BKLIST_ID(hv),hv);
        bklist_unlock(hv);
        STATS_LOCK;
        g_stats.set_failed ++;
        STATS_UNLOCK;
        return -1;
    }
    block* b = slab_alloc(sizeof(block) + nkey + nvalue);
    if(NULL == b){
        LOG_E("end set with error slab_alloc failed!\n");
        bklist_unlock(hv);
        STATS_LOCK;
        g_stats.set_failed ++;
        STATS_UNLOCK;
        return -1;
    }
    
    block_set_key(b,nkey,key);
    block_set_value(b,nvalue,value);
    bklist_add_nolock(b,hv);
    STATS_LOCK;
    g_stats.set_ok ++;
    STATS_UNLOCK;
    bklist_unlock(hv);
    LOG_I("end set!\n");
    return 0;
    
}
char* get(const uint32_t nkey, const char* key, int* nvalue){
    assert(0 < nkey && NULL != key && NULL != nvalue);
    LOG_I("begin get!\n");
    block* b = bklist_find(nkey,key);
    if(NULL != b){
        *nvalue = b->nvalue;
        LOG_I("end get!\n");
        STATS_LOCK;
        g_stats.get_ok ++;
        STATS_UNLOCK;
        return BLOCK_v(b);
    }
    *nvalue = 0;
    STATS_LOCK;
    g_stats.get_failed ++;
    STATS_UNLOCK;
    LOG_I("end get with error!!\n");
    return NULL;
}
int del(const uint32_t nkey, const char* key){
    assert(0 < nkey && NULL != key);
    LOG_I("begin del!\n");
    uint32_t hv = 0;
    bklist_lock(nkey,key,&hv);
    block* b = bklist_find_no_lock(nkey,key,hv);
    if(NULL == b){
        LOG_I("end del with error!\n");
        STATS_LOCK;
        g_stats.del_failed ++;
        STATS_UNLOCK;
        bklist_unlock(hv);
        return -1;
    }
    lru_del(b);
    int ret_delete = bklist_delete_nolock(b,hv);
    assert(0 == ret_delete);
    slab_free(b);
    bklist_unlock(hv);
    STATS_LOCK;
    g_stats.del_ok ++;
    STATS_UNLOCK;
    LOG_I("end del!\n");
    return 0;

}

void set_lru(uint32_t min, uint32_t max, uint32_t count, uint32_t sleep_time, uint32_t switcher){
    set_lru_parameter(min,max,count,sleep_time,switcher);
    lru_switch(switcher);
}

void* get_block_start(const uint32_t nkey, const char* key){
    assert(0 < nkey && NULL != key);
    block* b = bklist_find(nkey,key);
    if(NULL == b){
        STATS_LOCK;
        g_stats.get_failed ++;
        STATS_UNLOCK;
    }else{
        STATS_LOCK;
        g_stats.get_ok ++;
        STATS_UNLOCK;
    }
    return b;

}

void get_block_end(block* b){
    assert(NULL != b);
    uint32_t hv = 0;
    bklist_lock(b->nkey,BLOCK_k(b),&hv);
    b->nref --;
    //trick! when the client call the del API and the block is being sent. 
    //after send we should free it
    if(b->flags & BLOCK_FLAGS_WILL_FREE){
        
        slab_free(b);
    }
    bklist_unlock(hv);
}

char* get_backup_start(uint64_t* len){
    backup_lock();
    return backup_get_nolock(len);
}

void get_backup_end(int reset){
    if(1 == reset){
        backup_reset_nolock();
    }
    backup_unlock();
}
int expand(size_t size){
     if(size <= 1) {
        set_memory_parameter(size,0,0);
        return 0;
     }
     return page_expand(size);
}

char* stats_all(){
 
    return stats_show();
}


