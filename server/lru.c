/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "lru.h"

#include "config.h"

#include "log.h"
#include "clock.h"
#include "hash.h"
#include "bklist.h"
#include "stats.h"
#include "net.h"

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>



extern config g_config;
extern stats g_stats;
static block** lru_head = NULL;
static block** lru_tail = NULL;
static pthread_mutex_t lru_lock[LRU_MAX_ID] = { PTHREAD_MUTEX_INITIALIZER };
typedef struct __lru_cond{
    pthread_cond_t cond;
    int value;

} _lru_cond;
static _lru_cond cond[2];
static pthread_mutex_t lock;



#define LRU_LOG_PARAM g_config.block_min_time_no_activity,g_config.block_min_time_no_activity,g_config.block_max_time_no_activity,g_config.block_max_time_no_activity,old



//#define LRU_DEBUG
#ifdef LRU_DEBUG
static int lru_debug_has(block* b, block* lru_h, block* lru_t, int has);
static uint64_t lru_stats(slab* s, int* b_in_hot, int* b_in_warm, int* b_in_cold);

static int lru_debug_has(block* b, block* lru_h, block* lru_t, int has){
    assert(NULL != b);
    int ret_h = 0;
    int total_h = 0;
    int ret_t = 0;
    int total_t = 0;
    block* tail_ok = lru_t;
    block* head_ok = lru_h;
    block* tail = NULL;
    block* head = NULL;
    while(NULL != lru_h){
        if((lru_h->nkey == b->nkey) && (0 == memcmp(BLOCK_k(b),BLOCK_k(lru_h),b->nkey))){
             ret_h = 1;
             if(0 == has){
                LOG_B(b,"from head the same Block! BUG !!\n");
              }
        }
        //LOG_B(lru_h,"in lru from h2t\n");
        if(NULL == lru_h->next){
            tail = lru_h;
        }
        lru_h = lru_h->next;
        total_h ++;   
    }
    while(NULL != lru_t){
        if((lru_t->nkey == b->nkey) && (0 == memcmp(BLOCK_k(b),BLOCK_k(lru_t),b->nkey))){
             ret_t = 1;
             if(0 == has){
                LOG_B(b,"from tail the same Block! BUG !!\n");
              }
        }
        //LOG_B(lru_t,"in lru from t2h\n");
        if(NULL == lru_t->prev){
            head = lru_t;
        }
        lru_t = lru_t->prev;
        total_t ++;   
    }
    assert(head_ok == head);
    assert(tail_ok == tail);
    assert(total_h == total_t);
    assert(ret_t == ret_h);
    if(NULL != lru_h && NULL != lru_t){
        assert(has == ret_t);
    }
    return total_h;
}



static uint64_t lru_stats(slab* s, int* b_in_hot, int* b_in_warm, int* b_in_cold){
    assert(NULL != s && NULL != b_in_hot && NULL != b_in_warm && NULL != b_in_cold);
    uint64_t total = 0;
    LRU_HOT_LOCK(s->id);  
    block* b = lru_head[LRU_HOT_ID(s->id)];
    while(NULL != b){
        block_stats(b);
        b = b->next;
        (*b_in_hot) ++;
    }
    total += *b_in_hot;
    LRU_HOT_UNLOCK(s->id);
    
    LRU_WARM_LOCK(s->id);
    b = lru_head[LRU_WARM_ID(s->id)];
    while(NULL != b){
        block_stats(b);
        b = b->next;
        (*b_in_warm) ++;
    }
    total += *b_in_warm;
    LRU_WARM_UNLOCK(s->id);
    
    LRU_COLD_LOCK(s->id);
    b = lru_head[LRU_COLD_ID(s->id)];
    while(NULL != b){
        block_stats(b);
        b = b->next;
        (*b_in_cold) ++;
    }
    total += *b_in_cold;
    LRU_COLD_UNLOCK(s->id);
    return total;
}

#define LOG_BB LOG_B
#else
#define LOG_BB(b,f_, ...)
#endif

static void *lru_update_looper(void *arg);
static void lru_add_nolock(block* b, block** head, block** tail, int to_head);
static void lru_del_nolock(block* b, block** head, block** tail);

