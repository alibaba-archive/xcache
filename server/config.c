/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "config.h"
#include "protocol.h"
#include "log.h"
#include "mem.h"

#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>




static pthread_mutex_t lock;
config g_config;


void set_memory_parameter(uint8_t mem_auto_expand, uint64_t mem_size __attribute__((unused)), uint64_t mem_max_size __attribute__((unused))){
    CONFIG_LOCK;
    g_config.mem_auto_expand = mem_auto_expand;
    CONFIG_UNLOCK;
    
}
void get_memory_parameter(uint8_t* mem_auto_expand, uint64_t* mem_size __attribute__((unused)), uint64_t* mem_max_size __attribute__((unused))){
    CONFIG_LOCK;
    if(NULL != mem_auto_expand) *mem_auto_expand = g_config.mem_auto_expand;
    CONFIG_UNLOCK;   
}


void get_lru_parameter(uint32_t* min, uint32_t* max, uint32_t* count, uint32_t* interval, uint32_t* switcher){
    CONFIG_LOCK;
    if(NULL != min) *min = g_config.block_min_time_no_activity;
    if(NULL != max) *max = g_config.block_max_time_no_activity;
    if(NULL != count) *count = g_config.block_each_recycled_count;
    if(NULL != interval) *interval = g_config.block_recycled_interval;
    if(NULL != switcher) *switcher = g_config.lru_switcher;
    CONFIG_UNLOCK;
}

void set_lru_parameter(uint32_t min, uint32_t max, uint32_t count, uint32_t interval, uint32_t switcher){
    CONFIG_LOCK;
    g_config.block_max_time_no_activity = max;
    g_config.block_min_time_no_activity = min;
    g_config.block_each_recycled_count = (count == 0? 1:count);
    g_config.lru_switcher = switcher;
    g_config.block_recycled_interval = interval;
    CONFIG_UNLOCK;
    LOG_I("max = %u min = %u count = %u interval = %u,switcher = %u\n",max, min,(count == 0? 1:count),interval,switcher);
}



