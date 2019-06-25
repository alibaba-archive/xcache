/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "slab.h"
#include "config.h"
#include "log.h"
#include "clock.h"
#include "page.h"
#include "lru.h"
#include "bklist.h"
#include "net.h"
#include "stats.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>



#define SLAB_SPLIT_BIG_BLOCK

extern config g_config;

static slab* list = NULL;
static void slab_realloc(slab* s,page* p);
static int slab_id(const uint32_t size);

typedef struct __lru_cond{
    pthread_cond_t cond;
    int value;

} _lru_cond;
static _lru_cond cond[2];
static pthread_mutex_t lock;


static pthread_mutex_t slab_lru_lock = PTHREAD_MUTEX_INITIALIZER;




//#define SLAB_DEBUG
#ifdef SLAB_DEBUG
static void slab_debug(block* father, int n, int last_child_size);
static void slab_debug(block* father, int n, int last_child_size){
    assert(NULL != father);
    LOG_B(father,"father found!\n");
    assert(0 == father->nvalue);
    block** child_ptr = (block**)BLOCK_c(father);
    block* child = *child_ptr;
    int count = 0;
    while(NULL != child){
        LOG_B(child,"child found!\n");
        if(NULL == child->next){
            assert(child->nvalue == last_child_size);
        }else{
            assert(child->nvalue == (g_config.slab_max_size - sizeof(block)));
        }
        child = child->next;
        count ++;
        
     }
    assert(n == (count - 1));
}
#endif




static void slab_realloc(slab* s,page* p){
    assert(NULL != s && NULL != p);
    int i = s->fixed_block_count;
    char* buf = p->buffer;
    while(i > 0){
        block* b = (block*)buf;
        memset(b,0,s->fixed_block_size);
        b->next = s->block_list_free;
        if(b->next){
            b->next->prev = b;
        }
        b->flags = BLOCK_FLAGS_SLAB;
        b->slab_id = s->id;
        s->block_list_free = b;
        s->block_count_free ++;
        buf += s->fixed_block_size;
        i --;
    }
    p->next = s->page_list;
    if(p->next){
        p->next->prev = p;
    }
    s->page_list = p;
    s->page_count ++;
}

static int slab_id(const uint32_t size){
    uint32_t i = 0;
    if(size == g_config.slab_max_size){
        return g_config.slab_max_id - 1;
    }
    for(; i < g_config.slab_max_id; ++i){
        if(list[i].fixed_block_size >= size){
            return i;
        }
    }
    return -1;
}

slab* slab_get(uint8_t id){
    assert(id < g_config.slab_max_id && NULL != list);
    return &list[id];
    
}

static void signal_run(int on){
    pthread_mutex_lock(&lock);
    LOG_I("cond[0].value = %d, on = %d\n", cond[0].value , on);
    cond[0].value = on;
    if(1 == on) cond[1].value = 0;
    pthread_cond_signal(&(cond[0].cond));
    pthread_mutex_unlock(&lock);
}

static void signal_finished(){
    pthread_mutex_lock(&lock);
    LOG_I("signal!\n");
    cond[1].value = 1;
    pthread_cond_signal(&(cond[1].cond));
    pthread_mutex_unlock(&lock);
}

static void wait_run(int once){
    pthread_mutex_lock(&lock);
    LOG_I("cond[0].value = %d\n",cond[0].value);
    while(0 == cond[0].value){
        LOG_I("pthread_cond_wait\n");
        pthread_cond_wait(&(cond[0].cond), &lock);
    }
    if(1 == once) cond[0].value = 0;
    pthread_mutex_unlock(&lock);
}

static void wait_finished(){
    pthread_mutex_lock(&lock);
    LOG_I("cond[1].value = %d\n",cond[1].value);
    while(0 == cond[1].value){
        LOG_I("pthread_cond_wait\n");
        pthread_cond_wait(&(cond[1].cond), &lock);
    }
    pthread_mutex_unlock(&lock);

}



static void *slab_recycle_looper(void *arg __attribute__((unused))){
    slab* s = NULL;
    while(1){
        wait_run(1);
        
        int i = 0;
        LOG_I("net_switch(0)\n");
        net_switch(0);
        stats_wait();
        LOG_I("slab_lock\n");
        slab_lock();
        int b_free = get_free_page_count();
        while(i < g_config.slab_max_id){
            s = slab_get(i);
            slab_recycle(s);
            ++ i;
        }
        int a_free = get_free_page_count();
        if(0 != (a_free - b_free)) LOG_I("a_free = %d b_free = %d, new_free = %d\n",a_free,b_free, a_free - b_free);
        LOG_I("slab_unlock\n");
        slab_unlock();
        net_switch(1);
#if 0
        usleep(1 * 1000 * 1000);
        slab_switch(1);
#endif
    }
    return NULL;
}

