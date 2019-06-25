/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "stats.h"
#include "log.h"
#include "bklist.h"
#include "slab.h"
#include "page.h"
#include "backup.h"
#include "config.h"
#include <string.h>
#include <assert.h>


stats g_stats;
extern config g_config;


char* stats_show(){
    uint64_t total_in_bklist = 0;
    char* data_bklist = bklist_stats(&total_in_bklist);
    
    uint64_t total_in_slab = 0;
    char* data_slab = slab_stats(&total_in_slab);

    char* data_config = config_stats();
    char* data_page = page_stats();

    
    
    char* summary_1 = NULL;
    asprintf(&summary_1, "blocks_in_slab = %-10lu\n" "blocks_in_bklist = %-15lu\n" "blocks_in_backup = %-15lu\n",total_in_slab, total_in_bklist, backup_count());


    char* summary_2 = NULL;
    stats_lock();
    asprintf(&summary_2, "set_ok = %-10lu" "set_failed = %-15lu\n""get_ok = %-10lu" "get_failed = %-15lu\n""del_ok = %-10lu" "del_failed = %-15lu\n""fd_cleared = %-15lu\n""max_cons = %-15lu\n""op_lru = %-15lu\n" "%s\nComplied at: %s %s\nStarted at: %s\n",g_stats.set_ok,g_stats.set_failed,g_stats.get_ok,g_stats.get_failed,g_stats.del_ok,g_stats.del_failed,g_stats.fd_cleared,g_stats.max_cons,g_stats.op_lru,g_config.version,__DATE__,__TIME__,g_config.start_time);
    stats_unlock();


    char* total_content = malloc(strlen(data_bklist) + strlen(data_slab) + strlen(data_config) + strlen(data_page) + strlen(summary_1) + strlen(summary_2) + 1);
    total_content[0] = '\0';
    total_content = strcat(total_content,data_bklist);
    total_content = strcat(total_content,data_slab);
    total_content = strcat(total_content,data_config);
    total_content = strcat(total_content,data_page);
    total_content = strcat(total_content,summary_1);
    total_content = strcat(total_content,summary_2);
    
    free(data_bklist);
    free(data_slab);
    free(data_config);
    free(data_page);
    free(summary_1);
    free(summary_2);
    printf("%s",total_content);
    return total_content;
}



void stats_init(){
    memset(&g_stats,0,sizeof(stats));
    pthread_mutex_init(&(g_stats.lock),NULL);
    pthread_cond_init(&(g_stats.cond), NULL);
       
}

void stats_wait(){
    STATS_LOCK;
    LOG_I("thread_run = %ld\n",g_stats.thread_run);
    while(0 < g_stats.thread_run){
        LOG_I("thread_run = %ld and pthread_cond_wait\n",g_stats.thread_run);
        pthread_cond_wait(&g_stats.cond, &g_stats.lock);
    }
    STATS_UNLOCK;

}
void stats_signal(){
    STATS_LOCK;
    g_stats.thread_run --;
    assert(g_stats.thread_run >= 0);
    LOG_I("thread_run = %ld\n",g_stats.thread_run);
    if(0 == g_stats.thread_run){
      LOG_I("thread_run = %ld and pthread_cond_signal\n",g_stats.thread_run);
      pthread_cond_signal(&g_stats.cond);  
    }
    STATS_UNLOCK;
}

void stats_lock(){
    STATS_LOCK;
}
void stats_unlock(){
    STATS_UNLOCK;
}