static void lru_add_nolock(block* b, block** head, block** tail, int to_head){
    assert(NULL != b);
    assert(0 == (b->flags & BLOCK_FLAGS_LRU));
    assert(0 == (b->flags & BLOCK_FLAGS_SLAB));
    #ifdef LRU_DEBUG
    int total_b = lru_debug_has(b,*head,*tail,0);
    #endif
    if(NULL == *head){
        assert(NULL == *tail);
        *tail = *head = b;
        b->prev = b->next = NULL;       
    }else{
        if(1 == to_head){
            b->next = *head;
            if(b->next) b->next->prev = b;
            *head = b;
            b->prev = NULL;

        }else{
            b->prev = *tail;
            if(b->prev) b->prev->next = b;
            *tail = b;
            b->next = NULL;
        }
    }
    b->flags |= BLOCK_FLAGS_LRU;
    slab* s = slab_get(SLAB_ID(b->slab_id));
    if(IN_LRU_COLD(b->slab_id)){
        SLAB_STATS_LOCK(s);
        s->stats.lru_cold ++;
        SLAB_STATS_UNLOCK(s);
    }else if(IN_LRU_WARM(b->slab_id)){
        SLAB_STATS_LOCK(s);
        s->stats.lru_warm ++;
        SLAB_STATS_UNLOCK(s);
    }else{
        SLAB_STATS_LOCK(s);
        s->stats.lru_hot ++;
        SLAB_STATS_UNLOCK(s);
    }
    #ifdef LRU_DEBUG
    int total_e = lru_debug_has(b,*head,*tail,1);
    assert((total_b + 1) == total_e);
    #endif

}


static void lru_del_nolock(block* b, block** head, block** tail){
    assert(NULL != b );
    assert(0 != (b->flags & BLOCK_FLAGS_LRU));
    assert(0 == (b->flags & BLOCK_FLAGS_SLAB));
    assert(NULL != *head && NULL != *tail);
    #ifdef LRU_DEBUG
    int total_b = lru_debug_has(b,*head,*tail,1);
    #endif
    if(*head == *tail){
       assert(*head == b);
       *head = *tail = NULL;
    }else if(*head == b){
       *head = b->next;
       if(b->next) b->next->prev = NULL;
    }else if(*tail == b){
       *tail = b->prev;
       if(b->prev) b->prev->next = NULL;
    }else{
       if(b->prev) b->prev->next = b->next;
       if(b->next) b->next->prev = b->prev;
    }
    b->prev = b->next = NULL;
    LOG_BB(b,"remove form LRU\n");
    
    slab* s = slab_get(SLAB_ID(b->slab_id));
    if(IN_LRU_COLD(b->slab_id)){
        SLAB_STATS_LOCK(s);
        s->stats.lru_cold --;
        SLAB_STATS_UNLOCK(s);
    }else if(IN_LRU_WARM(b->slab_id)){
        SLAB_STATS_LOCK(s);
        s->stats.lru_warm --;
        SLAB_STATS_UNLOCK(s);
    }else{
        SLAB_STATS_LOCK(s);
        s->stats.lru_hot --;
        SLAB_STATS_UNLOCK(s);
    }
    
    b->slab_id = SLAB_ID(b->slab_id);
    b->flags &= ~BLOCK_FLAGS_LRU;
    #ifdef LRU_DEBUG
    int total_e = lru_debug_has(b,*head,*tail,0);
    assert(total_b == (total_e + 1));
    #endif

}




static void wait_run(){
    pthread_mutex_lock(&lock);
    while(0 == cond[0].value){
        LOG_I("pthread_cond_wait\n");
        pthread_cond_wait(&(cond[0].cond), &lock);
    }
    pthread_mutex_unlock(&lock);
}

static void signal_run(int on){
    pthread_mutex_lock(&lock);
    LOG_I("cond[0].value = %d, on = %d\n", cond[0].value , on);
    cond[0].value = on;
    pthread_cond_signal(&(cond[0].cond));
    pthread_mutex_unlock(&lock);
}

void lru_switch(int on){
   if(0 == g_config.enable_lru_balance) return;
   signal_run(on);
}

