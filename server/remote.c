/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "remote.h"
#include "protocol.h"
#include "server.h"
#include "backup.h"
#include "murmur3_hash.h"
#define LOG_CLIENT
#include "log.h"
#include <sys/types.h>         
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sysexits.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


#include <unistd.h>
#include <string.h>
#include <pthread.h>
#define _BSD_SOURCE             /* See feature_test_macros(7) */
#include <endian.h>


#define FAILD_CALL(f,a) if( 0 != (f)){ \
    close(sfd);\
    sfd = -1; \
    if(NULL != header){ \
        free(header); \
        header = NULL; \
    }\
    return (a); \
    }

#define FAILD_CALL_2(f,a) if( 0 != (f)){ \
    close(sfd);\
    sfd = -1; \
    if(NULL != header){ \
        free(header); \
        header = NULL; \
    }\
    *nvalue = 0; \
    return (a); \
    }

#define FAILD_CALL_3(f,a) if( 0 != (f)){ \
    close(sfd);\
    sfd = -1; \
    if(NULL != header){ \
        free(header); \
        header = NULL; \
    }\
    if(NULL != res_header){ \
    free(res_header); \
    res_header = NULL; \
    }\
    *nvalue = 0; \
    return (a); \
    }

#define FAILD_CALL_4(f,a) if( 0 != (f)){ \
    close(sfd);\
    sfd = -1; \
    if(NULL != header){ \
        free(header); \
        header = NULL; \
    }\
    if(NULL != res_header){ \
    free(res_header); \
    res_header = NULL; \
    }\
    *nvalue = 0; \
    free(value_got); \
    return (a); \
    }

#define FAILD_CALL_5(f,a) if( 0 != (f)){ \
    close(sfd);\
    sfd = -1; \
    if(NULL != header){ \
        free(header); \
        header = NULL; \
    }\
    if(NULL != res_header){ \
    free(res_header); \
    res_header = NULL; \
    }\
    free(value_got); \
    return (a); \
    }

#define FAILD_CALL_6(f,a) if( 0 != (f)){ \
    close(sfd);\
    sfd = -1; \
    if(NULL != header){ \
        free(header); \
        header = NULL; \
    }\
    if(NULL != res_header){ \
    free(res_header); \
    res_header = NULL; \
    }\
    return (a); \
    }




//#define TEST_CASE_MEMCACHED
//#define TEST_LOG_II
#ifdef TEST_LOG_II
#define LOG_II LOG_I
#else
#define LOG_II(f_, ...)
#endif



static struct addrinfo * gethostinfo(char* host, int port);
static int socket_read(int sfd, char* buf, int len, int detal, int sleeptime);
static int socket_write(int sfd, const char* buf, int len, int detal, int sleeptime);
static int client_init(char* host, int port, int block, const uint32_t nkey, const char* key);
static char* r_get_long(int sfd, size_t* size_prealloced, char** value_prealloced, const uint32_t nkey, const char* key, int* nvalue, int detal, int sleeptime, int* error);
static int r_set_long(int sfd, const uint32_t nkey, const char* key, const uint32_t nvalue, const char* value, int detal, int sleeptime);


static struct addrinfo * gethostinfo(char* host, int port){
    struct addrinfo *result;
    char service[NI_MAXSERV] = {0};
    struct addrinfo hints = { .ai_flags = AI_PASSIVE,
                              .ai_family = AF_UNSPEC,
                              .ai_socktype = SOCK_STREAM};

    snprintf(service, sizeof(service), "%d", port);
    int ret = getaddrinfo(host, service,&hints,&result);
    if(0 != ret){
        if(EAI_SYSTEM == ret){
            perror("net_init:getaddrinfo()");
            return NULL;
        }
        else{
            LOG_E("getaddrinfo(): %s\n", gai_strerror(ret));
            return NULL;
        }
    }
    return result;

}

