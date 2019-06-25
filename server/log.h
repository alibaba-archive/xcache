/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_LOG_H_
#define _X_LOG_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>



#define NONE                 "\e[0m"
#define RED                  "\e[0;31m"






#if 0
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOG_LOCK  pthread_mutex_lock(&log_lock);
#define LOG_UNLOCK pthread_mutex_unlock(&log_lock);
#else
#define LOG_LOCK
#define LOG_UNLOCK
#endif

#define DEBUG_XCACHE
#ifdef DEBUG_XCACHE
#define LOG_I(f_, ...) \
    do{\
        LOG_LOCK \
        printf("[0x%-10lx]%-11s:%-20s:%-4d  ",(unsigned long)pthread_self(),__FILE__,__FUNCTION__,__LINE__); \
        printf((f_), ##__VA_ARGS__); \
        LOG_UNLOCK \
      } \
      while(0)


#ifndef LOG_CLIENT


#define LOG_B(b,f_, ...) \
            do{\
                char b_key[b->nkey + 1]; \
                memcpy(b_key,BLOCK_k(b),b->nkey); \
                b_key[b->nkey] = 0; \
                LOG_LOCK \
                printf("[0x%-10lx]%-11s:%-20s:%-4d  ",(unsigned long)pthread_self(),__FILE__,__FUNCTION__,__LINE__); \
                printf("Block(%p)(nkey:%-8d,key:%-10s,nvalue:%-8d,%d|%d|%d) :",b,b->nkey,b_key,b->nvalue,SLAB_ID(b->slab_id),GET_WARM_BIT(IN_LRU_WARM(b->slab_id)),GET_COLD_BIT(IN_LRU_COLD(b->slab_id)));\
                printf((f_), ##__VA_ARGS__); \
                LOG_UNLOCK \
              } \
              while(0)

      
#define LOG_B_W(b,f_, ...) \
          do{\
              char b_key[b->nkey + 1]; \
              memcpy(b_key,BLOCK_k(b),b->nkey); \
              b_key[b->nkey] = 0; \
              LOG_LOCK \
              printf(RED); \
              printf("[0x%-10lx]warning: %-11s:%-20s:%-4d  ",(unsigned long)pthread_self(),__FILE__,__FUNCTION__,__LINE__); \
              printf("Block(%p)(nkey:%-8d,key:%-10s,nvalue:%-8d,%d|%d|%d) :",b,b->nkey,b_key,b->nvalue,SLAB_ID(b->slab_id),GET_WARM_BIT(IN_LRU_WARM(b->slab_id)),GET_COLD_BIT(IN_LRU_COLD(b->slab_id)));\
              printf((f_), ##__VA_ARGS__); \
              printf(NONE); \
              LOG_UNLOCK \
            } \
            while(0)

#define LOG_B_K(nkey,key,f_, ...) \
          do{\
              char b_key[nkey + 1]; \
              memcpy(b_key,key,nkey); \
              b_key[nkey] = 0; \
              LOG_LOCK \
              printf("[0x%-10lx]%-11s:%-20s:%-4d  ",(unsigned long)pthread_self(),__FILE__,__FUNCTION__,__LINE__); \
              printf("Block(nkey:%-8d,key:%-10s) :",nkey,b_key);\
              printf((f_), ##__VA_ARGS__); \
              LOG_UNLOCK \
            } \
            while(0)

#endif

#define LOG_W(f_, ...) \
     do{\
         LOG_LOCK \
         printf(RED); \
         printf("[0x%-10lx]warning: %-11s:%-20s:%-4d ",(unsigned long)pthread_self(),__FILE__,__FUNCTION__,__LINE__); \
         printf((f_), ##__VA_ARGS__); \
         printf(NONE); \
         LOG_UNLOCK \
       } \
       while(0)

#define LOG_E(f_, ...) \
    do{\
        LOG_UNLOCK \
        printf(RED); \
        printf("[0x%-10lx]error: %-11s:%-20s:%-4d ",(unsigned long)pthread_self(),__FILE__,__FUNCTION__,__LINE__); \
        printf((f_), ##__VA_ARGS__); \
        printf(NONE); \
        LOG_LOCK \
      } \
      while(0)

#else
#define LOG_I(f_, ...)
#define LOG_B(b,f_, ...)
#define LOG_B_K(nkey,key,f_, ...)
#define LOG_B_W(b,f_, ...)
#define LOG_W(f_, ...)

#define LOG_E(f_, ...) \
    do{\
        LOG_UNLOCK \
        printf(RED); \
        printf("[0x%-10lx]error: %-11s:%-20s:%-4d ",(unsigned long)pthread_self(),__FILE__,__FUNCTION__,__LINE__); \
        printf((f_), ##__VA_ARGS__); \
        printf(NONE); \
        LOG_LOCK \
      } \
      while(0)
      
      
#endif


#endif