static void slab_run(){
    if(g_config.enable_slab_recycle){
        
        pthread_attr_t  attr_1;
        pthread_attr_init(&attr_1);
        pthread_t thread_id_1;
        pthread_cond_init(&(cond[0].cond), NULL);
        pthread_cond_init(&(cond[1].cond), NULL);
        pthread_mutex_init(&lock, NULL);
        pthread_mutex_lock(&lock);
        cond[0].value = 0;
        cond[1].value = 1;
        pthread_mutex_unlock(&lock);
        if(0 != pthread_create(&thread_id_1,&attr_1,slab_recycle_looper,NULL)){
                perror("pthread_create() slab_recycle_looper");
                exit(EXIT_FAILURE);
        }
    }
}

void slab_lock(){
    //LOG_I("lock\n");
    pthread_mutex_lock(&slab_lru_lock);

}
void slab_unlock(){
    //LOG_I("unlock\n");
    pthread_mutex_unlock(&slab_lru_lock);
}

void slab_switch(int on){
    if(0 == g_config.enable_slab_recycle) return;
    LOG_I("on = %d\n",on);
    signal_run(on);
    if(0 == on){
        wait_finished();
    }
}

int slab_init(){
    if(NULL == list){
        list = calloc(g_config.slab_max_id,sizeof(slab));
    }
    assert(NULL != list);
    uint8_t id = 0;
    uint32_t size = g_config.slab_min_size + sizeof(block);
    page* p = page_alloc();
    if(NULL == p){
        LOG_E("the first page is alloc failed!\n");
        return -1;
    }
    while(id < g_config.slab_max_id){
        slab* s = &list[id];
        pthread_mutex_init(&s->lock,NULL);
        pthread_mutex_init(&s->stats.lock_stats,NULL);
        s->id = id;
        ++ id;
        LOG_I("create new slab id = %d\n",s->id);
    }
    id = 0;
    while(size <= g_config.slab_max_size && id < g_config.slab_max_id){
        page* p_next = page_alloc();
        if(NULL == p_next){
            id = g_config.slab_max_id -1;
        }
        if(id == g_config.slab_max_id -1){
            size = g_config.slab_max_size;
        }
        assert(0 == size % g_config.block_size_align);
        slab* s = &list[id];
        s->fixed_block_size = size;
        s->fixed_block_count = g_config.page_size/size;
        p->next = s->page_list;
        if(p->next){
            p->next->prev = p;
        }
        s->page_list = p;
        s->page_count ++;
        int i = s->fixed_block_count;
        char* buf = p->buffer;
        while(i > 0){
            block* b = (block*)buf;
            memset(b,0,s->fixed_block_size);
            b->slab_id = id;
            b->flags = BLOCK_FLAGS_SLAB;
            b->next = s->block_list_free;
            if(b->next){
                b->next->prev = b;
            }
            s->block_list_free = b;
            s->block_count_free ++;
            buf += s->fixed_block_size;
            i -= 1;    
        }
        LOG_I("create new slab fixed_block_size = %d, fixed_block_count = %d,block_count_free = %d, id = %d\n",s->fixed_block_size,s->fixed_block_count,s->block_count_free,s->id);
        id ++;
        size *= g_config.slab_factor;
        if (0 != size % g_config.block_size_align){
            size += (g_config.block_size_align - (size % g_config.block_size_align));
        }
        if(size >= g_config.slab_max_size && id < g_config.slab_max_id -1){
            size = g_config.slab_max_size;
            id  = g_config.slab_max_id -1;
        }
        p = p_next;
    }
    slab_run();
    return 0;
    
}



