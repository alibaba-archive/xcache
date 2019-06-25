/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "clock.h"

#include <stddef.h>

static time_t tv_sec_start;


void clock_init(){
    tv_sec_start = get_current_time_sec();
}


time_t get_current_time_sec(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}



