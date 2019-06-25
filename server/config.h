/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_CONFIG_H_
#define _X_CONFIG_H_

#include <sys/types.h>

#include <stdint.h>


//#define TEST_CASE_SMALL_MEM
#ifdef TEST_CASE_SMALL_MEM
#define MEM_SIZE (1024LL * 1024  * 64)
#define MEM_MAX_SIZE (1024LL * 1024  * 256)
#define MEM_SIZE_EXPAND (1024)
#define PAGE_SIZE  (1 * 1024 * 1024)
#else
#define MEM_SIZE (1024LL * 1024 * 1024 * 50)
#define MEM_MAX_SIZE (2LL * MEM_SIZE)
#define MEM_SIZE_EXPAND (800)
#define PAGE_SIZE  (1 * 1024 * 1024)
#endif
#define MEM_SIZE_EXPAND_AUTO (1)

#define BLOCK_SIZE_ALIGN (8)
#define SLAB_MIN_SIZE 48
#define SLAB_FACTOR (1.25)



#define SLAB_MAX_ID (1 << 6)                                            // | 0 0 x x x x x x|




#define LRU_HOT_ID(slab_id) (slab_id &((1 << 6) -1))                   // | 0 0 x x x x x x|
#define LRU_WARM_ID(slab_id) LRU_HOT_ID(slab_id) | (1 << 6)            // | 0 1 x x x x x x|
#define LRU_COLD_ID(slab_id) LRU_HOT_ID(slab_id) | (1 << 7)             // | 1 0 x x x x x x|


#define LRU_COLD_LOCK(slab_id) pthread_mutex_lock(&lru_lock[LRU_COLD_ID(slab_id)])
#define LRU_COLD_UNLOCK(slab_id) pthread_mutex_unlock(&lru_lock[LRU_COLD_ID(slab_id)])

#define LRU_WARM_LOCK(slab_id) pthread_mutex_lock(&lru_lock[LRU_WARM_ID(slab_id)])
#define LRU_WARM_UNLOCK(slab_id) pthread_mutex_unlock(&lru_lock[LRU_WARM_ID(slab_id)])

#define LRU_HOT_LOCK(slab_id) pthread_mutex_lock(&lru_lock[LRU_HOT_ID(slab_id)])
#define LRU_HOT_UNLOCK(slab_id) pthread_mutex_unlock(&lru_lock[LRU_HOT_ID(slab_id)])





#define LRU_MAX_ID (1 << 8)                                             // | x x x x x x x x|
#define SLAB_ID(lru_id) (lru_id &((1 << 6) -1))                        // | 0 0 x x x x x x|

#define IN_LRU_WARM(lru_id) (lru_id & (1 << 6))
#define IN_LRU_COLD(lru_id)  (lru_id & (1 << 7))




#define GET_WARM_BIT(v) ((v) >> 6)
#define GET_COLD_BIT(v) ((v) >> 7)



#define LRU_LOCK(lru_id) pthread_mutex_lock(&lru_lock[lru_id])
#define LRU_TRY_LOCK(lru_id) pthread_mutex_trylock(&lru_lock[lru_id])

#define LRU_UNLOCK(lru_id) pthread_mutex_unlock(&lru_lock[lru_id])





//#define test_case_hash


//#define TEST_CASE_MIN_DATA
//#define TEST_CASE_MIN_IOV_MAX
//#define TEST_CASE_MIN_MSG
//#define TEST_CASE_MIN_IOV




#ifdef TEST_CASE_MIN_DATA
#define DATA_BUFFER_SIZE     8
#define DATA_BUFFER_MAX     (8 * 4)
#else
#define DATA_BUFFER_SIZE     2048
#define DATA_BUFFER_MAX     (2048 * 2)
#endif


#ifdef TEST_CASE_MIN_IOV
#define DATA_IOV_SIZE        1
#define DATA_IOV_MAX        (1 * 4)
#else
#define DATA_IOV_SIZE        20
#define DATA_IOV_MAX        (20 * 4)
#endif

#ifdef TEST_CASE_MIN_MSG
#define DATA_MSG_SIZE        1
#define DATA_MSG_MAX        (1 * 2)
#else
#define DATA_MSG_SIZE        20
#define DATA_MSG_MAX        (20 * 4)
#endif