static block* slab_alloc_list(uint32_t size){
    assert(size > g_config.slab_max_size);
    //store pointer value
    size += sizeof(block*);
    block* father = slab_alloc(g_config.slab_max_size);
    if(NULL == father){
        LOG_E("cann't alloc father block\n");
        return NULL;
    }
    LOG_B(father,"split father block[%u]\n",g_config.slab_max_size);
    father->flags |= BLOCK_FLAGS_HAVE_CHILD;
    size -= g_config.slab_max_size;
    int n = size / (g_config.slab_max_size - sizeof(block));
    uint32_t last_child_size = size % (g_config.slab_max_size - sizeof(block));
    if(0 != last_child_size){
        last_child_size += sizeof(block);
        assert(last_child_size < g_config.slab_max_size);
    }
    block* before = NULL;
    int i = 0;
    while(i < n){
        
        block* child = slab_alloc(g_config.slab_max_size);
        if(NULL == child){
            slab_free(father); // should free the child list
            return NULL;
        }
        if(NULL == before){    
            memcpy(BLOCK_c(father),&child,sizeof(block*));
        }
        child->l_next= father;
        child->prev = before;
        if(NULL != before){
            before->next = child;
        }
        child->nkey = (i + 1);
        child->nvalue = (g_config.slab_max_size - sizeof(block));
        child->flags |= BLOCK_FLAGS_CHILD;
        child->next = NULL;
        before = child;
        ++ i;
        LOG_B(child,"split in to %d block[%u]\n",i,g_config.slab_max_size);
    }
    if(0 < last_child_size){
        block* last = slab_alloc(last_child_size);
        if(NULL == last){
            slab_free(father); // should free the child list
            return NULL;
        }
        
        if(NULL == before){
            memcpy(BLOCK_c(father),&last,sizeof(block*));
        }
        last->l_next = father;
        last->prev = before;
        if(NULL != before){
            before->next = last;
        }
        last->nvalue = (last_child_size - sizeof(block));
        last->nkey = (i + 1);
        last->flags |= BLOCK_FLAGS_CHILD;
        last->next = NULL;
        LOG_B(last,"split last block[%u]\n",last_child_size);
    }
#ifdef SLAB_DEBUG
    slab_debug(father,n,last_child_size - sizeof(block));
#endif
    return father;
}


block* slab_alloc(const int size){
    int id = slab_id(size);
    if(-1 == id){
        
#ifdef SLAB_SPLIT_BIG_BLOCK
        LOG_W("block size is too big!! %d > %u, begin to split it into small block!\n",size,g_config.slab_max_size);
        return slab_alloc_list(size);
#else
        LOG_E("block size is too big!! %d > %u\n",size,g_config.slab_max_size);
        return NULL;
#endif
    }
    LOG_I("size(%d) to slabs[%d:%u]\n",size,id,list[id].fixed_block_size);
    slab* s = &list[id];
    block* b = NULL;
    int unlocked = 0;
    SLAB_LOCK(s);
    if (s->block_count_free <= 0){
        LOG_I("slab_alloc s->block_count_free is 0\n");
        page* p = page_alloc();
        if(NULL == p){
           uint8_t auto_expand = 0;
           get_memory_parameter(&auto_expand,NULL,NULL);
           if((auto_expand > 0) && (0 == page_expand(MEM_SIZE_EXPAND))){
               LOG_W("page_expand %dM OK!\n",MEM_SIZE_EXPAND);
               p =  page_alloc();
            } 
        }
        if(NULL != p){
            slab_realloc(s,p);
            LOG_I("slab_realloc block_count_free = %d\n",s->block_count_free);
        }else{
            LOG_E("no free page!!\n");
        }
    }
    if (s->block_count_free <= 0){
        unlocked = 1;
        SLAB_UNLOCK(s);
        LOG_W("Retry to realloc slab failed, try to recycle!\n");
        lru_recycle(s, 3, 1);
       
    }
    if(unlocked){
        SLAB_LOCK(s);
        unlocked = 0;
        
    }
    if (s->block_count_free <= 0){
        
        LOG_E("Retry to recycle slab failed!!!\n");
        SLAB_UNLOCK(s);
        return NULL;
       
    }

    b = s->block_list_free;
    s->block_list_free = b->next;
    if (b->next){
        b->next->prev = NULL;
    }
    b->next = NULL;
    b->prev = NULL;
    assert(b->slab_id == s->id);
    s->block_count_free -= 1;
    b->time = get_current_time_sec();
    assert(b->flags == BLOCK_FLAGS_SLAB);
    b->flags &= ~ BLOCK_FLAGS_SLAB;
    s->real_mem_used += size;
    //int waste = ((uint64_t)s->page_count * g_config.page_size) - ((uint64_t)s->block_count_free * s->fixed_block_size) - s->real_mem_used;
    //assert(waste >= 0);
    SLAB_UNLOCK(s);
    return b;
}


