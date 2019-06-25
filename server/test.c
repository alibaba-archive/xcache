/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "config.h"
#include "log.h"
#include "block.h"
#include "remote.h"
#include "server.h"
#include "args.h"
#include "hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <pthread.h>



extern config g_config;
#define max_nkey_and_nvalue (int)(g_config.slab_max_size - sizeof(block))

#define TEST_CASE_SERVER_LIST

#ifdef TEST_CASE_SERVER_LIST
static char* case_server = NULL;
static int case_port = -1;

#else

#if 0
static char* case_server = "127.0.0.1";
static int case_port = 66666;
#else
static char* case_server = "127.0.0.1";
static int case_port = 66666;
#endif
#endif




static int case_try_time = 1;
static int case_enable_detal = 0;
static int case_sleep_time = 0;
static int case_socket_block = 0;


#define TEST_CASE_SERVER   case_server,case_port

#define TEST_CASE_SOCKET   case_enable_detal,case_sleep_time,case_socket_block




static void test_case_0();
static void test_case_1();
static void test_case_2();
static void test_case_3();
static void test_case_4();
static void test_case_5();
static void test_case_6();





static void test_case_7_8_max_size(char case_no,int nkey,int nvalue,int index);
static void test_case_7();
static void test_case_8();


static void *test_case_thread_set(void *arg);
static void *test_case_thread_get(void *arg);
static void test_case_thread(int thread_num, char case_no,int nkey, int nvalue);
static void test_case_9();






typedef struct _missed_b{
    uint32_t nkey;
    uint32_t nvalue;
    char key[1000];
    
} miss_b;

static miss_b b_error[50000] = {{0}};



static int b_error_index = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void add_to_miss(const uint32_t nkey,char* key, const uint32_t nvalue){
    pthread_mutex_lock(&lock);
    b_error[b_error_index].nkey = nkey;
    b_error[b_error_index].nvalue = nvalue;
    assert(1000 >= nkey);
    memcpy(b_error[b_error_index].key,key,nkey);
    ++b_error_index;
    assert(b_error_index < 50000);
    pthread_mutex_unlock(&lock);
}

void show_miss(){
    int i = 0;
    for(; i < b_error_index; ++i){
        LOG_B_K(b_error[i].nkey,b_error[i].key,"nvalue = %d is missed!!\n",b_error[i].nvalue);
    }
  
    LOG_E("total miss block is %d\n",b_error_index);
 
    b_error_index = 0;
}


static void r_get_try(const uint32_t nkey,char* key,const int nvalue,char* value_ok, int detal, int sleeptime, int block);

static void r_get_try(const uint32_t nkey,char* key,const int nvalue,char* value_ok, int detal, int sleeptime, int block){ 
     int len = 0;
     char* value =  NULL;
     int try = 0; 
     while(nvalue != len || 0 != memcmp(value_ok,value,nvalue)){  
         if(try >= case_try_time) break;
         ++ try; 
         value = r_get(TEST_CASE_SERVER,nkey,key,&len,detal, sleeptime, block);
         usleep(1 * 1000 * 10);
         LOG_I("nkey[%d]key[%s] try = %d\n",nkey,key,try);
     }
     if(nvalue != len || 0 != memcmp(value_ok,value,nvalue)){ 
         LOG_E("nkey[%d] key:%s may be recycle!!\n",nkey,key);
         add_to_miss(nkey,key,nvalue);
         
     }
     else{
         LOG_I("nkey[%d] key:%s total try = %d\n",nkey,key,try);
		 free(value);
     }
}




static void test_case_0(){
    r_set(TEST_CASE_SERVER,14,"xcache_servers",15,"127.0.0.1:55555",TEST_CASE_SOCKET);

}




