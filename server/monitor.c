/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "remote.h"
#include "conf.h"
#include "log.h"
#include <getopt.h>
#include <sys/resource.h>

#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <unistd.h>
#include <assert.h>


#define EXIT_WITH_USAGE \
{ \
    usage();\
     _exit(EXIT_SUCCESS);\
}
    
static void usage(){
    printf("%s Complied at %s %s \n","V2.1 at 2019/6/11",__DATE__,__TIME__);
    printf("-i --info=<ip:port>,<ip:port>                       show the infos of the servers\n"
           "-I --details=<ip:port>,<ip:port>                    show the details of the servers, it costs plenty of time\n"
           "-b --backup=<ip1:port1>,<ip2:port2>@threads@reset   backup data from <ip1:port1> to <ip2:port2>\n"
           "-e --expand=<ip1:port1@num1>,<ip1:port1@num2>       expand <num1>M at<ip1:port1> <num2>M at<ip2:port2>, when num is 0 , disable auto memory expand, when num is 1, enable it\n"
           "-t --touch=<ip:port>,<ip:port>                      touch the servers\n"
           "-l --lru=<ip1:port1@min-max-count-interval-switcher>,<ip2:port2@min-max-count-interval-switcher>    \nchange the<ip:port> lru parameter at 0<hot<=min<warm<max<=cold switcher[0:off,1:just lru update,2: lru recycled]\n"
           "-L --LRU=<ip1:port1@min-max-count-interval-switcher>,<ip2:port2@min-max-count-interval-switcher>    \nUnit is day. the uint of -l --lru is second\n"
           "-s --set=<ip1:port1>[key,value]                     set the key value, it'll replace the older one\n"
           "-g --get=<ip1:port1>[key]                           get the value of the key\n"
           "-d --del=<ip1:port1>                                del the key\n"
           "-S --SET=<ip1:port1>[value]                         set the proxy value, it'll replace the older one\n"
           "-G --GET=<ip1:port1>                                get the proxy value\n");
}



static void call_r_set(char* arg, char* secrect_key){
    assert(NULL != arg);
    char* mark1 = strchr(arg, ':');
    if(NULL == mark1) EXIT_WITH_USAGE;
    char* mark2 = strchr(mark1 + 1, '[');
    if(NULL == mark2) EXIT_WITH_USAGE;

    char* mark3 = mark2;
    if(NULL == secrect_key){
        mark3 = strchr(mark2 + 1, ',');
        if(NULL == mark3) EXIT_WITH_USAGE;
    }
    char* mark4 = strchr(mark3 + 1, ']');
    if(NULL == mark4) EXIT_WITH_USAGE;
    char* host = malloc(mark1 - arg + 1);
    memcpy(host, arg, mark1 - arg);
    host[mark1 - arg] = '\0';
    int port = atoi(mark1 + 1);

    char*key = NULL;
    if(NULL != secrect_key){
        key = secrect_key;
    }else{
        key = malloc(mark3 - mark2);
        memcpy(key, mark2 + 1, mark3 - mark2 - 1);
        key[mark3 - mark2 - 1] = '\0';
    }
    
    char* value = malloc(mark4 - mark3);
    memcpy(value, mark3 + 1, mark4 - mark3 - 1);
    value[mark4 - mark3 - 1] = '\0';
    r_update(host, port, strlen(key),key,strlen(value),value,0,0,0);
    free(host);
    if(NULL == secrect_key) free(key);
    free(value);
    
}


extern _conf_data proxy_conf;

#define LOG_I_PROXY printf

static int call_r_get(char* arg, char* secrect_key, int normal){
    assert(NULL != arg);
    char* mark1 = strchr(arg, ':');
    if(NULL == mark1) EXIT_WITH_USAGE;
    char* mark2 = NULL;
    char* mark3 = NULL;
    if(NULL == secrect_key){
        mark2 = strchr(mark1 + 1, '[');
        if(NULL == mark2) EXIT_WITH_USAGE;
        mark3 = strchr(mark2 + 1, ']');
        if(NULL == mark3) EXIT_WITH_USAGE;  
    }
    
    char* host = malloc(mark1 - arg + 1);
    memcpy(host, arg, mark1 - arg);
    host[mark1 - arg] = '\0';
    int port = atoi(mark1 + 1);

    char* key = NULL;
    if(NULL != secrect_key){
        key = secrect_key;
    }else{
        key = malloc(mark3 - mark2);
        memcpy(key, mark2 + 1, mark3 - mark2 - 1);
        key[mark3 - mark2 - 1] = '\0';
    }
    int len = 0;
    char* value = r_get(host, port, strlen(key),key,&len,0,0,0);
    if(NULL == value){
        if(NULL == secrect_key){
            free(host);
            fprintf(stderr,"value is NULL\n");
            return -1;
        }
        value = conf_from_file(&proxy_conf);
        if(NULL == value){
            free(host);
            fprintf(stderr,"value is NULL\n");
            return -1;
        }
    }else{
        if(NULL != secrect_key) conf_to_file(&proxy_conf,value);
    }
    if(1 == normal){
        LOG_I("len = %d\n%s\n",len,value);
    }else{
        LOG_I_PROXY("%s\n",value);
    }

    
    free(host);
    if(NULL == secrect_key) free(key);
    if(NULL != secrect_key) {
        if(NULL != strstr(value, "SERVER")){
            free(value);
            return 0;
        }else{
            free(value);
            return -1;
        }
    }else{
        free(value);
    }
    return 0;
    
}