void slab_free_nolock(block* b, slab* s){
    assert(NULL != b);
    assert(NULL != s);
    assert(0 == (b->flags & BLOCK_FLAGS_SLAB));
    assert(0 == b->nref);
    if(b->flags & BLOCK_FLAGS_HAVE_CHILD){
        assert(s->real_mem_used >= g_config.slab_max_size);
        s->real_mem_used -= g_config.slab_max_size;
    }else if(b->flags & BLOCK_FLAGS_CHILD){
        assert(s->real_mem_used >= (sizeof(block) + b->nvalue));
        s->real_mem_used -= (sizeof(block)  + b->nvalue);
    }else{
        assert(s->real_mem_used >= (sizeof(block) + b->nkey + b->nvalue));
        s->real_mem_used -= (sizeof(block) + b->nkey + b->nvalue);
    }
    //int waste = ((uint64_t)s->page_count * g_config.page_size) - ((uint64_t)s->block_count_free * s->fixed_block_size) - s->real_mem_used;
    //assert(waste >= 0);
    memset(b,0,s->fixed_block_size);
    b->next = s->block_list_free;
    if(b->next){
        b->next->prev = b;
    }
    s->block_list_free = b;
    s->block_count_free ++;
    b->flags = BLOCK_FLAGS_SLAB;
    //b->flags |= BLOCK_FLAGS_SLAB;
    //b->flags &= ~BLOCK_FLAGS_WILL_FREE;
    b->slab_id = s->id;
    
}


int slab_free(block* b){
    assert(NULL != b);
    if(0 < b->nref){
        b->flags|= BLOCK_FLAGS_WILL_FREE;
        LOG_B(b, "will be free after the block is send ok! see: get_block_end\n");
        return -1;
    }
#ifdef SLAB_SPLIT_BIG_BLOCK
    if(0 != (b->flags & BLOCK_FLAGS_HAVE_CHILD)){
        block** child_ptr = (block**)BLOCK_c(b);
        block* child = *child_ptr;
        block* child_next = NULL;
        while(NULL != child){
            // slab_free will set child->next NULL!
            LOG_B(child,"begin to free child\n");
            child_next = child->next;
            slab_free(child);
            LOG_B(child,"end to free child\n");
            child = child_next;
        }
        
    }
#endif
    slab* s = &list[SLAB_ID(b->slab_id)];
    SLAB_LOCK(s);
#ifdef SLAB_SPLIT_BIG_BLOCK
    int father = 0;
    if(0 != (b->flags & BLOCK_FLAGS_HAVE_CHILD)){
        LOG_B(b,"begin to free father\n");
        father = 1;
    }
#endif
    slab_free_nolock(b,s);
#ifdef SLAB_SPLIT_BIG_BLOCK
    if(0 != father){
        LOG_B(b,"end to free father\n");
    }
#endif
    SLAB_UNLOCK(s);
    return 0;
}


static void slab_stats_page(slab* s, uint64_t* b_normal, uint64_t* b_father, uint64_t* b_child, uint64_t* b_free){
    assert(NULL != s);
    page* p = s->page_list;
    while(NULL != p){
        uint32_t len = g_config.page_size;
        char* buf = p->buffer;
        while(len >= s->fixed_block_size){
            block* b = (block*)buf;
            if(b->flags == BLOCK_FLAGS_SLAB){
                //LOG_I("get a free block\n");
                (*b_free) ++;
            }else if(b->flags & BLOCK_FLAGS_HAVE_CHILD){
                //LOG_I("get a father block\n");
                (*b_father) ++;
            }else if(b->flags & BLOCK_FLAGS_CHILD){
                //LOG_I("get a child block\n");
                (*b_child) ++;
            }else{
                //LOG_I("get a normal block\n");
                (*b_normal) ++;
            }
            len -= s->fixed_block_size;
            buf += s->fixed_block_size;
            
        }
        p = p->next;
    }
}