static void test_case_1(){
    #if 0
    r_set(TEST_CASE_SERVER,2,"hi",5,"candy",TEST_CASE_SOCKET);
    r_get_try(2,"hi",5,"candy",TEST_CASE_SOCKET);
    r_set(TEST_CASE_SERVER,5,"hello",5,"vaney",TEST_CASE_SOCKET);
    r_get_try(5,"hello",5,"vaney",TEST_CASE_SOCKET);


    r_del(TEST_CASE_SERVER,5,"hello",TEST_CASE_SOCKET);
    r_del(TEST_CASE_SERVER,2,"hi",TEST_CASE_SOCKET);

    r_get_try(2,"hi",5,"candy",TEST_CASE_SOCKET);
    r_get_try(5,"hello",5,"vaney",TEST_CASE_SOCKET);
    #else
    //r_set(TEST_CASE_SERVER,14,"xcache_servers",15,"127.0.0.1:55555",TEST_CASE_SOCKET);
    r_get_try(14,"xcache_servers",15,"127.0.0.1:55555",TEST_CASE_SOCKET);
    #endif


}

static void test_case_2(){
    int i = 1;
    for(; i < 20; ++i){
        char key[20] = {0};
        char value[20] = {0};
        snprintf(key,20,"%d",i);
        snprintf(value,20,"candy%d",i);
        key[18] = '2';
        value[18] = '2';
        r_set(TEST_CASE_SERVER,20,key,20,value,TEST_CASE_SOCKET);
    }


    i = 1;
    for(; i < 20; ++i){
        char key[20] = {0};
        char value_ok[20] = {0};
        snprintf(key,20,"%d",i);
        snprintf(value_ok,20,"candy%d",i);
        key[18] = '2';
        value_ok[18] = '2';
        r_get_try(20,key,20,value_ok,TEST_CASE_SOCKET);
    }
  
}

static void test_case_3(){
    int i = 1;
    for(; i < 2000; ++i){
        char key[30] = {0};
        char value[200] = {0};
        snprintf(key,30,"%d",i);
        snprintf(value,200,"candy%d",i);
        key[28] = '3';
        value[198] = '3';
        r_set(TEST_CASE_SERVER,30,key,200,value,TEST_CASE_SOCKET);
    }
    i = 1;
    for(; i < 2000; ++i){
        char key[30] = {0};
        char value_ok[200] = {0};
        snprintf(key,30,"%d",i);
        snprintf(value_ok,200,"candy%d",i);
        key[28] = '3';
        value_ok[198] = '3';
        r_get_try(30,key,200,value_ok,TEST_CASE_SOCKET);
    }
}


static void test_case_4(){
    int i = 1;
    for(; i < 2000; ++i){
        char key[40] = {0};
        char value[20000] = {0};
        snprintf(key,40,"%d",i);
        snprintf(value,20000,"candy%d",i);
        key[38] = '4';
        value[19998] = '4';
        r_set(TEST_CASE_SERVER,40,key,20000,value,TEST_CASE_SOCKET);
    }
    i = 1;
    for(; i < 2000; ++i){
        char key[40] = {0};
        char value_ok[20000] = {0};
        snprintf(key,40,"%d",i);
        snprintf(value_ok,20000,"candy%d",i);
        key[38] = '4';
        value_ok[19998] = '4';
        r_get_try(40,key,20000,value_ok,TEST_CASE_SOCKET);
    }

    
}



static void test_case_5(){
    r_expand("127.0.0.1",55555,1000,TEST_CASE_SOCKET);
}


static void test_case_6(){
    int i = 1;
    for(; i < 2000; ++i){
        char key[60] = {0};
        char value[200000] = {0};
        snprintf(key,60,"%d",i);
        snprintf(value,200000,"candy%d",i);
        key[58] = '6';
        value[199998] = '6';
        r_set(TEST_CASE_SERVER,60,key,200000,value,TEST_CASE_SOCKET);
    }
    i = 1;
    for(; i < 2000; ++i){
        char key[60] = {0};
        char value_ok[200000] = {0};
        snprintf(key,60,"%d",i);
        snprintf(value_ok,200000,"candy%d",i);
        key[58] = '6';
        value_ok[199998] = '6';
        r_get_try(60,key,200000,value_ok,TEST_CASE_SOCKET);
    }
  
}