int config_init(int argc __attribute__((unused)), char**argv __attribute__((unused))){
    g_config.page_prealloc = 1;
    time_t now ;
    struct tm *tm_now;
    time(&now) ;
    tm_now = localtime(&now) ;
    asprintf(&g_config.start_time, "%d-%d-%d %d:%d:%d\n",tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    uint64_t max_mem_pc = getTotalSystemMemory();
    if(0 == g_config.mem_size) g_config.mem_size = MEM_SIZE;
    if(0 == g_config.mem_max_size) g_config.mem_max_size = max_mem_pc;
    if(0 == g_config.mem_size_expand) g_config.mem_size_expand = 1;
    if(0 == g_config.port) g_config.port = 20190;
    if(0 == g_config.enable_backup) g_config.enable_backup = 1;
    if(0 == g_config.enable_lru_balance) g_config.enable_lru_balance = 1;
    if(0 == g_config.enable_worker_recycle) g_config.enable_worker_recycle = 1;
    if(0 == g_config.worker_recycle_interval) g_config.worker_recycle_interval = WORKER_RECYCLE_INTERVAL;
    
    
    
    g_config.page_size = PAGE_SIZE;
    g_config.block_size_align = BLOCK_SIZE_ALIGN;
    g_config.slab_max_size = g_config.page_size;
    g_config.slab_min_size = SLAB_MIN_SIZE;
    g_config.slab_max_id = SLAB_MAX_ID;
    g_config.slab_factor = SLAB_FACTOR;
    g_config.bklist_power = BKLIST_POWER;
    g_config.bklist_lock_power = BKLIST_LOCK_POWER;
    CONFIG_LOCK;
    if(0 == g_config.block_max_time_no_activity) g_config.block_max_time_no_activity = BLOCK_MAX_TIME_NO_ACTIVITY;
    if(0 == g_config.block_min_time_no_activity) g_config.block_min_time_no_activity = BLOCK_MIN_TIME_NO_ACTIVITY;
    if(0 == g_config.block_each_recycled_count) g_config.block_each_recycled_count = 1;
    if(0 == g_config.block_recycled_interval) g_config.block_recycled_interval = 60;
    if(0 == g_config.mem_auto_expand) g_config.mem_auto_expand = 1;
    CONFIG_UNLOCK;
    if(0 == g_config.enable_lru_balance) g_config.enable_lru_balance = 1;
    g_config.net_backlog = NET_BACKLOG;
    g_config.con_max_id = CON_MAX_ID;
    g_config.data_buffer_size = DATA_BUFFER_SIZE;
    g_config.data_buffer_max = DATA_BUFFER_MAX;
    g_config.data_iov_size = DATA_IOV_SIZE;
    g_config.data_msg_size = DATA_MSG_SIZE;
    g_config.data_iov_max = DATA_IOV_MAX;
    g_config.data_msg_max = DATA_MSG_MAX;

    
    
    g_config.mem_max_size = g_config.page_size * (g_config.mem_max_size / g_config.page_size);
    assert(0 == g_config.mem_max_size % g_config.page_size);
    assert(g_config.mem_max_size >= g_config.mem_size);
    assert(g_config.mem_max_size < max_mem_pc);
    assert(0 == g_config.mem_size % g_config.page_size);
    assert(0 == (g_config.mem_size_expand * 1024 * 1024) % g_config.page_size);
    
    assert(g_config.mem_size >=  (g_config.page_size * g_config.slab_max_id));

    
    assert(g_config.slab_max_size <= g_config.page_size);
    assert(0 == g_config.slab_min_size % g_config.block_size_align);
    assert(g_config.slab_max_id > 0 && g_config.slab_max_id <= SLAB_MAX_ID);
    assert(g_config.bklist_power >= 16 && g_config.bklist_power < 64);
    assert(g_config.bklist_lock_power >= 8 && g_config.bklist_lock_power <= g_config.bklist_power);
    assert(g_config.block_max_time_no_activity > g_config.block_min_time_no_activity);
    assert(g_config.data_buffer_size >= sizeof(protocol_binary_request_header));
    assert(0 == g_config.data_buffer_size % 8);
    assert(g_config.data_buffer_max >= 2 * g_config.data_buffer_size && 0 == g_config.data_buffer_max % g_config.data_buffer_size);
    assert((2 * g_config.data_iov_size) <= g_config.data_iov_max && (2 * g_config.data_msg_size) <= g_config.data_msg_max);
    return 0;
}

char* config_stats(){
    char* content = NULL;
    uint32_t min = 0;
    uint32_t max = 0;
    uint32_t recycled_count_each_time = 0;
    uint32_t interval = 0;
    uint32_t switcher = 0;
    uint8_t mem_auto_expand = 0;
    get_lru_parameter(&min,&max,&recycled_count_each_time,&interval,&switcher);
    get_memory_parameter(&mem_auto_expand,NULL,NULL);
    
    if(g_config.mem_size >= 1024LL * 1024 * 1024){
        if(min >= (3600 * 24)){
            asprintf(&content,"mem_size_set = %luG\nmem_size_max_can_be = %luG\nmem_auto_expand = %d\n[0<hot<=%uD] [%uD<warm<%uD] [%uD<=cold] [recycled_count_each_time=%u] [interval=%u%s] [switcher=%u]\n",g_config.mem_size/1024/1024/1024, g_config.mem_max_size/1024/1024/1024,mem_auto_expand,min/3600/24,min/3600/24,max/3600/24,max/3600/24,recycled_count_each_time,interval>(3600 * 24)?interval/3600/24:interval,interval>(3600 * 24)?"D":"S",switcher);
        }else{
            asprintf(&content,"mem_size_set = %luG\nmem_size_max_can_be = %luG\nmem_auto_expand = %d\n[0<hot<=%uS] [%uS<warm<%uS] [%uS<=cold] [recycled_count_each_time=%u] [interval=%u%s] [switcher=%u]\n",g_config.mem_size/1024/1024/1024, g_config.mem_max_size/1024/1024/1024,mem_auto_expand,min,min,max,max,recycled_count_each_time,interval>(3600 * 24)?interval/3600/24:interval,interval>(3600 * 24)?"D":"S",switcher);
        }
    }else{
        if(min >= (3600 * 24)){
            asprintf(&content,"mem_size_set = %luM\nmem_size_max_can_be = %luM\nmem_auto_expand = %d\n[0<hot<=%uD] [%uD<warm<%uD] [%uS<=cold] [recycled_count_each_time=%u] [interval=%u%s] [switcher=%u]\n",g_config.mem_size/1024/1024, g_config.mem_max_size/1024/1024,mem_auto_expand,min/3600/24,min/3600/24,max/3600/24,max/3600/24,recycled_count_each_time,interval>(3600 * 24)?interval/3600/24:interval,interval>(3600 * 24)?"D":"S",switcher);

        }else{
            asprintf(&content,"mem_size_set = %luM\nmem_size_max_can_be = %luM\nmem_auto_expand = %d\n[0<hot<=%uS] [%uS<warm<%uS] [%uS<=cold] [recycled_count_each_time=%u] [interval=%u%s] [switcher=%u]\n",g_config.mem_size/1024/1024, g_config.mem_max_size/1024/1024,mem_auto_expand,min,min,max,max,recycled_count_each_time,interval>(3600 * 24)?interval/3600/24:interval,interval>(3600 * 24)?"D":"S",switcher);
        }
    }
    return content;
}

