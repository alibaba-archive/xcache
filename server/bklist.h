/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_BKLIST_H_
#define _X_BKLIST_H_

#include "block.h"


void  bklist_init();

int bklist_add(block* b);
int bklist_add_nolock(block* b, uint32_t hv);


int bklist_delete(block* b);
int bklist_delete_nolock(block* b, uint32_t hv);


block* bklist_find(const int nkey, const char* key);
block* bklist_find_no_lock(const uint32_t nkey, const char* key,uint32_t hv);
int bklist_replace_no_lock(block* old_b, block* new_b, uint32_t hv);



void bklist_lock(const int nkey, const char* key,uint32_t* hv );

int bklist_try_lock(const int nkey, const char* key,uint32_t* hv );
void bklist_unlock(uint32_t hv);

char* bklist_stats(uint64_t* r_total);




#endif