static void call_r_del(char* arg){
    assert(NULL != arg);
    char* mark1 = strchr(arg, ':');
    if(NULL == mark1) EXIT_WITH_USAGE;
    char* mark2 = strchr(mark1 + 1, '[');
    if(NULL == mark2) EXIT_WITH_USAGE;
    char* mark3 = strchr(mark2 + 1, ']');
    if(NULL == mark3) EXIT_WITH_USAGE;
    char* host = malloc(mark1 - arg + 1);
    memcpy(host, arg, mark1 - arg);
    host[mark1 - arg] = '\0';
    int port = atoi(mark1 + 1);
    char* key = malloc(mark3 - mark2);
    memcpy(key, mark2 + 1, mark3 - mark2 - 1);
    key[mark3 - mark2 - 1] = '\0';
    r_del(host, port, strlen(key),key,0,0,0);
    free(host);
    free(key);

    
}

static void call_r_lru(char* arg, char unit){
    assert(NULL != arg);
    char* p = strchr(arg,',');
    int len = strlen(arg);
    while(1){
        char* mark1 = strchr(arg,':');
        if(NULL == mark1) EXIT_WITH_USAGE;
        if((mark1 - arg + 1) == len) EXIT_WITH_USAGE;
        char* mark2 = strchr(mark1 + 1,'@');
        if(NULL == mark2) EXIT_WITH_USAGE;
        if((mark2 - arg + 1) == len) EXIT_WITH_USAGE;
        char* mark3 = strchr(mark2 + 1,'-');
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


        char* host = malloc(mark1 - arg + 1);
        memcpy(host,arg,mark1 - arg);
        host[mark1 - arg] = '\0';
        int port = atoi(mark1 + 1);
        int min = atoi(mark2 + 1);
        int max = atoi(mark3 + 1);
        int count = atoi(mark4 + 1);
        int interval = atoi(mark5 + 1);
        int switcher = atoi(mark6 + 1);
        if((0 >= min) || (min >= max) || (0 == port) || (0 >= count) || (0 > switcher)){
              LOG_E("min:%d must > 0 && min:%d must < max:%d && port:%d must > 0 && interval:%d must >= min:%d, switcher:%d must >= 0\n",min,min,max,port,interval,max,switcher);
              usage();
             _exit(EXIT_SUCCESS);
        }
        LOG_I("host = %s port = %d min = %d%c, max = %d%c count = %d,interval = %d%c,switcher = %d\n",host, port,min,unit, max,unit,count,interval,unit,switcher);
        if('D' == unit){
            min *= 3600 * 24;
            max *= 3600 * 24;
            interval *= 3600 *24;
        }
        r_lru(host,port,min,max,count,interval,switcher,0,0,0);
        free(host);
        if(NULL == p) _exit(EXIT_SUCCESS);
        ++ p;
        arg = p;
        p = strchr(arg,',');
     }
}

static void call_r_stats(char* arg, int full){
    assert(NULL != arg);
    char* p = strchr(arg,',');
    while(1){
        char* pp = strchr(arg,':');
        if(NULL == pp){
            usage();
             _exit(EXIT_SUCCESS);
        }
        char* host = malloc(pp - arg + 1);
        memcpy(host,arg,pp - arg);
        host[pp - arg] = '\0';
        ++ pp;
        int port = atoi(pp);
        if(0 != r_stats(host,port,full,0,0,0)){
            LOG_I("host: %s:%d is dead\n",host, port);
        }else{
            LOG_I("host: %s:%d is alive\n",host, port);
        }
        fflush(stdout);
        free(host);
        if(NULL == p) _exit(EXIT_SUCCESS);
        ++ p;
        arg = p;
        p = strchr(arg,',');
     }
    
}

static void call_r_backup(char* arg){
    assert(NULL != arg);
    char* p = strchr(arg,',');
    char* f_host = NULL;
    int f_port = 0;
    int reset = 0;
    int threads = 6;
    char* t_host = NULL;
    int t_port = 0;
    while(1){
        char* pp = strchr(arg,':');
        if(NULL == pp){
            usage();
             _exit(EXIT_SUCCESS);
        }
        char* host = malloc(pp - arg + 1);
        memcpy(host,arg,pp - arg);
        host[pp - arg] = '\0';
        ++ pp;
        int port = atoi(pp);
        if(0 == port){
             usage();
             _exit(EXIT_SUCCESS); 
        }
        if(NULL == f_host) {
            f_host = host;
            f_port = port;
        }else{
            t_host = host;
            t_port = port;
            break;
        }
        if(NULL == p) _exit(EXIT_SUCCESS);
        ++ p;
        arg = p;
        p = strchr(arg,',');
     }
    char* p_threads = strchr(arg,'@');
    if(NULL != p_threads){
        threads = atoi(++p_threads);
        if(threads < 0) threads = 3;
        char* p_reset = strchr(p_threads,'@');
        if(NULL != p_reset){
            reset = atoi(++p_reset);
            if(reset < 0) reset = 0;
            if(reset > 1) reset = 1;
        }
    }
    LOG_I("f_host = %s f_port = %d t_host = %s t_port = %d reset = %d threads = %d\n",f_host,f_port,t_host,t_port,reset,threads);
    r_backup(f_host,f_port,t_host,t_port,(uint32_t)reset,(uint32_t)threads,0,0,0);
    free(f_host);
    free(t_host);
    
}


