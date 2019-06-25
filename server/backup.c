/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/


#include "backup.h"
#include "block.h"
#include "log.h"
#include "config.h"

#include <arpa/inet.h>

#include <string.h>
#include <assert.h>
#define _BSD_SOURCE             /* See feature_test_macros(7) */
#include <endian.h>


static size_t backup_size_default = (sizeof(uint16_t) + 32) * 30000;
static size_t backup_size;
static char* backup = NULL;
static size_t backup_len = 0;
static uint64_t backup_total = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


//    [block_total][key_len_1][key_1][key_len_1][key_1]
//        64bit      16bit              16bit

#ifdef BACKUP_DEBUG
static void backup_debug(char* backup, size_t len){
    assert(NULL != backup && 0 < len);
    size_t size = sizeof(uint16_t);
    while(len > 0){
        uint16_t key_len = *((uint16_t*)(backup));
        key_len = ntohs(key_len);
        char key[key_len + 1];
        memcpy(key, backup + size, key_len);
        key[key_len] = '\0';
        len -= (size + key_len);
        backup += (size + key_len);
        LOG_I("nkey = %d, key = %s\n",key_len, key);

    }
}
#endif
void backup_init(){
    backup_size = backup_size_default;
    backup = malloc(backup_size);
    backup_len = sizeof(uint64_t);
    
}
void backup_add(block* b){
    assert(NULL != b);
    BACKUP_LOCK;
    if((backup_len + sizeof(b->nkey) + b->nkey) >= backup_size){
        LOG_I("realloc\n");
        backup = realloc(backup, 2 * backup_size);
        backup_size *= 2;
        assert(NULL != backup);
    }
    *((uint16_t*)(backup + backup_len)) = htons(b->nkey);
    memcpy(backup + backup_len + sizeof(b->nkey), BLOCK_k(b),b->nkey);
    backup_len += sizeof(b->nkey) + b->nkey;
    backup_total ++;
    assert(backup_total > 0);
    *((uint64_t*)(backup)) = htobe64(backup_total);
    
    #ifdef BACKUP_DEBUG
    backup_debug(backup,backup_len);
    #endif
    BACKUP_UNLOCK;
}
void backup_del(block* b){
    uint64_t i = 0;
    int nkey_bag = 0;
    BACKUP_LOCK;
    for(; i < backup_total; ++i){
        uint16_t key_len = *((uint16_t*)(backup + sizeof(uint64_t) + nkey_bag));
        key_len = ntohs(key_len);
        if(key_len == b->nkey){
            char* key_bag = backup + sizeof(uint64_t) + nkey_bag;
            if(0 == memcmp(BLOCK_k(b), key_bag + sizeof(uint16_t), key_len)){
                LOG_I("found the key!\n");
                int n = backup_size - (key_bag + sizeof(uint16_t) + key_len - backup);
                int zero = sizeof(uint16_t) + key_len;
                memmove(key_bag, key_bag + sizeof(uint16_t) + key_len, n);
                memset(backup + backup_size - zero, 0, zero);
                backup_total -= 1;
                backup_len -= sizeof(uint16_t) + key_len;
                *((uint64_t*)(backup)) = htobe64(backup_total);
                break;
           }
        }
        nkey_bag += sizeof(uint16_t) + key_len;
    }
    BACKUP_UNLOCK;
}

void backup_reset_nolock(){
    if(backup_len >= 2 * backup_size_default){
        LOG_I("realloc\n");
        backup = realloc(backup,  backup_size_default); 
        backup_size = backup_size_default;
    }
    backup_len = sizeof(uint64_t);
    backup_total = 0;

}
char* backup_get_nolock(uint64_t* len){
    *len = backup_len;
    return backup;
}

uint64_t backup_count(){
    uint64_t count = 0;
    BACKUP_LOCK;
    count = backup_total;
    BACKUP_UNLOCK;
    return count;
}


void backup_lock(){
    BACKUP_LOCK;
}
void backup_unlock(){
    BACKUP_UNLOCK;
}