static int client_init(char* host, int port, int block, const uint32_t nkey, const char* key){
    struct addrinfo *result = NULL;
    if((NULL == host) && (-1 == port)){
        assert(0 != nkey);
        assert(NULL != key);
        char _host[256] = {0};
        int _port = -1;
        uint32_t _key = MurmurHash3_x86_32(key,nkey);
        server_get(_key,_host,&_port);
        result = gethostinfo(_host,_port);
    }else{
        result = gethostinfo(host,port);
    }
    
    if(NULL == result){
        return -1;
    }
    int sfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol); 
    if(0 > sfd){
        LOG_E("socket failed! errno = %d : %s\n",errno,strerror(errno));
        return -1;
    }
    LOG_II("+connect\n");
    if(0 != connect(sfd, result->ai_addr, result->ai_addrlen)){
        LOG_E("connect failed! errno = %d : %s\n",errno,strerror(errno));
        close(sfd);
        freeaddrinfo(result);
        return -1;
    }
    LOG_II("-connect\n");
    if(0 == block){
        if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL) | O_NONBLOCK) < 0){
            LOG_E("fcntl O_NONBLOCK failed! errno = %d : %s\n",errno,strerror(errno));
            close(sfd);
            freeaddrinfo(result);
            return -1;
        }
    }
    freeaddrinfo(result);
	assert(0 <= sfd);
    return sfd;
    
}


static int socket_write(int sfd, const char* buf, int len, int detal, int sleeptime){
    LOG_II("+ socket_write len = %d\n",len);
    int len_total_write = 0;
    int ret = -1;
    assert(0 <= detal);
    assert(0 <= sleeptime);
    assert(NULL != buf && 0 < len && 0 <= sfd);
    if(detal > len) detal = len;
    while(len > 0){
        LOG_II("+ socket_write sleeptime = %d\n",sleeptime);
        if(0 != sleeptime){
            usleep(sleeptime);
        }
        if(0 < detal && detal < len){
            ret = send(sfd, buf, detal, MSG_NOSIGNAL);
        }else{
            ret = send(sfd, buf, len, MSG_NOSIGNAL);
        }
        LOG_II("- socket_write ret = %d\n",ret);
        if(-1 == ret){
            if (EAGAIN == errno || EWOULDBLOCK == errno){
                //LOG_W("O_NONBLOCK(%d) EAGAIN(%d): socket_write errno = %d\n",EAGAIN, EWOULDBLOCK, errno);
                continue;
            }else if(EINTR == errno){
                LOG_W("EINTR(%d): socket_write errno = %d\n",EINTR, errno);
                continue;
            }else if(EPIPE == errno){
                LOG_E("get a EPIPE\n");
            }
            LOG_E("write failed! errno = %d : %s\n",errno,strerror(errno));
            return -1;
        }
        if(0 == ret){
            LOG_E("why\n");
        }
        len_total_write += ret;
        len -= ret;
        buf += ret;
    }
    LOG_II("- socket_write len_total_write = %d\n",len_total_write);
    return 0;
}

static int socket_read(int sfd, char* buf, int len, int detal, int sleeptime){
    LOG_II("+ socket_read len = %d\n",len);
    int len_total_read = 0;
    int ret = -1;
    assert(0 <= detal);
    assert(0 <= sleeptime);
    assert(NULL != buf && 0 < len && 0 <= sfd);
    if(detal > len) detal = len;
    while(len > 0){
        LOG_II("+ socket_read\n");
        if(0 != sleeptime){
            usleep(sleeptime);
        }
        if(0 == detal){
            ret = read(sfd, buf, len);
        }else{
            if(len > detal){
                ret = read(sfd, buf, detal);
            }else{
                ret = read(sfd, buf, len);
            }
        }
        LOG_II("- socket_read ret = %d\n",ret);
        if(-1 == ret){
            if (EAGAIN == errno || EWOULDBLOCK == errno){
                //may be happend when set O_NONBLOCK
                //LOG_W("O_NONBLOCK(%d) EAGAIN(%d): socket_read errno = %d\n",EAGAIN, EWOULDBLOCK, errno);
                continue;
            }else if(EINTR == errno){
                //may be happend when not set O_NONBLOCK
                LOG_W("EINTR(%d): socket_read errno = %d\n",EINTR, errno);
                continue;
            }
            LOG_E("read failed! errno = %d : %s\n",errno,strerror(errno));
            return -1;
        }
        if(0 == ret){
            LOG_II("server close the connect!\n");
            return -1;
        }
        len_total_read += ret;
        buf += ret;
        len -= ret;
    }
    LOG_II("- socket_read len_total_read = %d\n",len_total_read);
    return 0;
}
int r_touch(char* host, int port){
    int sfd = client_init(host,port,0,0,NULL);
    if(-1 == sfd){
        LOG_E("host %s:%d may be failed!\n",host,port);
        return -1;
    }else{
        close(sfd);
    }
    return 0;
}