static void *lru_update_looper(void *arg __attribute__((unused))){
    int i = 0;
    uint32_t count = 0;
    uint32_t sleep_time = 0;
    uint32_t switcher = 0;
    
    slab* s = NULL;
    while(i < g_config.slab_max_id){
        wait_run();
        get_lru_parameter(NULL,NULL,&count,&sleep_time,&switcher);
        s = slab_get(i);
        if(1 == switcher){
            lru_update(s,count);
        }else if(2 == switcher){
            lru_update(s,count);
            SLAB_STATS_LOCK(s);
            if(count > s->stats.lru_cold) count = s->stats.lru_cold;
            SLAB_STATS_UNLOCK(s);
            if(0 < count){
                int recycled = lru_recycle(s,count,0);
                if(recycled > 0) LOG_I("%d blocks are recycled at slab[%d]\n",recycled, s->id);
            }
        }
        usleep(1 * 1000 * 100);
        ++ i;
        if(i == g_config.slab_max_id){
            i = 0;
            stats_lock();
            g_stats.op_lru ++;
            stats_unlock();
            sleep(sleep_time);
            
       }
    }
    return NULL;
}
void lru_init(){
    if(NULL == lru_head){
        lru_head = calloc(LRU_MAX_ID,sizeof(block*));
    }
    if(NULL == lru_tail){
        lru_tail = calloc(LRU_MAX_ID,sizeof(block*));
	}
    assert(NULL != lru_head);
    assert(NULL != lru_tail);   
    lru_run();
    uint32_t switcher = 0;
    get_lru_parameter(NULL,NULL,NULL,NULL,&switcher);
    lru_switch(switcher);

}

void lru_run(){
    if(g_config.enable_lru_balance){
        pthread_attr_t  attr;
        pthread_attr_init(&attr);
        pthread_cond_init(&(cond[0].cond), NULL);
        pthread_cond_init(&(cond[1].cond), NULL);
        pthread_mutex_init(&lock, NULL);
        pthread_mutex_lock(&lock);
        cond[0].value = 0;
        cond[1].value = 1;
        pthread_mutex_unlock(&lock);
        pthread_t thread_id;
        if(0 != pthread_create(&thread_id,&attr,lru_update_looper,NULL)){
                perror("pthread_create() lru_update_looper");
                exit(EXIT_FAILURE);
        }
    }
}

