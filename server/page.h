/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_PAGE_H_
#define _X_PAGE_H_

#include <stdint.h>
#include <stdlib.h>


typedef struct _page{
    struct _page* next;
    struct _page* prev;
    char* buffer;
    
}page;

void page_init();
page* page_alloc();

int page_expand(size_t size);

//page_free NOT FULLY TESTED!!
void page_free(page* p);

int get_free_page_count();
char* page_stats();

#endif

