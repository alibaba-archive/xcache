/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "net.h"

#include "config.h"
#include "log.h"
#include "connect.h"
#include "slab.h"
#include "lru.h"


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sysexits.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>



extern config g_config;

static pthread_mutex_t net_mutex = PTHREAD_MUTEX_INITIALIZER;


static void net_handler(void* arg, void* ep_event, int len);

static net* nlist = NULL;
static event_base* ev_base = NULL;

static void peer_client(struct sockaddr* addr){
    assert(NULL != addr);
    int port = 0;
    char ipstr[INET6_ADDRSTRLEN] = {0};
    if (AF_INET == addr->sa_family){
        struct sockaddr_in *s = (struct sockaddr_in *)addr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
    }else { // AF_INET6
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)addr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
    }
    LOG_I("Peer IP address: %s, port = %d\n", ipstr, port);
}
static void net_handler(void* arg, void* ep_event, int len){
    assert(NULL != arg && NULL != ep_event && len > 0);
    net* n = arg;
    struct sockaddr in_addr;
    socklen_t in_len;
    int sfd = -1;
    assert(NULL != n);
    while(1){
        in_len = sizeof(in_addr);
        sfd = accept(n->event.fd,&in_addr,&in_len);
        LOG_I("in_addr sa_family = %d, server_fd= %d \n",in_addr.sa_family,n->event.fd);
        if(-1 == sfd){
            if(EAGAIN == errno || EWOULDBLOCK == errno){
                break;
            }else if(EMFILE == errno){
                LOG_E("too many client coming!\n");
                net_switch(0);
                break;
            }
            LOG_E("accept errno = %d\n",errno);
            perror("accept()");
            net_delete(n);
            break;
        }
        //peer_client(&in_addr);
        if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL) | O_NONBLOCK) < 0){
            perror("fcntl()");
            close(sfd);
            break;
        }
      
        LOG_I("net_handler(%d), already accept a new client!\n",sfd);
        con_add(sfd);
        
    }

}



void net_init(char* host, int port,enum net_type type){
    if(NULL == ev_base){
        ev_base = event_init();
        assert(NULL != ev_base);
        
    }
    struct addrinfo *result,*rp;
    char service[NI_MAXSERV] = {0};
    struct addrinfo hints = { .ai_flags = AI_PASSIVE,
                              .ai_family = AF_UNSPEC,
                              .ai_socktype = (udp == type)? SOCK_DGRAM : SOCK_STREAM};

    snprintf(service, sizeof(service), "%d", port);
    int ret = getaddrinfo(host, service,&hints,&result);
    if(0 != ret){
        if(EAI_SYSTEM == ret){
            perror("net_init:getaddrinfo()");
        }
        else{
            LOG_E("getaddrinfo(): %s\n", gai_strerror(ret));
        }
        event_base_free(ev_base);
        exit(EX_OSERR);
    }
    int inited = 0;
    for(rp = result; NULL != rp; rp = rp->ai_next){
        struct linger ling = {0, 0};
        int flags = 1;
        LOG_I("ai_family = %d, ai_socktype = %d, ai_protocol = %d\n",rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        int sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol); 
        if(0 > sfd){
            perror("socket_int()");
            continue;
        }
        if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL) | O_NONBLOCK) < 0){
            perror("fcntl()");
            close(sfd);
            continue;
        }
        if(0 != setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags))){
            perror("SO_REUSEADDR ignore setsockopt()");
        }
        if(0 != setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags))){
            perror("SO_KEEPALIVE ignore setsockopt()");
        }
        if(0 != setsockopt(sfd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling))){
            perror("SO_LINGER ignore setsockopt()");
        }
        if(0 != setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags))){
            perror("TCP_NODELAY ignore setsockopt()");
        }
        if(-1 == bind(sfd,rp->ai_addr, rp->ai_addrlen)){
            if (EADDRINUSE != errno) {
                perror("bind()");
                close(sfd);
                continue;
            }
            close(sfd);
            perror("bind()");
            continue;
            
        }
        if (!(udp == type) && listen(sfd, g_config.net_backlog) == -1) {
            perror("listen()");
            close(sfd);
            continue;
          }
        net_add(sfd);
        inited = 1;
        LOG_I("net inits ok!\n");
        break;
         

    }
    freeaddrinfo(result);
    if(0 == inited){
        LOG_E("net has been totally inited failed!\n");
        exit(EXIT_FAILURE);
    }
#if 0
// default off until the xcache_moniter call 
    lru_switch(1);

    if(g_config.enable_slab_recycle) slab_switch(1);
#endif
    event_base_loop(ev_base);
    event_base_free(ev_base);
    
}

void net_add(int sfd){
    assert(0 <= sfd);
    net* n = calloc(1,sizeof(net));
    assert(NULL != n);
    n->event.fd = sfd;
    n->event.event_flags = EPOLLIN;
    n->event.handler = net_handler;
    event_add(ev_base,n);
    n->next = nlist;
    if(n->next){
        n->next->prev = n;
    }
    nlist = n;
    
    LOG_I("net_add(%d), begin to accept new client!\n",sfd);
}


void net_delete(net* n){
    assert(NULL != n && NULL != nlist);
    if(n == nlist){
        nlist = NULL;
    }
    else{
        if(n->next){
            n->next->prev = n->prev;
        }
        if(n->prev){
            n->prev->next = n->next;
        } 
        
    }
    event_delete(ev_base,n);
    close(n->event.fd);
}

void net_switch(int on){
    NET_LOCK;
    assert(NULL != nlist);
    net* n = nlist;
    while(NULL != n){
         if(1 <= on){
            n->event.event_flags = EPOLLIN;
         }else{
            n->event.event_flags = 0;
         }
         if(0 != event_mod(ev_base,n)){
            LOG_E("event_mod [%d] failed\n",n->event.event_flags);
         }
         n = n->next;
    }
    NET_UNLOCK;
    LOG_I("net_switch on = %d\n",on);
}