void lru_update(slab* s, uint32_t count){
    assert(NULL != s);
    uint32_t block_min_time_no_activity = 0;
    uint32_t block_max_time_no_activity = 0;
    get_lru_parameter(&block_min_time_no_activity,&block_max_time_no_activity,NULL,NULL,NULL);
    assert(block_min_time_no_activity < block_max_time_no_activity);
    
        {
    uint32_t try = count;
    time_t time_current = get_current_time_sec();
    LRU_HOT_LOCK(s->id);
    block** tail = &lru_tail[LRU_HOT_ID(s->id)];
    block** head = &lru_head[LRU_HOT_ID(s->id)];
    block* b = *tail;
    int unlock = 1;
    uint32_t still = 0;
    uint32_t move_1 = 0;
    uint32_t move_2 = 0;
    uint32_t lock_3 = 0;
    if(NULL != b){ LOG_I("update slab[%d] HOT\n",s->id);};
    while(0 < try && NULL != b){
        -- try;
        uint32_t hv = 0;
        uint32_t old = 0;
        if(bklist_try_lock(b->nkey,BLOCK_k(b),&hv)){
            LOG_BB(b,"bklist_try_lock ok\n");
            old = time_current - b->time;
            if(old <= block_min_time_no_activity){
                LOG_BB(b,"hot[0-%d],warm[%d-%d],old[%d-max].old[%d] still hot\n",LRU_LOG_PARAM);
                bklist_unlock(hv);
                b = b->prev;
                unlock = 1;
                ++ still;
                continue;
            }
            LOG_BB(b,"hot[0-%d],warm[%d-%d],old[%d-max].old[%d] not hot\n",LRU_LOG_PARAM);
            lru_del_nolock(b,head,tail);
            LRU_HOT_UNLOCK(s->id);
            unlock = 0;
            if(old > block_min_time_no_activity && old < block_max_time_no_activity){
                //move to warm
                LRU_WARM_LOCK(s->id);
                LOG_BB(b,"hot[0-%d],warm[%d-%d],old[%d-max].old[%d] from hot to warm\n",LRU_LOG_PARAM);
                b->slab_id = LRU_WARM_ID(s->id);
                lru_add_nolock(b,&lru_head[LRU_WARM_ID(s->id)],&lru_tail[LRU_WARM_ID(s->id)],1);
                LRU_WARM_UNLOCK(s->id);
                bklist_unlock(hv);
                LRU_HOT_LOCK(s->id);
                block** tail = &lru_tail[LRU_HOT_ID(s->id)];
                b = *tail;
                unlock = 1;
                ++ move_1;
                continue;

            }else if(old >= block_max_time_no_activity){
                //move to cold
                LRU_COLD_LOCK(s->id);
                LOG_BB(b,"hot[0-%d],warm[%d-%d],old[%d-max].old[%d] from hot to cold\n",LRU_LOG_PARAM);
                b->slab_id = LRU_COLD_ID(s->id);
                lru_add_nolock(b,&lru_head[LRU_COLD_ID(s->id)],&lru_tail[LRU_COLD_ID(s->id)],1);
                LRU_COLD_UNLOCK(s->id);
                bklist_unlock(hv);
                LRU_HOT_LOCK(s->id);
                block** tail = &lru_tail[LRU_HOT_ID(s->id)];
                b = *tail;
                unlock = 1;
                ++ move_2;
                continue;
            }
               
        }else{
            LOG_B(b,"bklist_try_lock failed, try the prev block and try is %d\n",try);
            b = b->prev;
            unlock = 1;
            ++ lock_3;
            continue;
        }
    }
    if(1 == unlock){
        LRU_HOT_UNLOCK(s->id);
    }
    if(still || move_1 || move_2 || lock_3){
        LOG_I("still hot : [%-10d] , move to warm: [%-10d] , move to cold: [%-10d] , using: [%-10d]\n",still, move_1,move_2,lock_3);
    }
        }

          {
    uint32_t try = count;
    time_t time_current = get_current_time_sec();
    LRU_WARM_LOCK(s->id);
    block** tail = &lru_tail[LRU_WARM_ID(s->id)];
    block** head = &lru_head[LRU_WARM_ID(s->id)];
    block* b = *tail;
    int unlock = 1;
    uint32_t still = 0;
    uint32_t move_1 = 0;
    uint32_t move_2 = 0;
    uint32_t lock_3 = 0;
    if(NULL != b){ LOG_I("update slab[%d] WARM\n",s->id);};
    while(0 < try && NULL != b){
        -- try;
        uint32_t hv = 0;
        uint32_t old = 0;
        if(bklist_try_lock(b->nkey,BLOCK_k(b),&hv)){
            LOG_BB(b,"bklist_try_lock ok\n");
            old = time_current - b->time;
            if(old > block_min_time_no_activity && old < block_max_time_no_activity){
                LOG_BB(b,"hot[0-%d],warm[%d-%d],old[%d-max].old[%d] still warm\n",LRU_LOG_PARAM);
                bklist_unlock(hv);
                b = b->prev;
                unlock = 1;
                ++ still;
                continue;
            }
            
            LOG_BB(b,"hot[0-%d],warm[%d-%d],old[%d-max].old[%d] not warm\n",LRU_LOG_PARAM);
            lru_del_nolock(b,head,tail);
            LRU_WARM_UNLOCK(s->id);
            unlock = 0;
            if(old <= block_min_time_no_activity){
                //move to hot
                LRU_HOT_LOCK(s->id);
                LOG_BB(b,"hot[0-%d],warm[%d-%d],old[%d-max].old[%d] from warm to hot\n",LRU_LOG_PARAM);
                b->slab_id = LRU_HOT_ID(s->id);
                lru_add_nolock(b,&lru_head[LRU_HOT_ID(s->id)],&lru_tail[LRU_HOT_ID(s->id)],1);
                LRU_HOT_UNLOCK(s->id);
                bklist_unlock(hv);
                LRU_WARM_LOCK(s->id);
                block** tail = &lru_tail[LRU_WARM_ID(s->id)];
                b = *tail;
                unlock = 1;
                ++ move_1;
                continue;
            }else if(old >= block_max_time_no_activity){
                //move to cold
                LRU_COLD_LOCK(s->id);
                LOG_BB(b,"hot[0-%d],warm[%d-%d],old[%d-max].old[%d] from warm to cold\n",LRU_LOG_PARAM);
                b->slab_id = LRU_COLD_ID(s->id);
                lru_add_nolock(b,&lru_head[LRU_COLD_ID(s->id)],&lru_tail[LRU_COLD_ID(s->id)],1);
                LRU_COLD_UNLOCK(s->id);
                bklist_unlock(hv);
                LRU_WARM_LOCK(s->id);
                block** tail = &lru_tail[LRU_WARM_ID(s->id)];
                b = *tail;
                unlock = 1;
                ++ move_2;
                continue;
            }
        }else{
                LOG_B(b,"bklist_try_lock failed, try the prev block and try is %d\n",try);
                b = b->prev;
                unlock = 1;
                ++ lock_3;
                continue;
        }
    }
    if(1 == unlock){
        LRU_WARM_UNLOCK(s->id);
    }
    if(still || move_1 || move_2 || lock_3){
        LOG_I("still warm: [%-10d] , move to hot : [%-10d] , move to cold: [%-10d] , using: [%-10d]\n",still, move_1,move_2,lock_3);
    }
        } 
        {
    uint32_t try = count;
    time_t time_current = get_current_time_sec();
    LRU_COLD_LOCK(s->id);
    block** tail = &lru_tail[LRU_COLD_ID(s->id)];
    block** head = &lru_head[LRU_COLD_ID(s->id)];
    block* b = *tail;
    int unlock = 1;
    uint32_t still = 0;
    uint32_t move_1 = 0;
    uint32_t move_2 = 0;
    uint32_t lock_3 = 0;
    if(NULL != b){ LOG_I("update slab[%d] COLD\n",s->id);};
    while(0 < try && NULL != b){
        -- try;
        uint32_t hv = 0;
        uint32_t old = 0;
        if(bklist_try_lock(b->nkey,BLOCK_k(b),&hv)){
            LOG_BB(b,"bklist_try_lock ok\n");
            old = time_current - b->time;
            if(old >= block_max_time_no_activity){
                LOG_BB(b,"hot[0-%d],warm[%d-%d],old[%d-max].old[%d] still cold\n",LRU_LOG_PARAM);
                bklist_unlock(hv);
                b = b->prev;
                unlock = 1;
                ++ still;
                continue;
            }else{
                LOG_BB(b,"hot[0-%d],warm[%d-%d],old[%d-max].old[%d] not cold\n",LRU_LOG_PARAM);
                lru_del_nolock(b,head,tail);
                LRU_COLD_UNLOCK(s->id);
                unlock = 0;
                if(old <= block_min_time_no_activity){
                    //move to hot
                    LRU_HOT_LOCK(s->id);
                    LOG_BB(b,"hot[0-%d],warm[%d-%d],old[%d-max].old[%d] from cold to hot\n",LRU_LOG_PARAM);
                    
                    b->slab_id = LRU_HOT_ID(s->id);
                    lru_add_nolock(b,&lru_head[LRU_HOT_ID(s->id)],&lru_tail[LRU_HOT_ID(s->id)],1);

                    LRU_HOT_UNLOCK(s->id);

                    bklist_unlock(hv);
                    LRU_COLD_LOCK(s->id);
                    block** tail = &lru_tail[LRU_COLD_ID(s->id)];
                    b = *tail;
                    unlock = 1;
                    ++ move_1;
                    continue;
                 }else if(old > block_min_time_no_activity && old < block_max_time_no_activity){
                    //move to warm
                    LRU_WARM_LOCK(s->id);
                    LOG_BB(b,"hot[0-%d],warm[%d-%d],old[%d-max].old[%d] from cold to warm\n",LRU_LOG_PARAM);
                    b->slab_id = LRU_WARM_ID(s->id);
                    lru_add_nolock(b,&lru_head[LRU_WARM_ID(s->id)],&lru_tail[LRU_WARM_ID(s->id)],1);
                    LRU_WARM_UNLOCK(s->id);
                    bklist_unlock(hv);
                    LRU_COLD_LOCK(s->id);
                    block** tail = &lru_tail[LRU_COLD_ID(s->id)];
                    b = *tail;
                    unlock = 1;
                    ++ move_2;
                    continue;
                 }
               }
            }else{
                LOG_B(b,"bklist_try_lock failed, try the prev block and try is %d\n",try);
                b = b->prev;
                unlock = 1;
                ++ lock_3;
                
                continue;
        }
    }
    if(1 == unlock){
        LRU_COLD_UNLOCK(s->id);
    }
    if(still || move_1 || move_2 || lock_3){
        LOG_I("still cold: [%-10d] , move to hot : [%-10d] , move to warm: [%-10d] , using: [%-10d]\n",still, move_1,move_2,lock_3);
    }
        }

//when multi-thread the debug is error.
#if 0
   #ifdef LRU_DEBUG
   int b_in_hot = 0;
   int b_in_warm = 0;
   int b_in_cold = 0;
   uint64_t b_in_lru = lru_stats(s,&b_in_hot,&b_in_warm,&b_in_cold); 
   assert((int)s->stats.lru_hot == b_in_hot);
   assert((int)s->stats.lru_warm == b_in_warm);
   assert((int)s->stats.lru_cold == b_in_cold);
   LOG_I("slab[%d], b_in_lru[%lu], b_in_hot[%d], b_in_warm[%d], b_in_cold[%d]\n",s->id, b_in_lru,b_in_hot, b_in_warm,b_in_cold);
#endif
   #endif
}


