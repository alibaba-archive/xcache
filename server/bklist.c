/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/


#include "bklist.h"

#include "config.h"
#include "log.h"
#include "hash.h"
#include "clock.h"
#include "lru.h"
#include "backup.h"

#include <stdlib.h>

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <assert.h>
#include <pthread.h>

extern config g_config;
static block** bklist = NULL;
static pthread_mutex_t* bklist_mutex = NULL;


//#define BKLIST_DEBUG

#ifdef  BKLIST_DEBUG
#define DEBUG_B LOG_B
#define DEBUG_B_K LOG_B_K
#define DEBUG_B_W LOG_B_W
#define DEBUG_I LOG_I

static uint32_t bklist_debug(uint32_t hv);
static uint32_t bklist_debug(uint32_t hv){
    block* it = bklist[BKLIST_ID(hv)];
    uint32_t count = 0;
    while(NULL != it){
        LOG_B(it, "in bklist[%10x] \n",BKLIST_ID(hv));
        it = it->l_next;
        if(NULL != it){
            assert(it->nkey > 0);
        }
        count ++;
    }
    LOG_I("bklist[%16x] has  block count(%d)\n",BKLIST_ID(hv),count);
    return count;
}

#else
#define DEBUG_B(b,f_, ...)
#define DEBUG_B_K(nkey,key,f_, ...)
#define DEBUG_B_W(nkey,key,f_, ...)
#define DEBUG_I(f_, ...)



#endif


void bklist_init(){
    bklist = (block**)calloc(BKLIST_MAX_ID,sizeof(block*));
    bklist_mutex = calloc(BKLIST_LOCK_MAX_ID,sizeof(pthread_mutex_t));
    uint32_t i = 0;
    while(i < BKLIST_LOCK_MAX_ID){
        pthread_mutex_init(&bklist_mutex[i], NULL);
        ++ i;
    }
}




int bklist_add_nolock(block* b, uint32_t hv){
    assert(NULL != b);
    DEBUG_B(b,"add to bklist[%10x] with hv(%10x)\n",BKLIST_ID(hv),hv);
    
    #ifdef  BKLIST_DEBUG
    int after_count = 0;
    int before_count = bklist_debug(hv);
    #endif
    
    b->l_next = bklist[BKLIST_ID(hv)];
    bklist[BKLIST_ID(hv)] = b;
    b->flags |= BLOCK_FLAGS_BKLIST;
    
    #ifdef  BKLIST_DEBUG
    after_count = bklist_debug(hv);
    assert((before_count + 1) == after_count);
    #endif
    assert(0 == IN_LRU_COLD(b->slab_id));
    assert(0 == IN_LRU_WARM(b->slab_id));
    lru_add(b);
    if(g_config.enable_backup) backup_add(b);
    return 0;
}

int bklist_add(block* b){
    assert(NULL != b);
    uint32_t hv = hash(BLOCK_k(b),b->nkey);
    BK_LIST_LOCK(hv);
    
    bklist_add_nolock(b, hv);

    BK_LIST_UNLOCK(hv);
    return 0;    
}


int bklist_replace_no_lock(block* old_b, block* new_b, uint32_t hv){
    assert(NULL != old_b && NULL != new_b && 0 != hv);
    assert(old_b != new_b);
    assert(new_b->l_next == old_b->l_next);
    block** head = &bklist[BKLIST_ID(hv)];
    block* it = *head;
    block* b_before = NULL;
    DEBUG_B(old_b,"block will be replace in bklist[%10x] with hv(%10x)\n",BKLIST_ID(hv),hv);

    #ifdef  BKLIST_DEBUG
    int after_count = 0;
    int before_count = bklist_debug(hv);
    #endif
    while(NULL != it){

        //DEBUG_B(it, "block to be compared in bklist[%10x] \n",BKLIST_ID(hv));
        if(it == old_b){
            if(it == *head){
                assert(0 != (it->flags & BLOCK_FLAGS_BKLIST));
                DEBUG_B(it, "block has been found in bklist[%10x] at head\n",BKLIST_ID(hv));
                *head = new_b;
                it->flags &= ~ BLOCK_FLAGS_BKLIST;
                it->l_next = NULL;
                #ifdef  BKLIST_DEBUG
                after_count = bklist_debug(hv);
                assert(before_count == after_count);
                #endif
                return 0;
                
            }

            DEBUG_B(it,"block has been found in bklist[%10x]\n",BKLIST_ID(hv));

            if(b_before){
                b_before->l_next = new_b;
            }
            it->flags &= ~ BLOCK_FLAGS_BKLIST;
            it->l_next = NULL;
            #ifdef  BKLIST_DEBUG
            after_count = bklist_debug(hv);
            assert(before_count == after_count);
            #endif
            return 0;
        }
        
        b_before = it;
        it = it->l_next;
    }
    #ifdef  BKLIST_DEBUG
    after_count = bklist_debug(hv);
    assert(before_count == after_count);
    #endif
    DEBUG_B(old_b,"block can't found in bklist[%10x] with hv(%10x)\n",BKLIST_ID(hv),hv);
    return -1;    
}