char* slab_stats(uint64_t* r_total){
    int i = 0;
    uint64_t total = 0;
    uint64_t total_in_lru = 0;
    uint64_t total_in_slab = 0;
    uint64_t lru_cold = 0;
    uint64_t lru_warm = 0;
    uint64_t lru_hot = 0;
    uint64_t recycled = 0;
    int64_t waste = 0;
    uint64_t total_waste = 0;
    char* title = NULL;
    int total_len = 0;
    asprintf(&title, "%-4s %-8s %-8s %-8s %-8s %-8s %-8s %-10s %-8s %-8s %-8s %-8s %-8s %-8s %-10s %-10s\n",\
        "slab",\
        "f_size","f_count",\
        "b_free","b_normal","b_father","b_child","b_count","p_count",\
        "lru","hot", "warm", "cold",\
        "recycled",\
        "r_used(K)", "wasted(K)");
    total_len += 2 * strlen(title);
    char** info_slab = calloc(1, g_config.slab_max_id * sizeof(char*));
    
    for(; i < g_config.slab_max_id; ++i){
        uint64_t b_normal = 0;
        uint64_t b_father = 0;
        uint64_t b_child = 0;
        uint64_t b_free = 0;
        SLAB_LOCK(&list[i]);
        total_in_slab = list[i].fixed_block_count * list[i].page_count - list[i].block_count_free;
        SLAB_STATS_LOCK(&list[i]);
        lru_cold = list[i].stats.lru_cold;
        lru_warm = list[i].stats.lru_warm;
        lru_hot = list[i].stats.lru_hot;
        recycled = list[i].stats.count_recycled;
        SLAB_STATS_UNLOCK(&list[i]);
        total_in_lru = lru_cold + lru_warm + lru_hot;
        
        //assert(total_in_lru == total_in_slab);
        total += total_in_lru;
        waste = ((uint64_t)list[i].page_count * g_config.page_size) - ((uint64_t)list[i].block_count_free * list[i].fixed_block_size) - list[i].real_mem_used;
        assert(waste >= 0);
        total_waste += waste;
        slab_stats_page(&list[i], &b_normal, &b_father, &b_child, &b_free);
        assert(b_free == list[i].block_count_free);
        assert((b_normal + b_father + b_child) == total_in_slab); 
        asprintf(&(info_slab[i]), "%-4d %-8d %-8d %-8d %-8lu %-8lu %-8lu %-10lu %-8d %-8lu %-8lu %-8lu %-8lu %-8lu %-10lu %-10ld\n",\
              i,\
              list[i].fixed_block_size, list[i].fixed_block_count,\
              list[i].block_count_free, b_normal, b_father, b_child, total_in_slab,list[i].page_count,\
              total_in_lru, lru_hot,lru_warm,lru_cold,\
              recycled,\
              list[i].real_mem_used/1024, waste/1024);
        total_len += strlen(info_slab[i]);
        SLAB_UNLOCK(&list[i]);
    }
    
    char* summary = NULL;
    asprintf(&summary, "total_waste = %luM\n",total_waste/1024/1024);
    total_len += strlen(summary);
    char* content_all = malloc(total_len + 1);
    content_all = strcpy(content_all, title);
    i = 0;
    for(; i < g_config.slab_max_id; ++i){
        content_all = strcat(content_all, info_slab[i]);
    }
    content_all = strcat(content_all, title);
    content_all = strcat(content_all, summary);
    free(info_slab);
    *r_total = total;
    return content_all;
}