static void test_case_7_8_max_size(char case_no, int nkey,int nvalue, int index){
    char key_candy[nkey];
    memset(key_candy,0,nkey);
    snprintf(key_candy,nkey,"%d",index);
    
    char* candy = calloc(nvalue,sizeof(char));
    assert(NULL != candy);
    memcpy(candy,"candy",5);

    key_candy[nkey - 2] = case_no;
    candy[nvalue - 2] = case_no;
    
    r_set(TEST_CASE_SERVER,nkey,key_candy,nvalue,candy,TEST_CASE_SOCKET);
    //r_get_try(nkey,key_candy,nvalue,candy,TEST_CASE_SOCKET);
    free(candy);
    
}

static void test_case_7(){
      int i = 0;
      while(i < 1024){
            test_case_7_8_max_size('7',32,1024 * 1024 * 40 + 10,i);
            i ++;
      }
}

static void test_case_8(){
    int nkey = 8;
    int nvalue = max_nkey_and_nvalue - nkey + 1;
    test_case_7_8_max_size('8',nkey, nvalue,8);

}


typedef struct _case_data{
    int index;
    int nkey;
    int nvalue;
    char case_no;

}case_data;

////////////////////
///////////////////



#define thread_enable_detal 0
#define thread_sleep_time 0
#define thread_socket_block 1

static void *test_case_thread_set(void *arg){
    r_get_try(14,"xcache_servers",15,"127.0.0.1:55555",TEST_CASE_SOCKET);
    return NULL;
    case_data* data = (case_data*)arg;
    assert(NULL != data);
    int i = data->index;
    char* key = calloc(1, data->nkey);
    char* value = calloc(1, data->nvalue);
    snprintf(key,data->nkey,"%d",i);
    snprintf(value,data->nvalue,"candy%d",i);
    key[data->nkey - 2] = data->case_no;
    value[data->nvalue - 2] = data->case_no;
    LOG_I("nkey[%d]key[%s],nvalue[%d]\n",data->nkey,key,data->nvalue);
    r_set(TEST_CASE_SERVER,data->nkey,key,data->nvalue,value,thread_enable_detal,thread_sleep_time,thread_socket_block);
    free(key);
    free(value);
    return NULL;

}


static void *test_case_thread_get(void *arg){
    r_get_try(14,"xcache_servers",15,"127.0.0.1:55555",TEST_CASE_SOCKET);
    return NULL;
    case_data* data = (case_data*)arg;
    assert(NULL != data);
    int i = data->index;
    char* key = calloc(1, data->nkey);
    char* value_ok = calloc(1, data->nvalue);
    snprintf(key,data->nkey,"%d",i);
    snprintf(value_ok,data->nvalue,"candy%d",i);
    key[data->nkey - 2] = data->case_no;
    value_ok[data->nvalue - 2] = data->case_no;
    LOG_I("nkey[%d]key[%s]\n",data->nkey,key);
    r_get_try(data->nkey,key,data->nvalue,value_ok,thread_enable_detal,thread_sleep_time,thread_socket_block);
    free(key);
    free(value_ok);
    return NULL;    
}





static void test_case_thread(int thread_num, char case_no,int nkey, int nvalue){
    int i = 1;
    
    case_data case_data_[thread_num * 2];
    pthread_t thread_id_set[thread_num];
    
    for(; i < thread_num; ++i){
        pthread_attr_t  attr;
        pthread_attr_init(&attr);
        case_data_[i].index = i;
        case_data_[i].case_no = case_no;
        case_data_[i].nkey = nkey;
        case_data_[i].nvalue = nvalue;
        LOG_I("thread test_case_8_thread_set %d\n",i);
        if(0 != pthread_create(&thread_id_set[i],&attr,test_case_thread_set,&case_data_[i])){
                perror("pthread_create()");
                exit(EXIT_FAILURE);
        }
        
    }
    #if 1
    i = 1;
    pthread_t thread_id_get[thread_num];
    for(; i < thread_num; ++i){
        pthread_attr_t  attr;
        pthread_attr_init(&attr);
        case_data_[i + thread_num].index = i;
        case_data_[i + thread_num].case_no = case_no;
        case_data_[i + thread_num].nkey = nkey;
        case_data_[i + thread_num].nvalue = nvalue;
        LOG_I("thread test_case_8_thread_get %d\n",i);
        if(0 != pthread_create(&thread_id_get[i],&attr,test_case_thread_get,&case_data_[i + thread_num])){
                perror("pthread_create()");
                exit(EXIT_FAILURE);
        }
       
    }
   i = 1;
   for(; i < thread_num; ++i){
        pthread_join(thread_id_get[i],NULL);
   }
   #endif
   i = 1;
   for(; i < thread_num; ++i){
        pthread_join(thread_id_set[i],NULL);
   }

}