block* bklist_find_no_lock(const uint32_t nkey, const char* key,uint32_t hv){
    assert(0 < nkey && NULL != key);
    block** head = &bklist[BKLIST_ID(hv)];
    block* it = *head;
    block* b_before = NULL;
    DEBUG_B_K(nkey,key,"block will be found in bklist[%10x] with hv(%10x)\n",BKLIST_ID(hv),hv);

    #ifdef  BKLIST_DEBUG
    int after_count = 0;
    int before_count = bklist_debug(hv);
    #endif



    while(NULL != it){

        //DEBUG_B(it, "block to be compared in bklist[%10x] \n",BKLIST_ID(hv));
        if(it->nkey == nkey && (0 == memcmp(key,BLOCK_k(it),nkey))){
            if(it == *head){
                it->time = get_current_time_sec();
                if(0 == (it->flags & BLOCK_FLAGS_BKLIST)){
                    LOG_W("may be some bug!!\n");
                    it->flags |= BLOCK_FLAGS_BKLIST;
                }
                DEBUG_B(it, "block has been found in bklist[%10x] at head\n",BKLIST_ID(hv));

                #ifdef  BKLIST_DEBUG
                after_count = bklist_debug(hv);
                assert(before_count == after_count);
                #endif

                return it;
            }

            DEBUG_B(it,"block has been found in bklist[%10x]\n",BKLIST_ID(hv));

            if(b_before){
                b_before->l_next = it->l_next;
            }
            it->l_next = *head;
            *head = it;
            if(0 == (it->flags & BLOCK_FLAGS_BKLIST)){
                LOG_W("may be some bug!!\n");
                it->flags |= BLOCK_FLAGS_BKLIST;
            }
            it->time = get_current_time_sec();
            
            #ifdef  BKLIST_DEBUG
            after_count = bklist_debug(hv);
            assert(before_count == after_count);
            #endif
            
            return it;
        }
        
        b_before = it;
        it = it->l_next;
    }
    #ifdef  BKLIST_DEBUG
    after_count = bklist_debug(hv);
    assert(before_count == after_count);
    #endif
    DEBUG_B_K(nkey,key,"block can't found in bklist[%10x] with hv(%10x)\n",BKLIST_ID(hv),hv);
    return NULL;
}