static int slab_page_free_block(slab*s, page* old_p){
    assert(NULL != old_p);
    uint32_t i = 0;
    char* old_buf = old_p->buffer;
    int count = 0;
    while(i < s->fixed_block_count){
        block* b = (block*)(old_buf);
        if(b->flags & BLOCK_FLAGS_SLAB){
            count ++;
        }
        old_buf += s->fixed_block_size;
        ++ i;
            
    }
    return count;
}
static void slab_fill_page(slab*s, char** old_buf, int* res_old, char** new_buf, int* res_new){
    assert(NULL != s);
    assert(NULL != (*old_buf) && NULL != (*new_buf));
    assert((*res_old) > 0);
    assert((*res_new) > 0);
    int j = s->fixed_block_size;
    while(((*res_old) > 0) && ((*res_new) > 0)){
        block* b = (block*)(*old_buf);
        block* b_new = (block*)(*new_buf);
        if(b->flags & BLOCK_FLAGS_SLAB){
            if(b->prev){
              b->prev->next = b->next;
            }
            if(b->next){
              b->next->prev = b->prev;
            }
            if(s->block_list_free == b){
                s->block_list_free = b->next;
            }
            s->block_count_free --;
            b->next = b->prev = NULL;
            
            (*old_buf) += s->fixed_block_size;
            (*res_old) --;
        }else if(b->flags & BLOCK_FLAGS_CHILD){
            uint32_t hv = 0;
            if(!bklist_try_lock(b->l_next->nkey,BLOCK_k(b->l_next),&hv)){
                LOG_B_W(b->l_next,"bklist_try_lock failed!\n");
                break;
            }
            if(0 < b->l_next->nref){
                LOG_B_W(b->l_next,"is busy in get [%d]\n",b->l_next->nref);
                bklist_unlock(hv);
                break;
            }
            memcpy(b_new,b,j);
            block_equal(b_new,b);
            assert(NULL != b->l_next);
            
            if(NULL == b->prev){
                memcpy(BLOCK_c(b->l_next),&b_new,sizeof(block*));
            }
            if(b->prev){
                b->prev->next = b_new;
            }
            if(b->next){
                b->next->prev = b_new;
            } 
            assert(NULL != b_new->l_next);
            assert(0 < b_new->l_next->nkey);
            bklist_unlock(hv);
            (*new_buf) += s->fixed_block_size;
            (*old_buf) += s->fixed_block_size;
            (*res_old) --;
            (*res_new) --;
            
        }else{
            uint32_t hv = 0;
            if(!bklist_try_lock(b->nkey,BLOCK_k(b),&hv)){
                LOG_B_W(b,"bklist_try_lock failed!\n");
                break;
            }
            if(0 < b->nref){
                LOG_B_W(b,"is busy in get [%d]\n",b->nref);
                bklist_unlock(hv);
                break;
            }
            int slab_id = b->slab_id;
            if(!lru_try_lock_mutex(slab_id)){
                LOG_W("lru_try_lock_mutex LRU[%d] failed!\n",slab_id);
                bklist_unlock(hv);
                break;
            }
            memcpy(b_new,b,j);
            block_equal(b_new,b);
            if(b->flags & BLOCK_FLAGS_HAVE_CHILD){
                block** child_ptr = (block**)BLOCK_c(b);
                block* child = *child_ptr;
                assert(NULL != child);
                while(NULL != child){
                    child->l_next = b_new;
                    child = child->next;
                }
            }
            if(0 != bklist_replace_no_lock(b,b_new,hv)){
                LOG_B_W(b,"bklist_replace_no_lock error\n");
            }
            
            lru_replace_onlock(b,b_new);
            lru_unlock_mutex(slab_id);
            bklist_unlock(hv);
            (*new_buf) += s->fixed_block_size;
            (*old_buf) += s->fixed_block_size;
            (*res_old) --;
            (*res_new) --;
        } 
   }

  
}