static void test_case_thread_single(int thread_num, char case_no,int nkey, int nvalue) __attribute__((unused));

static void test_case_thread_single(int thread_num, char case_no,int nkey, int nvalue){
    int i = 1;
    
    case_data case_data_[thread_num * 2];
    pthread_t thread_id_set[1];
    
    for(; i < 2; ++i){
        pthread_attr_t  attr;
        pthread_attr_init(&attr);
        case_data_[i].index = i;
        case_data_[i].case_no = case_no;
        case_data_[i].nkey = nkey;
        case_data_[i].nvalue = nvalue;
        LOG_I("thread test_case_8_thread_set %d\n",i);
        if(0 != pthread_create(&thread_id_set[i],&attr,test_case_thread_set,&case_data_[i])){
                perror("pthread_create()");
                exit(EXIT_FAILURE);
        }
        
    }
    i = 1;
    pthread_t thread_id_get[thread_num];
    for(; i < thread_num; ++i){
        pthread_attr_t  attr;
        pthread_attr_init(&attr);
        case_data_[i + thread_num].index = 1;
        case_data_[i + thread_num].case_no = case_no;
        case_data_[i + thread_num].nkey = nkey;
        case_data_[i + thread_num].nvalue = nvalue;
        LOG_I("thread test_case_8_thread_get %d\n",i);
        if(0 != pthread_create(&thread_id_get[i],&attr,test_case_thread_get,&case_data_[i + thread_num])){
                perror("pthread_create()");
                exit(EXIT_FAILURE);
        }
       
    }
   i = 1;
   for(; i < 2; ++i){
        pthread_join(thread_id_set[i],NULL);
   }
   i = 1;
   for(; i < thread_num; ++i){
        pthread_join(thread_id_get[i],NULL);
   }
}


#define case_9_nkey           (30)
#define case_9_nvalue         (max_nkey_and_nvalue - 90 - 1)
#define case_9_thread_num     (20)
#define case_9_no     '9'

static void test_case_9(){
    test_case_thread(case_9_thread_num,case_9_no,case_9_nkey,case_9_nvalue);
    //test_case_thread_single(case_9_thread_num,case_9_no,case_9_nkey,case_9_nvalue);
}



static void test_case_10(){
    r_stats("127.0.0.1",66666,1,TEST_CASE_SOCKET);

}


static void test_case_11(){
    void* b_mem = calloc(1, sizeof(block) + sizeof(block*));
    void* b1_mem = calloc(1, sizeof(block) + sizeof(block*));
   
    block* b =  b_mem;
    block* b1 = b1_mem;
    b->nvalue = 5;
    b1->nvalue = 4;

    memcpy(BLOCK_c(b),&b1,sizeof(block*));
    memcpy(BLOCK_c(b1),&b,sizeof(block*));
 
    block** bb1_a = (block**)BLOCK_c(b);
    block** bb_a = (block**)BLOCK_c(b1);

    
    assert((*bb1_a)->nvalue == b1->nvalue);
    assert((*bb_a)->nvalue == b->nvalue);
    free(b_mem);
    free(b1_mem);


}



static void test_case_12(){
    block* b = (block*)malloc(1024 * 1024 * 8);
    char* data = malloc(1024 * 1024 * 8);
    char* data2 = malloc(1024 * 2);
    data[100] = 'c';
    int i = 0;
    while(i < 512){
        memcpy(b,data,1024 * 1024 * 8 + 10);
        i ++;
        data2 = realloc(data2, 1024 * 2 * 2);
        data2 = realloc(data2, 1024 * 2 * 2 * 2);
        data2 = realloc(data2, 1024 * 2 * 2 * 2 * 2);
    }
    
}

