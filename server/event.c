/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "event.h"
#include "log.h"

#include <sys/epoll.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>






#define EVENT_MAX_ID 4028
#define EVENT_MIN_ID 32

#ifdef EVENT_DEBUG
#define EVENT_LOG_I LOG_I
#else
#define EVENT_LOG_I(f_, ...)

#endif


event_base* event_init(){
    event_base* p = malloc(sizeof(event_base));
    assert(NULL != p);
    p->epfd = epoll_create1(0);
    p->timeout = -1;
    assert(0 <= p->epfd);
    return p;
}


int event_add(event_base* event_base, void* arg){
    struct epoll_event ep_event;
    event* ev = (event*)arg;
    assert(NULL != event_base && NULL != ev);
    ep_event.events = ev->event_flags;
    ep_event.data.ptr = arg;
    int ret = epoll_ctl(event_base->epfd,EPOLL_CTL_ADD,ev->fd,&ep_event);
    if(-1 == ret){
        LOG_E("EPOLL_CTL_ADD[%d:%d] failed!\n",event_base->epfd,ev->fd);
        perror("epoll_ctl():EPOLL_CTL_ADD");
    }else{
        EVENT_LOG_I("EPOLL_CTL_ADD[%d:%d] OK \n",event_base->epfd,ev->fd);
    }
    return ret;
}


int event_mod(event_base* event_base, void* arg){
    struct epoll_event ep_event;
    event* ev = (event*)arg;
    assert(NULL != event_base && NULL != ev);
    ep_event.events = ev->event_flags;
    ep_event.data.ptr = arg;
    int ret = epoll_ctl(event_base->epfd,EPOLL_CTL_MOD,ev->fd,&ep_event);
    if(-1 == ret){
        LOG_E("EPOLL_CTL_MOD[%d:%d] failed!\n",event_base->epfd,ev->fd);
        perror("epoll_ctl():EPOLL_CTL_MOD");
    }else{
        EVENT_LOG_I("EPOLL_CTL_MOD[%d:%d] OK \n",event_base->epfd,ev->fd);
    }
    return ret;
}

int event_delete(event_base* event_base, void* arg){
    event* ev = (event*)arg;
    assert(NULL != event_base && NULL != ev);
    int ret = epoll_ctl(event_base->epfd,EPOLL_CTL_DEL,ev->fd,NULL);
    if(-1 == ret){
        LOG_E("EPOLL_CTL_DEL[%d:%d] failed!\n",event_base->epfd,ev->fd);
        perror("epoll_ctl():EPOLL_CTL_DEL");
    }else{
        EVENT_LOG_I("EPOLL_CTL_DEL[%d:%d] OK \n",event_base->epfd,ev->fd);
    }
    return ret;
}

int event_clear(int fd_to_clear, void* event_ptr, int len){
    assert(fd_to_clear >= 0 && NULL != event_ptr && len > 0);
    int i = 0;
    int got = 0;
    while(i < len){
        struct epoll_event* ep_event = event_ptr;
        event* ev = (event*)ep_event[i].data.ptr;
        if(NULL == ev){
            LOG_W("get the another event, just pass it\n");
        }else if(ev->fd == fd_to_clear){
            ep_event[i].data.ptr = NULL;
            LOG_W("get the clear fd[%d] event, just ignore it\n",fd_to_clear);
            ++ got;
        }
        ++ i;
    }
    return got;
    
}
int event_base_loop(const event_base* event_base){
    assert(NULL != event_base);
    //struct epoll_event ep_event[EVENT_MAX_ID];
    struct epoll_event* ep_event = calloc(1, EVENT_MIN_ID * sizeof(struct epoll_event));
    int ep_event_size = EVENT_MIN_ID;
    int ret = -1;
    int i;
    while(1){
        ret = epoll_wait(event_base->epfd,ep_event,ep_event_size,event_base->timeout);
        if(-1 == ret){
            if(EINTR == errno){
                LOG_W("EINTR at epfd[%d]\n",event_base->epfd);
                continue;
            }
            LOG_E("epoll_wait errno = %d EINTR = %d, EBADF = %d EFAULT = %d EINVAL = %d \n",errno,EINTR,EBADF,EFAULT,EINVAL);
            perror("epoll_wait()\n");
            // BUG GOT!!
            assert(ret == 0);
            return ret;
        }
        for(i = 0; i < ret; ++i){
            LOG_I("ret = %d\n",ret);

            if((EPOLLERR & ep_event[i].events) || (EPOLLHUP & ep_event[i].events)){
                LOG_E("epoll_wait errno = %d EPOLLERR = %d, EPOLLHUP = %d EPOLLRDHUP = %d\n",errno,EPOLLERR,EPOLLHUP,EPOLLRDHUP);
                EVENT_LOG_I("server will detect by reading or writing and close client socket!\n");
                
            }
            if((EPOLLRDHUP & ep_event[i].events)){
                LOG_E("EPOLLRDHUP : epoll_wait errno = %d EPOLLERR = %d, EPOLLHUP = %d EPOLLRDHUP = %d\n",errno,EPOLLERR,EPOLLHUP,EPOLLRDHUP);
                EVENT_LOG_I("server will delete and close client socket!\n");
                //FIX ME: should define a error handler for each client
            }
            
            if(ret == EVENT_MAX_ID){
                LOG_W("must expand ep_event!\n");
            }
            event* ev = (event*)ep_event[i].data.ptr;
            if(NULL != ev){
                ev->handler(ev,ep_event,ret);
            }else{
                LOG_W("just ignore the clearing event!\n");
            }
            
            
        }
        if(ret == ep_event_size){
            ep_event_size = (2 * ep_event_size) > EVENT_MAX_ID? EVENT_MAX_ID : 2 * ep_event_size;
            ep_event = realloc(ep_event,ep_event_size * sizeof(struct epoll_event));
            assert(NULL != ep_event);
            LOG_W("expand the ep_event_size to %d\n",ep_event_size);
        }
    }
    return ret;

}

void event_base_free(event_base* event){
    assert(NULL != event);
    close(event->epfd);
    free(event);
}



