/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "config.h"
#include "clock.h"
#include "page.h"
#include "slab.h"
#include "block.h"
#include "bklist.h"
#include "lru.h"
#include "net.h"
#include "worker.h"
#include "connect.h"
#include "protocol.h"
#include "stats.h"
#include "args.h"
#include "backup.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


void test_case_1(){
    int i = 0;
    int count = 0;
    while(i < 1024){
        int j = 0;
        while(j < 1){
            block* b = slab_alloc(1024 * 1024 * 8);
            assert(NULL != b);
            ++j;
            ++count; 
            //printf("count = %d\n",count);
         }
        ++i;       
    }
    printf("count = %d\n",count);

/*
without memset
real    0m0.111s
user    0m0.011s
sys     0m0.101s

*/
/*
real    0m6.374s
user    0m0.552s
sys     0m5.835s

*/

/*
memcached
without memset
real    0m0.115s
user    0m0.010s
sys     0m0.123s


*/
/*
real    0m6.095s
user    0m0.480s
sys     0m5.659s


*/

}



void logo_init(){
    printf("Complied at %s %s \n",__DATE__,__TIME__);  
}



extern config g_config;
int main (int argc, char**argv){
        logo_init();
        args_init(argc, argv);
        stats_init();
        clock_init();
        config_init(argc,argv);
        page_init();
        slab_init();
        lru_init();
        bklist_init();
        con_init();
        worker_init();
        backup_init();
        net_init(NULL,g_config.port,tcp);
        return 0;
}

