/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "mem.h"
#include <unistd.h>


uint64_t getTotalSystemMemory()
{
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

