/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "block.h"

#include "config.h"
#include "log.h"

#include <string.h>
#include <assert.h>


extern config g_config;

//#define BLOCK_DEBUG
#ifdef BLOCK_DEBUG
#define BLOCK_LOG_B LOG_B

static uint32_t block_debug(block* father){
    assert(NULL != father);
    LOG_B(father,"father found!\n");
    block** child_ptr = (block**)BLOCK_c(father);
    block* child = *child_ptr;
    int count = 1;
    uint32_t nvalue = father->nvalue - BLOCK_FATHER_nv(father);
    while(NULL != child){
        LOG_B(child,"child found!\n");
        nvalue -= child->nvalue;
        if(NULL == child->next){
            assert(child->nvalue < g_config.slab_max_size);
        }else{
            assert(child->nvalue == g_config.slab_max_size - sizeof(block));
        }
        child = child->next;
        count ++;
        
     }
    assert(0 == nvalue);
    return count;
}
#else
#define BLOCK_LOG_B(b,f_, ...)
#endif


void block_set_key(block* b,const uint32_t nkey, const char* key){
    assert(0 < nkey && NULL != key);
    assert(0 == (b->flags & BLOCK_FLAGS_CHILD));
    b->nkey = nkey;
    memcpy(BLOCK_k(b),key,nkey);

}
void block_set_value(block* b,const uint32_t nvalue, const char* value){
    assert(0 < nvalue && NULL != value);
    if(0 != (b->flags & BLOCK_FLAGS_HAVE_CHILD)){
         uint32_t nvalue_father = g_config.slab_max_size - sizeof(block) - sizeof(block*) - b->nkey;
         if(0 != nvalue_father){
            memcpy(BLOCK_v(b),value,nvalue_father);
         }
         b->nvalue = nvalue;
         BLOCK_LOG_B(b,"set father nvalue[%u]\n",nvalue_father);
         value += nvalue_father;
         block** child_ptr = (block**)BLOCK_c(b);
         block* child = *child_ptr;    
#ifdef BLOCK_DEBUG
         
         uint32_t after_count = 0;
         uint32_t before_count = block_debug(b);
#endif
         while(NULL != child){
            memcpy(BLOCK_CHILD_v(child),value,child->nvalue);
            value += child->nvalue;
            BLOCK_LOG_B(child,"set child nvalue[%u]\n",child->nvalue);
            child = child->next;  
         }
#ifdef BLOCK_DEBUG
         after_count = block_debug(b);
         assert(after_count == before_count);
#endif
        
    }else{
        b->nvalue = nvalue;
        memcpy(BLOCK_v(b),value,nvalue);
    }

}

void block_stats(block* b){
    assert(NULL != b);
    #if 0
    if(b->flags & BLOCK_FLAGS_BKLIST){
        LOG_B(b,"get real block flags = %x\n",b->flags);
    }
    #endif
}
void block_equal(block* b1, block* b2){
    assert(b1 != b2);
    assert(b1->next == b2->next);
    assert(b1->l_next == b2->l_next);
    assert(b1->prev == b2->prev);
    assert(b1->flags == b2->flags);
    assert(b1->nkey == b2->nkey);
    assert(b1->nvalue == b2->nvalue);
}

