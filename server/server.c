/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "server.h"
#include "murmur3_hash.h"
#include "remote.h"
#include "conf.h"
#include "log.h"




#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <string.h>
#include <assert.h>



typedef struct __server{
    char* ip;
    int port;
    int weight;
    uint64_t hit;
    
} _server;


typedef struct _hash_s{
    uint32_t hash;
    _server* s;
}hash_s;





static _server* slist = NULL;
static uint32_t s_size = 20;
static uint32_t s_used = 0;

static hash_s* hlist = NULL;
static uint32_t h_size = 2048;
static uint32_t h_used = 0;

static int compar(const void * a, const void * b){
    hash_s* a_s = (hash_s*)a;
    hash_s* b_s = (hash_s*)b;
    if(a_s->hash == b_s->hash){
        return 0;
    }else if(a_s->hash > b_s->hash){
        return 1;
    }
    return -1;
}

static void _server_add(char* ip, int port, int weight){
    assert(NULL != ip);
    assert(0 <= weight);
    assert(2048 > weight);
    if(NULL == slist){
        slist = calloc(s_size, sizeof(_server));
        assert(NULL != slist);
    }
    if(s_size == s_used){
        s_size = 2 * s_size;
        slist = realloc(slist,s_size * sizeof(_server));
    }
    if(NULL == hlist){
        hlist = calloc(h_size, sizeof(hash_s));
        assert(NULL != hlist);
    }
    if(h_size <= (h_used + weight)){
        h_size = 2 * h_size + weight;
        hlist = realloc(hlist,h_size * sizeof(hash_s));
    }
    
    uint32_t k = 0;
    while(k < s_used){
        if((slist[k].port == port) && !memcmp(slist[k].ip,ip,strlen(slist[k].ip))){
            LOG_E("the %s:%d is exsited\n",ip,port);
            return;
        }
        ++ k;
    }
#ifdef SERVER_DEBUG

    uint32_t h = 0;
    uint32_t h_befort_count = 0;
    uint32_t h_after_count = 0;
    while(h < s_used){
        LOG_I("before: %s:%d %d\n",slist[h].ip,slist[h].port,slist[h].weight);
        ++ h;
        ++ h_befort_count;
    }
#endif
    if(0 == weight) weight = 1;
    slist[s_used].ip = malloc(strlen(ip) + 1);
    memcpy(slist[s_used].ip,ip,strlen(ip));
    slist[s_used].ip[strlen(ip)] = 0;
    slist[s_used].port = port;
    slist[s_used].weight = weight;
    int i = 0;
    int len = strlen(ip) + 50;
             //127.0.0.1:55555:90  50 is enough for 2 int64 when are converted to string
    char* format = malloc(len);
    assert(NULL != format);
    while(i < weight){
        snprintf(format,len,"%s:%d#%d",ip,port,i);
        #ifdef SERVER_DEBUG
        LOG_I("format = %s\n",format);
        #endif
        //here we use the same len to make more balance
        hlist[h_used].hash = MurmurHash3_x86_32(format,len);
        hlist[h_used].s = &slist[s_used];
        h_used += 1;
        memset(format, 0, len);
        ++ i;
    }
#ifdef SERVER_DEBUG
    uint32_t j = 0;
    while(j < h_used){
        LOG_I("hash[%-3d] = %u, s = %s:%d\n",j,hlist[j].hash,hlist[j].s->ip,hlist[j].s->port);
        ++ j;
    }
#endif
    qsort(hlist,h_used,sizeof(hash_s),compar);
#ifdef SERVER_DEBUG
    j = 0;
    while(j < h_used){
        LOG_I("sorted: hash[%-3d] = %u, s = %s:%d\n",j,hlist[j].hash,hlist[j].s->ip,hlist[j].s->port);
        ++ j;
    }
#endif
    s_used ++;
#ifdef SERVER_DEBUG
        h = 0;
        while(h < s_used){
            LOG_I("after: %s:%d %d\n",slist[h].ip,slist[h].port,slist[h].weight);
            ++ h;
            h_after_count ++;
        }
        assert(h_after_count == (h_befort_count + 1));
#endif

    free(format);
}

void server_stats(){
    uint32_t j = 0;
    while(j < h_used){
        LOG_I("total: hash[%-3d] = %u, s = %s:%d\n",j,hlist[j].hash,hlist[j].s->ip,hlist[j].s->port);
        ++ j;
    }
}


extern _conf_data proxy_conf;
static int proxy_add(char* conf){
    char* marker1 = strchr(conf,'=');
    if(NULL == marker1){
        LOG_E("conf(%s) format is error!\n",conf);
        return -1;
    }
    char* marker2 = strchr(marker1 + 1,':');
    if(NULL == marker2){
        LOG_E("port(%s) not found!\n",conf);
        return -1;
    }
    char* host = malloc(marker2 - marker1);
    memcpy(host,marker1 + 1, marker2 - marker1 - 1);
    host[marker2 - marker1 - 1] = '\0';
    int port = atoi(marker2 + 1);
    int value_len = 0;
    char* value = r_get(host, port, strlen(proxy_conf.key), proxy_conf.key,&value_len, 0,0,0);
    int readed = 0;
    if(NULL == value){
        LOG_W("proxy(%s:%d) does not have info of servers, try to read from file\n",host,port);
        value = conf_from_file(&proxy_conf);
        if(NULL == value){
            LOG_E("read_from_file value is null!\n");
            return -1;
        }
        readed = 1;
    }
    assert(proxy_conf.max_len >= strlen(value));
    free(host);
    server_add(value,0,0);
    if(!readed) conf_to_file(&proxy_conf, value);
    free(value);
    return 0;
}

int server_init(char* conf){
    assert(NULL != conf);
    if(NULL != strchr(conf, 'S')){
        server_add(conf,0,0);
        return 0;
    }
    return proxy_add(conf);


}

void server_add(char* conf, int port, int weight){
    assert(NULL != conf);
    // conf = "--SERVER=ip:port weight --SERVER=ip:port weight"
    char* p = strchr(conf,'=');
    if(NULL == p){
        _server_add(conf,port,weight);
        return;
    }
    int len = strlen(conf);
    while(NULL != p){
        char _ip[256] = {0};
        char _port[256] = {0};
        char _weight[256] = {0};
        p ++;
        char* end = strchr(p, ':');
        memcpy(_ip, p, end - p);
        p = end + 1;
        end = strchr(p, ' ');
        memcpy(_port, p, end -p);
        p = end + 1;
        end = strchr(p, ' ');
        if(NULL == end){
            memcpy(_weight, p, conf + len -p);
        }else{
            memcpy(_weight, p, end -p);  
        }
        _server_add(_ip,atoi(_port),atoi(_weight));
        p = strchr(p,'=');
    }
}

void server_get(uint32_t key, char* ip, int* port){
    uint32_t i = 0;
    while(i < h_used){
        if(key < hlist[i].hash){
            break;
        }
        ++ i;
        
    }
    if(i == h_used){
        i = 0;
#ifdef SERVER_DEBUG
        LOG_I("get the first one\n%u > %u at %s:%d\n",key, hlist[i].hash, hlist[i].s->ip,hlist[i].s->port);
#endif    
    }else{
#ifdef SERVER_DEBUG
        LOG_I("%u < %u at %s:%d\n",key, hlist[i].hash, hlist[i].s->ip,hlist[i].s->port);
#endif
    }
    hlist[i].s->hit ++;
    memcpy(ip,hlist[i].s->ip,strlen(hlist[i].s->ip));
    *port = hlist[i].s->port;
   
}