void lru_add(block* b){
    assert(NULL != b);
    int lru_id = b->slab_id;
    LRU_LOCK(lru_id);
    lru_add_nolock(b,&lru_head[lru_id],&lru_tail[lru_id],1);
    LRU_UNLOCK(lru_id);
    
}

void lru_del(block* b){
    assert(NULL != b);
    int lru_id = b->slab_id;
    LRU_LOCK(lru_id);
    lru_del_nolock(b,&lru_head[lru_id],&lru_tail[lru_id]);
    LRU_UNLOCK(lru_id);
}

void lru_replace_onlock(block* old_b, block* new_b){
    assert(NULL != old_b && NULL != new_b);
    assert(old_b != new_b);
    block** head = &lru_head[old_b->slab_id];
    block** tail = &lru_tail[old_b->slab_id];
    #ifdef LRU_DEBUG
    int total_b = lru_debug_has(old_b,*head,*tail,1);
    #endif
    if(old_b->prev){
        old_b->prev->next = new_b;
    }
    if(old_b->next){
        old_b->next->prev = new_b;
    }

    if(*head == *tail){
       assert(*head == old_b);
       *head = *tail = new_b;
    }else if(*head == old_b){
        *head = new_b;
    }else if(*tail == old_b){
       *tail = new_b;
    }
    old_b->prev = old_b->next = NULL;
    old_b->nkey = 0;
    old_b->slab_id = SLAB_ID(old_b->slab_id);
    old_b->flags &= ~BLOCK_FLAGS_LRU;
    #ifdef LRU_DEBUG
    int total_e_1 = lru_debug_has(old_b,*head,*tail,0);
    int total_e_2 = lru_debug_has(new_b,*head,*tail,1);
    assert(total_b == total_e_1);
    assert(total_b == total_e_2);
    #endif
}


