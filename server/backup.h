/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_BACK_UP_H_
#define _X_BACK_UP_H_

#include <stddef.h>
#include "block.h"

void backup_init();
void backup_add(block* b);
void backup_del(block* b);
char* backup_get_nolock(uint64_t* len);
void backup_reset_nolock();
uint64_t backup_count();
void backup_lock();
void backup_unlock();


#endif