static int r_set_long(int sfd, const uint32_t nkey, const char* key, const uint32_t nvalue, const char* value, int detal, int sleeptime){
    assert(0 < nkey && NULL != key && 0 < nvalue && NULL != value);
    LOG_II("+r_set_long\n");
    protocol_binary_request_header* header = calloc(1,sizeof(protocol_binary_request_header));
	assert(NULL != header);
    header->request.magic = PROTOCOL_BINARY_REQ;
    
    header->request.keylen = htons(nkey);
#ifdef TEST_CASE_MEMCACHED
    header->request.opcode = PROTOCOL_BINARY_CMD_SET;
    header->request.extlen = 8;
    header->request.bodylen = htonl(nvalue + 8 + nkey);
#else
    header->request.opcode = 's';
    header->request.bodylen = htonl(nvalue);
#endif
    FAILD_CALL(socket_write(sfd,(char*)(header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),-1)
#ifdef TEST_CASE_MEMCACHED
    char ext[8] = {0};
    FAILD_CALL(socket_write(sfd,ext,8,detal,sleeptime),-1)
#endif
    FAILD_CALL(socket_write(sfd,key,nkey,detal,sleeptime),-1)
    FAILD_CALL(socket_write(sfd,value,nvalue,detal,sleeptime),-1)
    free(header);
    LOG_II("-r_set_long\n");
    return 0;
}

int r_update(char* host, int port, const uint32_t nkey, const char* key, const uint32_t nvalue, const char* value, int detal, int sleeptime, int block){
    assert(0 < nkey && NULL != key && 0 < nvalue && NULL != value);
    LOG_II("+r_update\n");
    int sfd = client_init(host,port,block,nkey,key);
    if(-1 == sfd){
        LOG_E("client init failed!\n");
        return -1;
    }
    protocol_binary_request_header* header = calloc(1,sizeof(protocol_binary_request_header));
	assert(NULL != header);
    header->request.magic = PROTOCOL_BINARY_REQ;
    
    header->request.keylen = htons(nkey);
#ifdef TEST_CASE_MEMCACHED
    header->request.opcode = PROTOCOL_BINARY_CMD_SET;
    header->request.extlen = 8;
    header->request.bodylen = htonl(nvalue + 8 + nkey);
#else
    header->request.opcode = 'u';
    header->request.bodylen = htonl(nvalue);
#endif
    FAILD_CALL(socket_write(sfd,(char*)(header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),-1)
#ifdef TEST_CASE_MEMCACHED
    char ext[8] = {0};
    FAILD_CALL(socket_write(sfd,ext,8,detal,sleeptime),-1)
#endif
    FAILD_CALL(socket_write(sfd,key,nkey,detal,sleeptime),-1)
    FAILD_CALL(socket_write(sfd,value,nvalue,detal,sleeptime),-1)
    close(sfd);
    free(header);
    LOG_II("-r_update\n");
    return 0;
}

int r_set(char* host, int port, const uint32_t nkey, const char* key, const uint32_t nvalue, const char* value, int detal, int sleeptime, int block){
    assert(0 < nkey && NULL != key && 0 < nvalue && NULL != value);
    LOG_II("+r_set\n");
    int sfd = client_init(host,port,block,nkey,key);
    if(-1 == sfd){
        LOG_E("client init failed!\n");
        return -1;
    }
    protocol_binary_request_header* header = calloc(1,sizeof(protocol_binary_request_header));
	assert(NULL != header);
    header->request.magic = PROTOCOL_BINARY_REQ;
    
    header->request.keylen = htons(nkey);
#ifdef TEST_CASE_MEMCACHED
    header->request.opcode = PROTOCOL_BINARY_CMD_SET;
    header->request.extlen = 8;
    header->request.bodylen = htonl(nvalue + 8 + nkey);
#else
    header->request.opcode = 's';
    header->request.bodylen = htonl(nvalue);
#endif
    FAILD_CALL(socket_write(sfd,(char*)(header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),-1)
#ifdef TEST_CASE_MEMCACHED
    char ext[8] = {0};
    FAILD_CALL(socket_write(sfd,ext,8,detal,sleeptime),-1)
#endif
    FAILD_CALL(socket_write(sfd,key,nkey,detal,sleeptime),-1)
    FAILD_CALL(socket_write(sfd,value,nvalue,detal,sleeptime),-1)
    close(sfd);
    free(header);
    LOG_II("-r_set\n");
    return 0;
}

int r_expand(char* host, int port, const size_t size, int detal, int sleeptime, int block){
    LOG_I("+r_expand\n");
    int sfd = client_init(host,port,block,0,NULL);
    if(-1 == sfd){
        LOG_E("client init failed!\n");
        return -1;
    }
    protocol_binary_request_header* header = calloc(1,sizeof(protocol_binary_request_header));
	assert(NULL != header);
    header->request.magic = PROTOCOL_BINARY_REQ;
    header->request.opcode = 'e';
    header->request.bodylen = htonl(size);
    FAILD_CALL(socket_write(sfd,(char*)(header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),-1)
    close(sfd);
    free(header);
    LOG_I("-r_expand\n");
    return 0;
}
int r_lru(char* host, int port, int min, int max,int recycle_count, int interval, int switcher,int detal, int sleeptime,  int block){
    LOG_I("+r_lru\n");
    int sfd = client_init(host,port,block,0,NULL);
    if(-1 == sfd){
        LOG_E("client init failed!\n");
        return -1;
    }
    protocol_binary_request_header* header = calloc(1,sizeof(protocol_binary_request_header));
	assert(NULL != header);
    header->request.magic = PROTOCOL_BINARY_REQ;
    header->request.opcode = 'r';
    header->request.reserved = htons(switcher);
    header->request.bodylen = htonl(min);
    header->request.opaque = htonl(max);
    header->request.keylen = htons(recycle_count);
    header->request.cas = htonl(interval);
    FAILD_CALL(socket_write(sfd,(char*)(header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),-1)
    close(sfd);
    free(header);
    LOG_I("-r_lru\n");
    return 0;

}

int r_stats(char* host, int port, int full, int detal, int sleeptime, int block){
    LOG_II("+r_stats\n");
    int sfd = client_init(host,port,block,0,NULL);
    if(-1 == sfd){
        LOG_E("client init failed!\n");
        return -1;
    }
    protocol_binary_request_header* header = calloc(1,sizeof(protocol_binary_request_header));
	assert(NULL != header);
    header->request.magic = PROTOCOL_BINARY_REQ;
    header->request.opcode = 'l';
    FAILD_CALL(socket_write(sfd,(char*)(header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),-1)
    protocol_binary_response_header* res_header = calloc(1,sizeof(protocol_binary_response_header));
    assert(NULL != res_header);
    FAILD_CALL_6(socket_read(sfd,(char*)(res_header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),-1);
    if(PROTOCOL_BINARY_RES != res_header->response.magic){
        LOG_E("error magic code = %c\n",res_header->response.magic);
        close(sfd);
        free(header);
        free(res_header);
        return -1;
    }
    res_header->response.bodylen = ntohl(res_header->response.bodylen);
    char* value_got = calloc(1, res_header->response.bodylen + 1);
    FAILD_CALL_5(socket_read(sfd,value_got,res_header->response.bodylen ,detal,sleeptime),-1)
    close(sfd);
    value_got[res_header->response.bodylen] = '\0';
    if(full){
        printf("%s",value_got);
    }else{
        printf"%s",strstr(value_got,"slab f_size"));
    }
    free(header);
    free(res_header);
    free(value_got);
    LOG_II("-r_stats\n");
    return 0;
}


int r_mem(char* host, int port, int detal, int sleeptime, int block){
    LOG_I("+r_expand\n");
    int sfd = client_init(host,port,block,0,NULL);
    if(-1 == sfd){
        LOG_E("client init failed!\n");
        return -1;
    }
    protocol_binary_request_header* header = calloc(1,sizeof(protocol_binary_request_header));
	assert(NULL != header);
    header->request.magic = PROTOCOL_BINARY_REQ;
    header->request.opcode = 'm';
    FAILD_CALL(socket_write(sfd,(char*)(header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),-1)
    close(sfd);
    free(header);
    LOG_I("-r_expand\n");
    return 0;
}


int r_del(char* host, int port, const uint32_t nkey, const char* key, int detal, int sleeptime, int block){
    assert(0 < nkey && NULL != key);
    LOG_I("+r_del\n");
    int sfd = client_init(host,port,block,nkey,key);
    if(-1 == sfd){
        LOG_E("client init failed!\n");
        return -1;
    }
    protocol_binary_request_header* header = calloc(1,sizeof(protocol_binary_request_header));
	assert(NULL != header);
    header->request.magic = PROTOCOL_BINARY_REQ;
    header->request.opcode = 'd';
    header->request.keylen = htons(nkey);
    FAILD_CALL(socket_write(sfd,(char*)(header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),-1)
    FAILD_CALL(socket_write(sfd,key,nkey,detal,sleeptime),-1)
    close(sfd);
    free(header);
    LOG_I("-r_del\n");
    return 0;
}
//#define TEST_USE_PREALLOC
static char* r_get_long(int sfd, size_t* size_prealloced __attribute__((unused)), char** value_prealloced __attribute__((unused)), const uint32_t nkey, const char* key, int* nvalue, int detal, int sleeptime, int* error){
    protocol_binary_request_header* header = calloc(1,sizeof(protocol_binary_request_header));
	assert(NULL != header);
    header->request.magic = PROTOCOL_BINARY_REQ;
    header->request.keylen = htons(nkey);
    
#ifdef TEST_CASE_MEMCACHED
    header->request.opcode = PROTOCOL_BINARY_CMD_GETK;
    header->request.bodylen = htonl(nkey);
#else
    header->request.opcode = 'g';
#endif
    FAILD_CALL_2(socket_write(sfd,(char*)(header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),NULL);
    FAILD_CALL_2(socket_write(sfd,key,nkey,detal,sleeptime),NULL)
    protocol_binary_response_header* res_header = calloc(1,sizeof(protocol_binary_response_header));
	assert(NULL != res_header);
    FAILD_CALL_3(socket_read(sfd,(char*)(res_header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),NULL);
    if(PROTOCOL_BINARY_RES != res_header->response.magic){
        LOG_E("error magic code = 0x%x BUG at xcache server!\n",res_header->response.magic);
		free(header);
		free(res_header);
		*nvalue = 0;
        *error = 1;
		return NULL;
    }
    res_header->response.keylen = ntohs(res_header->response.keylen);
    res_header->response.bodylen = ntohl(res_header->response.bodylen);
    if(0 == res_header->response.keylen){
        LOG_E("keylen is 0, BUG at xcache server! \n");
		free(header);
		free(res_header);
        *error = 1;
		*nvalue = 0;
		return NULL;
    }
#ifdef TEST_CASE_MEMCACHED
    if(0 != res_header->response.extlen){

        char ext_got[res_header->response.extlen + 1];
        FAILD_CALL_3(socket_read(sfd,ext_got,res_header->response.extlen,detal,sleeptime),NULL)
        ext_got[res_header->response.extlen + 1] = 0;
    }
#endif
    char key_got[res_header->response.keylen + 1];
    FAILD_CALL_3(socket_read(sfd,key_got,res_header->response.keylen,detal,sleeptime),NULL)
    key_got[res_header->response.keylen + 1] = 0;

#ifdef TEST_CASE_MEMCACHED
    uint32_t value_len = res_header->response.bodylen - res_header->response.keylen - res_header->response.extlen;
    if(0 == value_len){
        *nvalue = 0;
        return NULL;
    }
#else
    uint32_t value_len = res_header->response.bodylen;
#endif
    if(0 == value_len){
        LOG_E("value_len = 0\n");
        free(header);
        free(res_header);
        *nvalue = 0;
        return NULL;
    }

#ifndef TEST_USE_PREALLOC
    char* value_got = malloc(value_len + 1);
	assert(NULL != value_got);
#else
    if(value_len + 1 > *size_prealloced){
       *size_prealloced = value_len + 1;
       LOG_II("*size_prealloced = %lu\n",*size_prealloced);
       *value_prealloced = realloc(*value_prealloced,*size_prealloced) ;
       assert(NULL != *value_prealloced);  
    }
    memset(*value_prealloced,0,*size_prealloced);
    char* value_got = *value_prealloced;
	
#endif    
    FAILD_CALL_4(socket_read(sfd,value_got,value_len ,detal,sleeptime),NULL)
    value_got[value_len] = '\0';
    LOG_II("key = %s, value = %s\n",key_got,value_got);
    LOG_II("-r_get\n");
    *nvalue = value_len;
    free(header);
    free(res_header);
    return value_got;
}


static char* _r_get_keys(uint64_t* keys_count, char* f_host, int f_port, int reset, int detal, int sleeptime, int block){
    LOG_I("+_r_get_keys\n");
    int sfd = client_init(f_host,f_port,block,0,NULL);
    if(-1 == sfd){
        LOG_E("client init failed!\n");
        return NULL;
    }
    protocol_binary_request_header* header = calloc(1,sizeof(protocol_binary_request_header));
    assert(NULL != header);
    header->request.magic = PROTOCOL_BINARY_REQ;
    header->request.opcode = 'b';
    header->request.keylen = htons(reset);
    FAILD_CALL(socket_write(sfd,(char*)(header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),NULL)
    protocol_binary_response_header* res_header = calloc(1,sizeof(protocol_binary_response_header));
    assert(NULL != res_header);
    FAILD_CALL_6(socket_read(sfd,(char*)(res_header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),NULL);
    if(PROTOCOL_BINARY_RES != res_header->response.magic){
        LOG_E("error magic code = %c\n",res_header->response.magic);
        close(sfd);
        free(header);
        free(res_header);
        return NULL;
    }
    res_header->response.cas = be64toh(res_header->response.cas);
    char* value_got = malloc(res_header->response.cas + 1);
    FAILD_CALL_5(socket_read(sfd,value_got,res_header->response.cas ,detal,sleeptime),NULL)
    close(sfd);
    value_got[res_header->response.cas] = '\0';
    free(header);
    free(res_header);
    LOG_I("-_r_get_keys\n");
    *keys_count = be64toh(*(uint64_t*)(value_got));
    value_got += sizeof(uint64_t);
    return value_got;

}
static int _r_del(char* keys, uint64_t keys_count, char* host, int port, int detal, int sleeptime, int block){
        size_t size = sizeof(uint16_t);
        uint64_t count = 0;
        int sfd = client_init(host,port,block,0,NULL);
        if(-1 == sfd){
            LOG_E("sfd client init failed!\n");
            return -1;
        }
        protocol_binary_request_header* header = calloc(1,sizeof(protocol_binary_request_header));
        assert(NULL != header);
        header->request.magic = PROTOCOL_BINARY_REQ;
        header->request.opcode = 'd';
        while(keys_count > 0){
            uint16_t nkey = *((uint16_t*)(keys));
            nkey = ntohs(nkey);
            assert(0 != nkey);
            char key[nkey + 1];
            memcpy(key, keys + size, nkey);
            key[nkey] = '\0';
            -- keys_count;
            keys += (size + nkey);
            LOG_I("%lu nkey = %d, key = %s\n",count,nkey, key);
            header->request.keylen = htons(nkey);
            FAILD_CALL(socket_write(sfd,(char*)(header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),-1)
            FAILD_CALL(socket_write(sfd,key,nkey,detal,sleeptime),-1)  
            count ++;
        }
        close(sfd);
        free(header);
        LOG_I("delete from %s:%d [count:%lu]\n",host,port,count);
        return 0;


}
static void _r_store(char* keys, uint64_t keys_count, char* f_host, int f_port, char* t_host, int t_port, int detal, int sleeptime, int block){
    size_t size = sizeof(uint16_t);
    uint64_t count_ok = 0;
    uint64_t count_failed = 0;
    int f_sfd = client_init(f_host,f_port,block,0,NULL);
    if(-1 == f_sfd){
        LOG_E("f_sfd client init failed!\n");
        return;
    }
    int t_sfd = client_init(t_host,t_port,block,0,NULL);
    if(-1 == t_sfd){
        LOG_E("t_sfd client init failed!\n");
        return;
    }
 #ifndef TEST_USE_PREALLOC
    size_t size_prealloced = 0;
    char* value_prealloced = NULL;
 #else
    size_t size_prealloced = 1024 * 1024 * 5;
    char* value_prealloced = malloc(size_prealloced);
    assert(NULL != value_prealloced);
 #endif
    while(keys_count > 0){
        uint16_t nkey = *((uint16_t*)(keys));
        nkey = ntohs(nkey);
        assert(0 != nkey);
        char key[nkey + 1];
        memcpy(key, keys + size, nkey);
        key[nkey] = '\0';
        -- keys_count;
        keys += (size + nkey);
        if(0 == keys_count % 100) LOG_I("%lu nkey = %d, key = %s\n",count_failed + count_ok,nkey, key);
        int nvalue = 0;
        int error = 0;
        char* value = r_get_long(f_sfd, &size_prealloced,&value_prealloced,nkey, key, &nvalue, detal, sleeptime, &error);
        LOG_II("size_prealloced = %lu error = %d\n",size_prealloced,error);
        if(0 != error){
            #ifndef TEST_USE_PREALLOC
            if(NULL != value) free(value);
            #endif      
            LOG_E("the server may be bugs!\n");
            count_failed += keys_count;
            break;
        }
        if(NULL != value && 0 != nvalue){
            if(0 != r_set_long(t_sfd,nkey,key,nvalue,value,detal, sleeptime)){
                LOG_W("r_set error!\n");
                count_failed ++;
            }else{
                count_ok ++;
            }
            #ifndef TEST_USE_PREALLOC
            free(value);
            #endif
        }else{
            count_failed ++;
        }


    }
    close(f_sfd);
    close(t_sfd);
    #ifdef TEST_USE_PREALLOC
    free(value_prealloced);
    LOG_I("size_prealloced = %lu\n",size_prealloced);
    #endif
    LOG_I("backup from %s:%d to %s:%d [ok:%lu failed:%lu]\n",f_host,f_port,t_host,t_port,count_ok, count_failed);
}
#define TEST_BACKUP_THREAD
#ifdef TEST_BACKUP_THREAD
typedef struct __restore_data{
    char* keys;
    uint64_t keys_t_count;
    char* f_host;
    int f_port;
    char* t_host;
    int t_port;
    int detal;
    int sleeptime;
    int block;
    
}_restore_data;



static void *backup_thread(void *arg){
    _restore_data* data = (_restore_data*)arg;
    assert(NULL != data);
#ifndef TEST_CASE_DELETE_ALL
    _r_store(data->keys,data->keys_t_count,data->f_host,data->f_port,data->t_host,data->t_port,data->detal,data->sleeptime,data->block);
#else
    _r_del(data->keys,data->keys_t_count,data->f_host,data->f_port,data->detal,data->sleeptime,data->block);
#endif
    return NULL;

}
#endif

void r_backup(char* f_host, int f_port, char* t_host, int t_port, uint32_t reset, uint32_t thread_num, int detal, int sleeptime, int block){
    uint64_t keys_count = 0;
    char* keys = _r_get_keys(&keys_count,f_host, f_port, reset, detal, sleeptime, block);
    
    
    if((NULL == keys) || (0 == keys_count)){
        LOG_W("no keys to backup on %s:%d\n",f_host,f_port);
        return;
    }
    char* keys_to_free = keys - sizeof(uint64_t);
    LOG_I("keys_count = %lu\n",keys_count);
#ifndef TEST_BACKUP_THREAD
    _r_store(keys, keys_count,f_host, f_port, t_host, t_port, detal, sleeptime, block);
#else
    if(thread_num >= keys_count){
        _r_store(keys, keys_count,f_host, f_port, t_host, t_port, detal, sleeptime, block);
        free(keys_to_free);
        return;
    }
    uint64_t split = keys_count/thread_num;
    pthread_t thread_id_restore[thread_num];
    _restore_data data[thread_num];
    uint32_t index = 0;
    uint64_t total = 0;
    uint64_t keys_t_count = 0;
    while(index < thread_num){
        
        if((thread_num - 1) == index){
            keys_t_count = keys_count - total;
        }else{
            keys_t_count = split;
        }
        total += split;
        
        data[index].keys = keys;
        data[index].keys_t_count = keys_t_count;
        data[index].f_host = f_host;
        data[index].f_port = f_port;
        data[index].t_host = t_host;
        data[index].t_port = t_port;
        data[index].detal = detal;
        data[index].sleeptime = sleeptime;
        data[index].block = block;
        
        while(keys_t_count > 0){
            uint16_t nkey = *((uint16_t*)(keys));
            nkey = ntohs(nkey);
            assert(0 != nkey);
            keys += (sizeof(uint16_t) + nkey);
            -- keys_t_count;
                
        }
        ++ index;  
    }
    index = 0;
    while(index < thread_num){
        pthread_attr_t  attr;
        pthread_attr_init(&attr);
        if(0 != pthread_create(&thread_id_restore[index],&attr,backup_thread,&data[index])){
                LOG_E("pthread_created failed! errno = %d : %s\n",errno,strerror(errno));
                exit(EXIT_FAILURE);
        }
        ++ index;
    }
   
   uint32_t i = 0;
   for(; i < thread_num; ++i){
        pthread_join(thread_id_restore[i],NULL);
   }
#endif
   free(keys_to_free);
}





char* r_get(char* host, int port, const uint32_t nkey, const char* key, int* nvalue, int detal, int sleeptime, int block){
    assert(0 < nkey && NULL != key && NULL != nvalue);  
    
    LOG_II("+r_get\n");
    int sfd = client_init(host,port,block,nkey,key);
    if(-1 == sfd){
        LOG_E("client init failed!\n");
        return NULL;
    }
    protocol_binary_request_header* header = calloc(1,sizeof(protocol_binary_request_header));
	assert(NULL != header);
    header->request.magic = PROTOCOL_BINARY_REQ;
    header->request.keylen = htons(nkey);
    
#ifdef TEST_CASE_MEMCACHED
    header->request.opcode = PROTOCOL_BINARY_CMD_GETK;
    header->request.bodylen = htonl(nkey);
#else
    header->request.opcode = 'g';
#endif
    FAILD_CALL_2(socket_write(sfd,(char*)(header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),NULL);
    FAILD_CALL_2(socket_write(sfd,key,nkey,detal,sleeptime),NULL)
    protocol_binary_response_header* res_header = calloc(1,sizeof(protocol_binary_response_header));
	assert(NULL != res_header);
    FAILD_CALL_3(socket_read(sfd,(char*)(res_header->bytes),sizeof(protocol_binary_request_header),detal,sleeptime),NULL);
    if(PROTOCOL_BINARY_RES != res_header->response.magic){
        LOG_E("error magic code = %c\n",res_header->response.magic);
        close(sfd);
		free(header);
		free(res_header);
		*nvalue = 0;
		return NULL;
    }
    res_header->response.keylen = ntohs(res_header->response.keylen);
    res_header->response.bodylen = ntohl(res_header->response.bodylen);
    if(0 == res_header->response.keylen || 0 == res_header->response.bodylen){
        if(0 == res_header->response.keylen) { LOG_E("keylen == 0, may be some bug!!\n");}
        if(0 == res_header->response.bodylen) { LOG_II("key[%s] is not found!\n");}
        close(sfd);
		free(header);
		free(res_header);
		*nvalue = 0;
		return NULL;
    }
#ifdef TEST_CASE_MEMCACHED
    if(0 != res_header->response.extlen){

        char ext_got[res_header->response.extlen + 1];
        FAILD_CALL_3(socket_read(sfd,ext_got,res_header->response.extlen,detal,sleeptime),NULL)
        ext_got[res_header->response.extlen + 1] = 0;
    }
#endif
    char key_got[res_header->response.keylen + 1];
    FAILD_CALL_3(socket_read(sfd,key_got,res_header->response.keylen,detal,sleeptime),NULL)
    key_got[res_header->response.keylen + 1] = 0;

#ifdef TEST_CASE_MEMCACHED
    uint32_t value_len = res_header->response.bodylen - res_header->response.keylen - res_header->response.extlen;
    if(0 == value_len){
        *nvalue = 0;
        return NULL;
    }
#else
    uint32_t value_len = res_header->response.bodylen;
#endif
    char* value_got = calloc(1, value_len + 1);
	assert(NULL != value_got);
    
    FAILD_CALL_4(socket_read(sfd,value_got,value_len ,detal,sleeptime),NULL)
    LOG_II("key = %s, value = %s\n",key_got,value_got);
    close(sfd);
    LOG_II("-r_get\n");
    *nvalue = value_len;
    free(header);
    free(res_header);
    return value_got;
    
}




