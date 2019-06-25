/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "page.h"


#include "log.h"
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>


static page* pglist = NULL;
static size_t mem_total_malloced = 0;
static size_t mem_total_used = 0;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int free_page = 0;


extern config g_config;
void page_init(){
    int count = g_config.mem_size/ g_config.page_size;
    if(g_config.page_prealloc){
        while(count > 0){
            page* p = malloc(sizeof(page));
            p->buffer = malloc(g_config.page_size);
            assert(NULL != p->buffer);
            PAGE_LOCK;
            p->next = pglist;
            if(p->next) {
                p->next->prev = p;
            }
            pglist = p;
            mem_total_malloced += g_config.page_size;
            free_page ++;
            PAGE_UNLOCK;
            -- count;
        }
    }else{
        free_page = count;
    }
}

page* page_alloc(){
    page* p = NULL;
    PAGE_LOCK;
    if(NULL != pglist){
        p = pglist;
        pglist = p->next;
        if(p->next){
            p->next->prev = NULL;
        }
        p->next = NULL;
        p->prev = NULL;
        free_page --;
        mem_total_used += g_config.page_size;
        assert(free_page >= 0);
        PAGE_UNLOCK;
        return p;
    }
    if(mem_total_malloced + g_config.page_size > g_config.mem_size){
        LOG_E("the mem must be limited to %lu, now is %lu\n",g_config.mem_size,mem_total_malloced);
        PAGE_UNLOCK;
        return NULL;
    }
    
    p = malloc(sizeof(page));
    if(NULL == p){
        LOG_E("no free memory for sizeof(page)!\n");
        free(p);
        PAGE_UNLOCK;
        return NULL;
    }
    p->buffer = malloc(g_config.page_size);
    if(NULL == p->buffer){
        LOG_E("no free memory for g_config.page_size!\n");
        PAGE_UNLOCK;
        return NULL;
    }
    free_page --;
    mem_total_malloced += g_config.page_size;
    mem_total_used += g_config.page_size;
    p->next = NULL;
    p->prev = NULL;
    PAGE_UNLOCK;
    return p;      
        
}
int page_expand(size_t size){
    PAGE_LOCK;
    size *= 1024 * 1024;
    size += (g_config.page_size - size % g_config.page_size);
    if((g_config.mem_size + size) > g_config.mem_max_size){
        LOG_E("the mem must be limited to %lu, now max is %lu\n",g_config.mem_max_size,g_config.mem_size);
        PAGE_UNLOCK;
        return -1;
    }
    g_config.mem_size += size;
    free_page += size /g_config.page_size;
    LOG_I("the max men_size is from %lu to %lu, free_page is from %ld to %d\n",g_config.mem_size - size, g_config.mem_size,free_page - size /g_config.page_size,free_page);
    PAGE_UNLOCK;
    return 0;
        
}
void page_free(page* p){
    assert(NULL != p);
    PAGE_LOCK;
    p->next = pglist;
    if(p->next) {
        p->next->prev = p;
    }
    pglist = p;
    p->prev = NULL;
    free_page ++;
    mem_total_used -= g_config.page_size;
    LOG_I("free_page = %d\n",free_page);
    PAGE_UNLOCK;
}

int get_free_page_count(){
    int free = 0;
    PAGE_LOCK;
    free = free_page;
    PAGE_UNLOCK;
    return free;
}

char* page_stats(){
    char* content = NULL;
    PAGE_LOCK;
    if(mem_total_malloced >= 1024LL * 1024 * 1024){
        if(mem_total_used >= 1024LL * 1024 * 1024){
            asprintf(&content,"mem_total_malloced = %luG\nmem_total_used = %luG\nfree_page = %d\n", mem_total_malloced/1024/1024/1024, mem_total_used/1024/1024/1024, free_page);
        }else{
            asprintf(&content,"mem_total_malloced = %luG\nmem_total_used = %luM\nfree_page = %d\n", mem_total_malloced/1024/1024/1024,mem_total_used/1024/1024, free_page);
        }
    }else{
      asprintf(&content,"mem_total_malloced = %luM\nmem_total_used = %luM\nfree_page = %d\n", mem_total_malloced/1024/1024,mem_total_used/1024/1024, free_page);  
    }
    PAGE_UNLOCK;
    return content;
}
