/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/


#include "args.h"
#include "daemon.h"
#include "log.h"
#include "config.h"
#include "mem.h"


#include <getopt.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>
#include <assert.h>




extern config g_config;
#define EXIT_WITH_USAGE \
{ \
    usage();\
     _exit(EXIT_SUCCESS);\
}



static void enable_coredump(){
    if (g_config.enable_coredump) {
        struct rlimit rlim;
        struct rlimit rlim_new;
        if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
            rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
            if (setrlimit(RLIMIT_CORE, &rlim_new)!= 0) {
                rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
                (void)setrlimit(RLIMIT_CORE, &rlim_new);
            }
        }
        if ((getrlimit(RLIMIT_CORE, &rlim) != 0) || rlim.rlim_cur == 0) {
            LOG_E("failed to ensure corefile creation\n");
            perror("getrlimit");
            exit(EXIT_FAILURE);
        }
    }

}

static void usage(){
    printf("%s Complied at %s %s \n", g_config.version,__DATE__,__TIME__);
    printf("-p --port                                            set the port to listen\n"
           "-P --PROXY                                           enable process as proxy which have -20 nice and just 64M memory used\n"
           "-d --daemon                                          run as a daemon\n"
           "-c --enable-coredumps                                enable coredump when error\n"
           "-m --memory-limit=<num>                              set the default memory, Unit is G\n"
           "-M --memory-max-limit=<num>                          set the max memory you can use in the computer which must bigger than memory limit. Unit is G\n"
           "-s --slab-recycle                                    enable slab recycle (beta)\n"
           "-l --lru=<min-max-count-interval-switcher>\n"
           "                                                     set three LRU list: [hot<=min],[min<warm<max],[max<=cold],Unit: Second\n"
           "                                                     set LRU count: the count of update and recycle\n"
           "                                                     set the interval: the next update or recycle, Unit: Second\n"
           "                                                     set LRU switcher: [off=0],[just update=1],[update and recycle=2]\n"
           "                                                     example: -l 50-100-2000-100-1\n"
           "-L --LRU=<min-max-count-interval-switcher>  \n"
           "                                                     the same as -l --lru but the unit is day\n"
           "-b --backup                                          enable backup\n");
}
static void call_r_lru(char* arg, char unit){
    assert(NULL != arg);
    int len = strlen(arg);
    char* mark3 = strchr(arg,'-');
    if(NULL == mark3) EXIT_WITH_USAGE;
    if((mark3 - arg + 1) == len) EXIT_WITH_USAGE;
    char* mark4 = strchr(mark3 + 1,'-');
    if(NULL == mark4) EXIT_WITH_USAGE;
    if((mark4 - arg + 1) == len) EXIT_WITH_USAGE;
    char* mark5 = strchr(mark4 + 1,'-');
    if(NULL == mark5) EXIT_WITH_USAGE;
    if((mark5 - arg + 1) == len) EXIT_WITH_USAGE;


    char* mark6 = strchr(mark5 + 1,'-');
    if(NULL == mark6) EXIT_WITH_USAGE;
    if((mark6 - arg + 1) == len) EXIT_WITH_USAGE;
    int min = atoi(arg);
    int max = atoi(mark3 + 1);
    int count = atoi(mark4 + 1);
    int interval = atoi(mark5 + 1);
    int switcher = atoi(mark6 + 1);
    if((0 >= min) || (min >= max) || (0 >= count) || (0 > switcher)){
          LOG_E("[0 < min:%d < max:%d ], interval:%d > 10 switcher:%d >= 0\n",min,max,interval,switcher);
          usage();
         _exit(EXIT_SUCCESS);
    }
    LOG_I("min = %d%c, max = %d%c count = %d,interval = %d%c,switcher = %d\n",min,unit, max,unit,count,interval,unit,switcher);
    if('D' == unit){
        min *= 3600 * 24;
        max *= 3600 * 24;
        interval *= 3600 *24;
    }
    set_lru_parameter(min,max,count,interval,switcher);
        
}

int args_init(int argc, char**argv){
    char *short_opts = 
        "p:" /*port to listen*/
        "d"  /*daemonize*/
        "c"  /*enable coredump*/
        "m:" /*default memory limited*/
        "M:" /*max memory limited*/
        "h"  /*show usage*/
        "b"  /*enable backup*/
        "s"  /*enable slab recycle*/
        "l:" /*update lru parameter*/
        "L:" /*update lru parameter*/
        "P"  /*create a proxy process*/
        ;
    const struct option long_opts[] = {
            {"port", required_argument, 0, 'p'},
            {"daemon", no_argument, 0, 'd'},
            {"enable-coredumps", no_argument, 0, 'c'},
            {"memory-limit", required_argument, 0, 'm'},
            {"memory-max-limit", required_argument, 0, 'M'},
            {"help", no_argument, 0, 'h'},
            {"backup", no_argument, 0, 'b'},
            {"slab-recycle", no_argument, 0, 's'},
            {"lru", required_argument, 0, 'l'},
            {"LRU", required_argument, 0, 'L'},
            {"PROXY", no_argument, 0, 'P'},
            {0, 0, 0, 0}
                
    };
    int opt_index;
    int c = -1;
    uint64_t max_mem_pc = getTotalSystemMemory();
    g_config.version = "Version at: 2.1 on 2019/6/12";
    while (-1 != (c = getopt_long(argc, argv, short_opts,
                    long_opts, &opt_index))) {
        switch(c){
            case 'p':
                g_config.port = atoi(optarg);
                break;
            case 'd':
                g_config.daemonize = 1;
                break;
            case 'c':
                g_config.enable_coredump = 1;
                enable_coredump();
                break;
            case 'm':
                g_config.mem_size = ((size_t)atoi(optarg)) * 1024 * 1024 * 1024;
                g_config.mem_max_size = max_mem_pc;
                break;
            case 'M':
                g_config.mem_max_size = ((size_t)atoi(optarg)) * 1024 * 1024 * 1024;
                if(g_config.mem_max_size > max_mem_pc) g_config.mem_max_size = max_mem_pc;
                break;
            case 'b':
                g_config.enable_backup = 1;
                break;
            case 's':
                g_config.enable_slab_recycle = 1;
                break;
            case 'l':
                call_r_lru(optarg,'S');
                break;
            case 'L':
                call_r_lru(optarg,'D');
                break;
            case 'P':
                g_config.mem_size = 1024LL * 1024  * 64;
                g_config.mem_max_size = g_config.mem_size;
                g_config.enable_backup = 0;
                g_config.enable_slab_recycle = 0;
                int which = PRIO_PROCESS;
                id_t pid = getpid();
                int priority = -20;
                char renice_cmd[128] = {0};
                sprintf(renice_cmd, "sudo renice %d %d", priority, pid);
                system(renice_cmd);
                int ret = getpriority(which, pid);
                if(-1 == ret){
                    LOG_E("getpriority error\n");
                }else{
                    LOG_I("getpriority = %d\n",ret);
                }
                break;
            default:
                usage();
                _exit(EXIT_SUCCESS);
                break;
        }

    }
    if(g_config.daemonize){
        g_config.enable_coredump = 1;
        LOG_I("daemonize");
        daemon_init(g_config.enable_coredump);
    }
    return 0;
}