block* bklist_find(const int nkey, const char* key){
    assert(0 < nkey && NULL != key);
    uint32_t hv = hash(key,nkey);
    block* b = NULL;
    BK_LIST_LOCK(hv);
    
    #ifdef  BKLIST_DEBUG
    int after_count = 0;
    int before_count = bklist_debug(hv);
    #endif
    
    b = bklist_find_no_lock(nkey,key,hv);
    if(NULL != b){
        b->nref ++;
    }
    
    #ifdef  BKLIST_DEBUG
    after_count = bklist_debug(hv);
    assert(before_count == after_count);
    #endif
    
    BK_LIST_UNLOCK(hv);
    return b;

}
int bklist_delete_nolock(block* b, uint32_t hv){
    assert(NULL != b  && 0 < hv);
    assert(0 != (b->flags & BLOCK_FLAGS_BKLIST));
    block** head = &bklist[BKLIST_ID(hv)];
    block* it = *head;
    block* b_before = NULL;
    int ret = -1;

    DEBUG_B(b,"block will be deleted from bklist[%10x] with hv(%10x)\n",BKLIST_ID(hv),hv);
    
    #ifdef  BKLIST_DEBUG
    int after_count = 0;
    int before_count = bklist_debug(hv);
    #endif

  
    while(NULL != it){
        if(it->nkey == b->nkey && (0 == memcmp(BLOCK_k(b),BLOCK_k(it),b->nkey))){
            if(it == *head){
                *head = it->l_next;
                it->l_next = NULL;
                b->flags &= ~ BLOCK_FLAGS_BKLIST;
                DEBUG_B(it,"block has been delete from bklist[%10x] with hv(%10x)\n",BKLIST_ID(hv),hv);
                ret = 0;
                if(g_config.enable_backup) backup_del(b);
                break;
            }
            if(b_before){
                b_before->l_next = it->l_next;
            }
            b->flags &= ~ BLOCK_FLAGS_BKLIST;
            b->l_next = NULL;
            ret = 0;
            if(g_config.enable_backup) backup_del(b);
            break;
        }else{
            b_before = it;
        }
        it = it->l_next;
    }
    #ifdef  BKLIST_DEBUG
    after_count= bklist_debug(hv);
    if(0 == ret){
        assert(before_count == (after_count + 1));
    }else{
        assert(before_count == after_count);
    }
    #endif
    return ret;
}

int bklist_delete(block* b){
    assert(NULL != b);
    uint32_t hv = hash(BLOCK_k(b),b->nkey);
    int ret = -1;
    BK_LIST_LOCK(hv);
    ret = bklist_delete_nolock(b,hv);
    BK_LIST_UNLOCK(hv);
    return ret;
}



void bklist_ref(block* b, int ref_up, int in_lock){
    assert(NULL != b);
    uint32_t hv = 0;
    if(0 != in_lock){
        hv = hash(BLOCK_k(b),b->nkey);
        BK_LIST_LOCK(hv);
    }
    0 == ref_up? b->nref -- : b->nref ++;
    if(0 != in_lock){
        BK_LIST_UNLOCK(hv);
    }
}

void  bklist_lock(const int nkey, const char* key, uint32_t* hv){
    assert(0 != nkey && NULL != key && NULL != hv);
    *hv = hash(key,nkey);
    BK_LIST_LOCK(*hv);
    
}



int bklist_try_lock(const int nkey, const char* key,uint32_t* hv ){
    assert(0 != nkey && NULL != key && NULL != hv);
    *hv = hash(key,nkey);
    return (0 == BK_LIST_TRY_LOCK(*hv));
}

void bklist_unlock(uint32_t hv){
    BK_LIST_UNLOCK(hv);
}


char* bklist_stats(uint64_t* r_total){
    uint32_t i = 0;
    uint64_t total = 0;
    int total_len = 0;
    char** info_bklist = calloc(1, sizeof(char*) * BKLIST_MAX_ID);
    for(; i < BKLIST_MAX_ID; ++i){
        BK_LIST_LOCK(i);
        
        block* b = bklist[i];
        if(NULL != b){
            DEBUG_I("bklist[%x] LOCK[%x]\n",i,BKLIST_LOCK_ID(i));
        }
        int count = 0;
        while(NULL != b){
            //block_stats(b);
            b = b->l_next;
            count ++;
        }
        if(0 < count){
            asprintf(&(info_bklist[i]), "bklist[%x] LOCK[%x] has %d block\n",i,BKLIST_LOCK_ID(i),count);
            total_len += strlen(info_bklist[i]);
            total += count;
           
        }
        BK_LIST_UNLOCK(i);
        
    }
    char* summary = NULL;
    asprintf(&summary, "total block in bklist is %lu\n",total);
    total_len += strlen(summary);
    char* content_all = malloc(total_len + 1);
    content_all[0] = '\0';
    i = 0;
    for(; i < BKLIST_MAX_ID; ++i){
        if(NULL != info_bklist[i]){
            content_all = strcat(content_all,info_bklist[i]);
        }
    }
    free(info_bklist);
    content_all = strcat(content_all,summary);
    *r_total = total;
    return content_all;
}

