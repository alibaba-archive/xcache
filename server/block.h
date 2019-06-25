/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_BLOCK_H_
#define _X_BLOCK_H_

#include <sys/time.h>
#include <stdint.h>


#define BLOCK_k(b) ((b)->flags & BLOCK_FLAGS_HAVE_CHILD ? (b)->data + sizeof(struct block*) : (b)->data)
#define BLOCK_v(b) ((b)->flags & BLOCK_FLAGS_HAVE_CHILD ? (b)->data + sizeof(struct block*) + (b)->nkey : (b)->data + (b)->nkey)
#define BLOCK_c(b) ((b)->data)
#define BLOCK_FATHER_nv(b) (g_config.slab_max_size - sizeof(block) - sizeof(struct block*) - (b)->nkey)

                               
// child without key data
#define BLOCK_CHILD_v(b) ((b)->data)
#define BLOCK_CHILD_k(b) (NULL)



#define BLOCK_FLAGS_SLAB          (1)
#define BLOCK_FLAGS_BKLIST        (1 << 1)
#define BLOCK_FLAGS_LRU           (1 << 2)
#define BLOCK_FLAGS_WILL_FREE     (1 << 3)
#define BLOCK_FLAGS_HAVE_CHILD    (1 << 4)
#define BLOCK_FLAGS_CHILD         (1 << 5)

#define BLOCK_FLAGS_MAX           (1 << 7)




typedef struct _block{
    struct _block* prev;   // used_in_lru
    struct _block* next;   // used_in_lru
    struct _block* l_next; // used_in_bklist
                           // used_in_child is point to father
    uint16_t nkey;
    uint16_t nref;
    uint32_t nvalue;
    uint8_t flags;
    uint8_t slab_id;
    time_t time;
    char data[]; //if (1 == (flags & BLOCK_FLAGS_HAVE_CHILD))
                //the first data(0 - sizeof(block*) is the _block** child
}block;




void block_set_key(block* b,const uint32_t nkey, const char* key);
void block_set_value(block* b,const uint32_t nvalue, const char* value);
void block_stats(block* b);
void block_equal(block* b1, block* b2);


#endif