static void test_case_13(){


     //153034976

     char ip[256] = {0};
     int port = 0;
     
     server_get(3777042790,ip,&port);
     LOG_I("%ld at %s:%d\n",3061729550, ip,port);

     server_get(897165224,ip,&port);
     LOG_I("%d at %s:%d\n",897165224, ip,port);

     server_get(3777042780,ip,&port);
     LOG_I("%ld at %s:%d\n",3777042780, ip,port);


     server_get(3061729520,ip,&port);
     LOG_I("%ld at %s:%d\n",3061729520, ip,port);

     server_get(897165240,ip,&port);
     LOG_I("%d at %s:%d\n",897165240, ip,port);

     server_get(3777042790,ip,&port);
     LOG_I("%ld at %s:%d\n",3777042790, ip,port);

     FILE* f = fopen("/workspace/2018/xcache/server/key","r");
     if(NULL == f) return;
     uint32_t line = 0;
     char buf[256];
	 while (fgets(buf, sizeof(buf), f)) {
		++ line;
        LOG_I("line = %s, len = %lu\n",buf,strlen(buf));
        uint32_t _key = hash(buf,strlen(buf) -1);
        server_get(_key,ip,&port);
        LOG_I("%u at %s:%d\n",_key, ip,port);
        LOG_I("%u slot %d\n",_key,_key%16384);
     }
     LOG_I("line = %d\n",line);
     fclose(f);

 
}

static void test_case_14(){
    char* p = malloc(1024);
    char* data = "1 2 3";
    memcpy(p,data,strlen(data));


    snprintf(p,1024,"%8d,%10d",1,2);
    int len = strlen(p);
    p[len] = '\n';
    LOG_I("%s",p);
}

static int reset = 0;
static int thread_num = 6;

static void test_case_15(){
    r_backup("127.0.0.1",55555,"127.0.0.1", 55556,reset,thread_num,TEST_CASE_SOCKET);
    if(1 == reset){
        reset = 0;
    }else if(0 == reset){
        reset = 1;
    }
}




static void test_case_16(){
#include<arpa/inet.h>
#include<netdb.h>
    struct hostent* host = gethostbyname("google.com");
    if(NULL == host){
        printf("gethostbyname return NULL\n");
    }
    printf("official name: %s \n",host->h_name);
    int i = 0;
    for(; host->h_aliases[i]; ++i){
        printf("Aliases %s \n",host->h_aliases[i]);
    }
    for(i=0;host->h_addr_list[i];i++){
        printf("IP addr %s \n",inet_ntoa(*(struct in_addr*)host->h_addr_list[i]));
    }


}


static void test_case_17(){
    char* key = "server_list";
    char* value = "--SERVER=127.0.0.1:66666 20 --SERVER=127.0.0.1:77777 60";
    r_update("127.0.0.1",55555,strlen(key),key,strlen(value),value,thread_enable_detal,thread_sleep_time,thread_socket_block);
    

}

int main (int argc, char**argv){
    printf("+test remote api\n");
    args_init(argc, argv);
    config_init(argc, argv);
    server_init("--PROXY=127.0.0.1:55555");
    int i = 0;
    int j = 50000000;
    for(; i < j; ++i){
        #if 0 
        test_case_0();
        test_case_1();
        test_case_2();
        test_case_3();
        test_case_4();
        test_case_5();
        test_case_6();
        test_case_7();
        test_case_8();
        test_case_9();
        test_case_10();
        test_case_15();
        #else
        test_case_17();
        printf("i = %d\n",i);
        fflush(stdout);
        //test_case_1();
        //test_case_0();
        //test_case_2();
        //test_case_3();
        //test_case_7();
        //test_case_10();
        //test_case_15();
        #endif
        //show_miss();
    }
    fflush(stdout);
    printf("-test remote api i = %d\n",i);
    return 0;

}