static void call_r_expand(char* arg){
    assert(NULL != arg);
    char* p = strchr(arg,',');
    while(1){
        char* pp = strchr(arg,':');
        if(NULL == pp){
            usage();
             _exit(EXIT_SUCCESS);
        }
        char* host = malloc(pp - arg + 1);
        memcpy(host,arg,pp - arg);
        host[pp - arg] = '\0';
        ++ pp;
        char* p_num = strchr(pp, '@');
        if(NULL == p_num){
              usage();
             _exit(EXIT_SUCCESS);
        }
        char* s_port = malloc(p_num - pp);
        memcpy(s_port,pp,p_num - pp);
        int port = atoi(s_port);
        size_t size = atol(++p_num);
        if(0 == port){
              usage();
             _exit(EXIT_SUCCESS);  
        }
        LOG_I("host = %s port = %d size = %lu\n",host, port,size);
        r_expand(host,port,size,0,0,0);
        free(host);
        free(s_port);
        
        if(NULL == p) _exit(EXIT_SUCCESS);
        ++ p;
        arg = p;
        p = strchr(arg,',');
     }
    
}
static void call_r_touch(char* arg){
    assert(NULL != arg);
    char* p = strchr(arg,',');
    while(1){
        char* pp = strchr(arg,':');
        if(NULL == pp){
            usage();
             _exit(EXIT_SUCCESS);
        }
        char* host = malloc(pp - arg + 1);
        memcpy(host,arg,pp - arg);
        host[pp - arg] = '\0';
        ++ pp;
        int port = atoi(pp);
        if(0 != r_touch(host,port)){
            LOG_I("host: %s:%d is dead\n",host, port);
        }else{
            LOG_I("host: %s:%d is alive\n",host, port);
        }

        fflush(stdout);
        free(host);
        if(NULL == p) _exit(EXIT_SUCCESS);
        ++ p;
        arg = p;
        p = strchr(arg,',');
     }
    
}

static int args_init(int argc, char**argv){
    char *short_opts = 
        "i:" /*show the infos of servers*/
        "I:" /*show the details of servers*/
        "b:"  /*backup servers*/
        "e:"  /*expand the max memory of server*/
        "t:" /*heartbeat for servers*/
        "l:" /*update lru parameter,Unit is second*/
        "L:" /*update lru parameter,Unit is day*/
        "s:" /*set the key value*/
        "g:" /*get the value*/
        "d:" /*del the value*/
        "S:" /*get the value of proxy*/
        "G:" /*get the value of proxy*/
        "h"  /*usage*/
        ;
    const struct option long_opts[] = {
            {"info", required_argument, 0, 'i'},
            {"details", required_argument, 0, 'I'},
            {"backup", required_argument, 0, 'b'},
            {"expand", required_argument, 0, 'e'},
            {"touch", required_argument, 0, 't'},
            {"lru", required_argument, 0, 'l'},
            {"LRU", required_argument, 0, 'L'},
            {"set", required_argument, 0, 's'},
            {"get", required_argument, 0, 'g'},
            {"del", required_argument, 0, 'd'},
            {"SET", required_argument, 0, 'S'},
            {"GET", required_argument, 0, 'G'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
                
    };
    int opt_index;
    int c = -1;
    while (-1 != (c = getopt_long(argc, argv, short_opts,
                    long_opts, &opt_index))) {
        switch(c){
            case 'i':
                call_r_stats(optarg, 0);
                break;
            case 'I':
                call_r_stats(optarg, 1);
                break;
            case 'b':
                call_r_backup(optarg);
                break;
            case 'e':
                call_r_expand(optarg);
                break;
            case 't':
                call_r_touch(optarg);
                break;
            case 'l':
                call_r_lru(optarg,'S');
                break;
            case 'L':
                call_r_lru(optarg,'D');
                break;
            case 's':
                call_r_set(optarg,NULL);
                break;
            case 'g':
                call_r_get(optarg,NULL,1);
                break;
            case 'd':
                call_r_del(optarg);
                break;
            case 'S':
                call_r_set(optarg,proxy_conf.key);
                break;
            case 'G':
                return call_r_get(optarg,proxy_conf.key,0);
            default:
                usage();
                _exit(EXIT_SUCCESS);
                break;
        }

    }
    return 0;
}

int main (int argc, char**argv){
    return args_init(argc, argv);
}