void slab_recycle(slab* s){
    assert(NULL != s);
    SLAB_LOCK(s);
    LOG_I("slab[%d] block_count_free = %d, fixed_block_count = %d\n",s->id,s->block_count_free, s->fixed_block_count);
    if(s->block_count_free < s->fixed_block_count){
        LOG_W("no need to handle slab[%d] block_count_free[%u] < fixed_block_count[%u]\n",s->id,s->block_count_free,s->fixed_block_count);
        SLAB_UNLOCK(s);
        return;
    }
    int count_p = s->page_count;
    page* old_p = s->page_list;
    if(NULL == old_p){
        LOG_W("empty slab[%d]\n",s->id);
        SLAB_UNLOCK(s);
        return;
    }
    page* new_p = page_alloc();
    page* new_next_p = NULL;
    if(NULL == new_p){
        LOG_W("no free page to do page recycle!\n");
        SLAB_UNLOCK(s);
        return;
    }
    int b_free_pages = get_free_page_count();
    int b_slab_pages = s->page_count;
    int b_slab_block = s->block_count_free;
    int res_old = s->fixed_block_count;
    int res_new = s->fixed_block_count;
    char* old_buf = (char*)(old_p->buffer);
    char* new_buf = (char*)(new_p->buffer);
    LOG_I("real go!\n");
    while(res_new > 0 && count_p > 0 && NULL != new_p && NULL != old_p){
 #if 1
        if(res_old == (int)s->fixed_block_count){
            int free_blocks = slab_page_free_block(s,old_p);
            if(0 == free_blocks){
                old_p = old_p->next;
                if(NULL == old_p){
                   if(new_next_p) page_free(new_next_p);
                   if(res_new < (int)s->fixed_block_count){
                       LOG_I("add the res_new to freelist!\n");
                       goto stop;
                   }else{
                       LOG_I("just free the empty new_p\n");
                       page_free(new_p);
                       break;
                       
                   }
                   
                }
                old_buf = (char*)(old_p->buffer);
                count_p --;
                LOG_W("page[%d] in slab[%d] is full, no need to recycle!\n", count_p, s->id );
                continue;
            }else if((int)s->fixed_block_count == free_blocks){
                while(0 < free_blocks){
                    block* b = (block*)(old_buf);
                    assert(b->flags & BLOCK_FLAGS_SLAB);
                    if(b->prev){
                      b->prev->next = b->next;
                    }
                    if(b->next){
                      b->next->prev = b->prev;
                    }
                    if(s->block_list_free == b){
                        s->block_list_free = b->next;
                    }
                    s->block_count_free --;
                    b->next = b->prev = NULL;
                    old_buf += s->fixed_block_size;
                    free_blocks --;
                    
                }
                if(old_p->prev){
                    old_p->prev->next = old_p->next;
                }
                if(old_p->next){
                    old_p->next->prev = old_p->prev;
                }
                if(s->page_list == old_p){
                    s->page_list = old_p->next;
                }
                s->page_count --;
                page* new_old_p = old_p->next;
                page_free(old_p);
                if(NULL == new_old_p){
                    if(new_next_p) page_free(new_next_p);
                    if(res_new < (int)s->fixed_block_count){
                       LOG_I("add the res_new to freelist!\n");
                       goto stop;
                   }else{
                       LOG_I("just free the empty new_p\n");
                       page_free(new_p);
                       break;
                   }
                }
                old_p = new_old_p;
                old_buf = (char*)(old_p->buffer);
                count_p --;
                LOG_W("page[%d] in slab[%d] is empty, just recycle!\n", count_p, s->id );
                continue;
            }
                
        }
#endif
        slab_fill_page(s, &old_buf, &res_old, &new_buf, &res_new);
        if(0 == res_old){
            if(0 == res_new){
                LOG_W("bad action at page[%d] in slab[%d] !\n", count_p, s->id );
            }
            if(old_p->prev){
                old_p->prev->next = old_p->next;
            }
            if(old_p->next){
                old_p->next->prev = old_p->prev;
            }
            if(s->page_list == old_p){
                s->page_list = old_p->next;
            }
            s->page_count --;
            page* next = old_p->next;
            page_free(old_p);
            if(NULL != next){
                if(NULL == new_next_p){
                    new_next_p = page_alloc();
                }else{
                    LOG_I("add new_next_p is not used before\n");
                }
            }
            if((NULL == new_next_p) || (NULL == next)){
stop:
                while(res_new > 0){
                   block* free_b = (block*)new_buf;
                   memset(free_b,0,s->fixed_block_size);
                   free_b->next = s->block_list_free;
                   if(free_b->next){
                        free_b->next->prev = free_b;
                   }
                   free_b->flags = BLOCK_FLAGS_SLAB;
                   free_b->slab_id = s->id;
                   s->block_list_free = free_b;
                   s->block_count_free ++;
                   new_buf += s->fixed_block_size;
                   res_new --; 
                }
                new_p->next = s->page_list;
                if(new_p->next){
                    new_p->next->prev = new_p;
                }
                new_p->prev = NULL;
                s->page_list = new_p;
                s->page_count ++;
                break;
            }
            old_p = next;
            old_buf = (char*)(old_p->buffer);
            res_old = s->fixed_block_count;
            count_p --;
            
        }
        if(0 == res_new){
            new_p->next = s->page_list;
            if(new_p->next){
                new_p->next->prev = new_p;
            }
            new_p->prev = NULL;
            s->page_list = new_p;
            s->page_count ++;
            new_p = new_next_p;
            new_next_p = NULL;
            assert(NULL != new_p);
            res_new = s->fixed_block_count;
            new_buf = (char*)(new_p->buffer);
        }
        
    }
    int a_free_pages = get_free_page_count();
    int a_slab_pages = s->page_count;
    int a_slab_block = s->block_count_free;
    if(0 != (a_free_pages - b_free_pages)) LOG_I("a_free = %d b_free = %d, new_free = %d  at slab[%d][%d %d][%d %d]\n",a_free_pages,b_free_pages, a_free_pages - b_free_pages,s->id,b_slab_pages, a_slab_pages,b_slab_block,a_slab_block);
    SLAB_UNLOCK(s); 
}