int lru_recycle(slab* s, uint32_t try_times, int update){
    assert(NULL != s);
    uint32_t try = try_times;
    int i = 0;
    int recycled = 0;
    if(1 == update) lru_update(s, try);
    while(0 < try){
        block** head = NULL;
        block** tail = NULL;
        LRU_COLD_LOCK(s->id);
        head = &lru_head[LRU_COLD_ID(s->id)];
        tail = &lru_tail[LRU_COLD_ID(s->id)];
        block* b = *tail;
        uint32_t hv = 0;
        -- try;
        if(NULL != b){
            if(bklist_try_lock(b->nkey,BLOCK_k(b),&hv)){
                LOG_B(b,"no body use it,get the lock!!\n");
                if(0 < b->nref){
                    LOG_B_W(b,"b is used!! nref = %d\n",b->nref);
                    bklist_unlock(hv);
                    LRU_COLD_UNLOCK(s->id);
                    ++ i;
                    usleep(10 * (4 + i));
                    continue;
                }else{
                    assert(0 != IN_LRU_COLD(b->slab_id));
                    assert(0 == IN_LRU_WARM(b->slab_id));
                    lru_del_nolock(b,head,tail);
                    int ret_delete = bklist_delete_nolock(b,hv);
                    assert(0 == ret_delete);
                    LOG_B_W(b,"has been recycle!!, slab[%d] recycle count = %lu\n",s->id, s->stats.count_recycled);
                    slab_free(b);
                    SLAB_STATS_LOCK(s);
                    s->stats.count_recycled ++;
                    recycled ++;
                    SLAB_STATS_UNLOCK(s);
                    bklist_unlock(hv);
                    LRU_COLD_UNLOCK(s->id);
                    continue;

                }
            }else{
                LOG_B(b,"some body use it,can't hold the lock!!\n");
                LRU_COLD_UNLOCK(s->id);
                ++ i;
                usleep(10 * (4 + i));
                continue;
            }
        }else{
            LOG_W("no block in cold LRU[%d]\n",s->id);
            LRU_COLD_UNLOCK(s->id);
            break;
        }
    }
    return recycled;
}

void lru_lock_mutex(uint64_t lru_id){
    LRU_LOCK(lru_id);
}

int lru_try_lock_mutex(uint64_t lru_id){
    return (0 == LRU_TRY_LOCK(lru_id));
}


void lru_unlock_mutex(uint64_t lru_id){
    LRU_UNLOCK(lru_id);

}