#ifndef IOV_MAX
#ifdef TEST_CASE_MIN_IOV_MAX
#define IOV_MAX 1
#else
#define IOV_MAX 1024
#endif
#endif



#define BKLIST_POWER  16
#define BKLIST_MAX_ID ((uint32_t)1 << g_config.bklist_power)        
#define BKLIST_ID(hv) ((hv) & (BKLIST_MAX_ID - 1))


#define BKLIST_LOCK_POWER  15
#define BKLIST_LOCK_MAX_ID ((uint32_t)1 << g_config.bklist_lock_power)        
#define BKLIST_LOCK_ID(hv) ((hv) & (BKLIST_LOCK_MAX_ID - 1))


#define BK_LIST_LOCK(hv) pthread_mutex_lock(&bklist_mutex[BKLIST_LOCK_ID(hv)])
#define BK_LIST_TRY_LOCK(hv) pthread_mutex_trylock(&bklist_mutex[BKLIST_LOCK_ID(hv)])
#define BK_LIST_UNLOCK(hv) pthread_mutex_unlock(&bklist_mutex[BKLIST_LOCK_ID(hv)])

#define SLAB_STATS_LOCK(s) pthread_mutex_lock(&((s)->stats.lock_stats))
#define SLAB_STATS_UNLOCK(s) pthread_mutex_unlock(&((s)->stats.lock_stats))


#define PAGE_LOCK pthread_mutex_lock(&lock)
#define PAGE_UNLOCK pthread_mutex_unlock(&lock)


#define SLAB_LOCK(s) pthread_mutex_lock(&((s)->lock))
#define SLAB_UNLOCK(s) pthread_mutex_unlock(&((s)->lock))




#define STATS_LOCK pthread_mutex_lock(&g_stats.lock)
#define STATS_UNLOCK pthread_mutex_unlock(&g_stats.lock)


#define BACKUP_LOCK pthread_mutex_lock(&lock)
#define BACKUP_UNLOCK pthread_mutex_unlock(&lock)

#define NET_LOCK pthread_mutex_lock(&net_mutex)
#define NET_UNLOCK pthread_mutex_unlock(&net_mutex)


#define CONFIG_LOCK pthread_mutex_lock(&lock)
#define CONFIG_UNLOCK pthread_mutex_unlock(&lock)



#define BLOCK_MAX_TIME_NO_ACTIVITY 5 * 24 * 3600   //five days
#define BLOCK_MIN_TIME_NO_ACTIVITY 2 * 24 * 3600   //two days
#define WORKER_RECYCLE_INTERVAL  2


#define NET_BACKLOG 1024
#define CON_MAX_ID  1024





typedef struct _config{
    char* version;
    char* start_time;
    uint64_t mem_max_size;
    uint64_t mem_size;
    uint32_t mem_size_expand;
    uint32_t page_prealloc;
    uint32_t page_size;
    uint32_t block_size_align;
    uint32_t slab_max_size;
    uint32_t slab_min_size;
    uint8_t slab_max_id;
    uint8_t mem_auto_expand;
    uint32_t bklist_power;
    uint32_t bklist_lock_power;
    uint32_t block_max_time_no_activity;
    uint32_t block_min_time_no_activity;
    uint32_t block_each_recycled_count;
    uint32_t block_recycled_interval;
    uint32_t worker_recycle_interval;
    uint32_t lru_switcher;
    uint32_t con_max_id;
    uint32_t data_buffer_size;
    uint32_t data_buffer_max;
    uint32_t data_iov_size;
    uint32_t data_iov_max;
    uint32_t data_msg_size;
    uint32_t data_msg_max;
    int port;
    int enable_backup;
    int net_backlog;
    int daemonize;
    int enable_coredump;
    int enable_slab_recycle;
    int enable_lru_balance;
    int enable_worker_recycle;
    double slab_factor;
}config;



void get_lru_parameter(uint32_t* min, uint32_t* max, uint32_t* count, uint32_t* interval, uint32_t* switcher);
void set_lru_parameter(uint32_t min, uint32_t max, uint32_t count, uint32_t interval, uint32_t switcher);
void set_memory_parameter(uint8_t mem_auto_expand, uint64_t mem_size, uint64_t mem_max_size);
void get_memory_parameter(uint8_t* mem_auto_expand, uint64_t* mem_size, uint64_t* mem_max_size);
int config_init(int argc, char**argv);
char* config_stats();



#endif
