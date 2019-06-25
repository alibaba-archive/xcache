/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "worker.h"
#include "log.h"
#include "config.h"
#include "connect.h"
#include "stats.h"
#include "clock.h"


#include <sys/epoll.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>


//#define WORKER_DEBUG

#ifdef WORKER_DEBUG
#define WORKER_LOG_I LOG_I
#else
#define WORKER_LOG_I(f_, ...)

#endif

#define WORK_MAX_ID   10
#define PIPE_READ 0
#define PIPE_WRITE 1

#define LIST_DELETE 0
#define LIST_ADD 1
#define LIST_CLEAR 3


extern config g_config;
extern stats g_stats;


static worker* wklist = NULL;
static int id = 0;
static void *thread_worker(void *arg);
static void worker_handler(void *arg, void* ep_event, int len);
typedef struct __worker_cond{
    pthread_cond_t cond;
    int value;

} _worker_cond;
static _worker_cond cond;
static pthread_mutex_t lock;



static void wait_run(int once){
    pthread_mutex_lock(&lock);
    while(WORK_MAX_ID > cond.value){
        LOG_I("pthread_cond_wait\n");
        pthread_cond_wait(&cond.cond, &lock);
    }
    if(1 == once) cond.value = 0;
    pthread_mutex_unlock(&lock);
}

static void signal_run(int on){
    pthread_mutex_lock(&lock);
    LOG_I("cond[0].value = %d, on = %d\n", cond.value , on);
    cond.value += on;
    pthread_cond_signal(&cond.cond);
    pthread_mutex_unlock(&lock);
}
void worker_run_recycle(){
    signal_run(WORK_MAX_ID);
}

static void *thread_worker(void *arg){
    worker* w = (worker*)arg;
    assert(NULL != w);
    if(0 != event_add(w->ev_base,w)){
        perror("event_add()");
        LOG_I("worker_looper created failed!\n");
        return NULL;
    }
    LOG_I("worker_looper created!\n");
    signal_run(1);
    event_base_loop(w->ev_base);
    event_base_free(w->ev_base);
    LOG_I("worker_looper exit!\n");
    return NULL;
}

static int worker_debug_has(con* c, con* head, con* tail, int has, time_t now, int* existed){
    if(0 != now){
        assert((NULL == c) && (NULL != existed));
    }else{
        assert(NULL != c);
    }
    int count_from_head = 0;
    int count_from_tail = 0;
    int existed_from_head = 0;
    int existed_from_tail = 0;
    while(NULL != head){
        if(!has){
            if(0 != now){
                assert(!(now - head->time > g_config.worker_recycle_interval && !head->state));
            }else{
                assert(c != head);
            }
        }else{
            if(0 != now){
                if(now - head->time > g_config.worker_recycle_interval && !head->state){
                    ++ existed_from_head;
                }  
            }else{
                if(c == head){
                    ++ existed_from_head;
                    assert(1 >= existed_from_head);
                }
            }
        }
        head = head->next;
        count_from_head ++;
    }
    while(NULL != tail){
        if(!has){
            if(0 != now){
                assert(!(now - tail->time > g_config.worker_recycle_interval && !tail->state));
            }else{
                assert(c != tail);
            }
                
        }else{
            if(0 != now){
                if(now - tail->time > g_config.worker_recycle_interval && !tail->state){
                    ++ existed_from_tail;
                }
            }else{
                if(c == tail){
                    ++ existed_from_tail;
                    assert(1 >= existed_from_tail);
                }
            }
        }
        tail = tail->prev;
        count_from_tail ++;
    }
    assert(existed_from_tail == existed_from_head);
    if(0 != now) *existed = existed_from_tail;
    assert(count_from_tail == count_from_head);
    return count_from_head;
}

static void op_list(int op, con* c, con** head, con** tail, time_t now, void* ep_event, int len){
    WORKER_LOG_I("op = %d\n",op);
    if(LIST_ADD == op){
        
        assert(NULL != c);
        WORKER_LOG_I("add fd[%d] to worker!\n",c->ev.fd);
#ifdef WORKER_DEBUG
     int before = worker_debug_has(c,*head,*tail,0,0,NULL);
#endif
        c->next = *head;
        c->prev = NULL;
        if(c->next) c->next->prev = c;
        *head = c;
        if(NULL == *tail) *tail = *head;
#ifdef WORKER_DEBUG
     int after = worker_debug_has(c,*head,*tail,1,0,NULL);
     assert((after - 1) == before);
#endif

    }else if(LIST_DELETE == op){
    
        assert(NULL != c);
        WORKER_LOG_I("delete fd[%d] from worker!\n",c->ev.fd);
#ifdef WORKER_DEBUG
       int before = worker_debug_has(c,*head,*tail,1,0,NULL);
#endif
       if(*tail == *head){
        if(c == *tail){
            *tail = *head = NULL;
        }
        }else if(*tail == c){
            *tail = c->prev;
            if(c->prev) c->prev->next = NULL;
        }else if(*head == c){
            *head = c->next;
            if(c->next) c->next->prev = NULL;
        }else{
            if(c->prev) c->prev->next = c->next;
            if(c->next) c->next->prev = c->prev;
        }
        c->next = c->prev = NULL;
#ifdef WORKER_DEBUG
        int after = worker_debug_has(c,*head,*tail,0,0,NULL);
        assert((after + 1) == before);
#endif
    }else if(LIST_CLEAR == op){
#ifdef WORKER_DEBUG
         WORKER_LOG_I("+clear\n");
         int existed_before = 0;
         int before = worker_debug_has(NULL,*head,*tail,1,now,&existed_before);
#endif
        con* it = *head;
        while(NULL != it){
            con* next = it->next;
            LOG_W("check fd[%d,%lu] in worker, compare to %lu!\n",it->ev.fd, it->time, now);
            if(now - it->time > g_config.worker_recycle_interval && !it->state){
                assert(9 != it->time);
                LOG_W("recycle fd[%d] time[%lu]\n",it->ev.fd,it->time);
                if(*head == *tail){
                    *head = *tail = NULL;
                }else if(*head == it){
                    *head = it->next;
                    if(it->next) it->next->prev = NULL;
                }else if(*tail == it){
                    *tail = it->prev;
                    if(it->prev) it->prev->next = NULL;
                }else{
                    if(it->next) it->next->prev = it->prev;
                    if(it->prev) it->prev->next = it->next;
                }
                it->prev = it->next = NULL;
                worker* w = it->w;
                assert(NULL != w);
                event_delete(w->ev_base,it);
                // the it->ev.fd may already in the epoll events and will be handle by connect. we should remove it!
                int got = event_clear(it->ev.fd,ep_event,len);
                WORKER_LOG_I("got = %d\n",got);
                assert(1 >= got);
                con_delete(it);
                stats_lock();
                g_stats.fd_cleared ++;
                stats_unlock();
            }
            it = next;
        }
#ifdef WORKER_DEBUG
         int existed_after = 0;
         int after = worker_debug_has(NULL,*head,*tail,0,now,&existed_after);
         assert(0 == existed_after);
         assert((existed_before - existed_after) == (before - after));
         WORKER_LOG_I("-clear\n");
#endif
    }
}





static void worker_handler(void *arg, void* ep_event, int len){
    worker* w = (worker*)arg;
    char cmd[1] = {0};
    assert(NULL != w);
    if(1 != read(w->pipefd[PIPE_READ],cmd,1)){
        LOG_W("worker_handler will read again\n");
        return;
    }
    LOG_I("worker_handler get cmd = %c\n",cmd[0]);
    if('c' == cmd[0]){
        pthread_mutex_lock(&w->lock);
        if(NULL != w->con_tail){
            con* c = w->con_tail;
            WORKER_LOG_I("add the new client(%d) to epool\n",w->con_tail->ev.fd);
            if(0 != event_add(w->ev_base,w->con_tail)){
                LOG_E("add the new client(%d) to epool failed!, may be the client shutdown!\n",w->con_tail->ev.fd);
                op_list(LIST_DELETE,c, &w->con_head, &w->con_tail,0,NULL,0);
                con_delete(c);
                pthread_mutex_unlock(&w->lock);
                return;
            }
            op_list(LIST_DELETE,c, &w->con_head, &w->con_tail,0,NULL,0);
            op_list(LIST_ADD,c, &w->con_head_used, &w->con_tail_used,0,NULL,0);
            
        }
        pthread_mutex_unlock(&w->lock);
        
     }else if('r' == cmd[0]){
        op_list(LIST_CLEAR,NULL,&w->con_head_used, &w->con_tail_used,get_current_time_sec(),ep_event, len);
     }
    return;
}

static void* thread_looper_clear(void *arg){
    assert(NULL == arg);
    wait_run(1);
    LOG_I("wait for worker thread init ok\n");
    while(1){
        wait_run(1);
        LOG_W("may be some bad client exsited! begin to clear!\n");
        int i = WORK_MAX_ID - 1;
        while(i >= 0){
            if(1 != write(wklist[i].pipefd[PIPE_WRITE],"r",1)){
                perror("write()");
                LOG_E("Failed to tell worker[%d] to recycle fd\n",i);
            }
            -- i;
            
        }
    }   
}

void worker_clear(){
     if(g_config.enable_worker_recycle){
        pthread_attr_t  attr;
        pthread_attr_init(&attr);
        pthread_t thread_id;
        if(0 != pthread_create(&thread_id,&attr,thread_looper_clear,NULL)){
                perror("pthread_create() recycle_fd");
                exit(EXIT_FAILURE);
        }
    }
}
void worker_init(){
    int i = 0;
    if(NULL == wklist){
        wklist = calloc(WORK_MAX_ID,sizeof(worker));
        assert(NULL != wklist);
    }
    for(; i < WORK_MAX_ID; ++i){
        pthread_attr_t  attr;
        pthread_attr_init(&attr);
        wklist[i].pipefd = calloc(2,sizeof(int)); 
        if(pipe(wklist[i].pipefd)){
            perror("pipe error");
            exit(EXIT_FAILURE);
        }
        wklist[i].ev_base = event_init();
        wklist[i].event.fd = wklist[i].pipefd[PIPE_READ];
        wklist[i].event.event_flags = EPOLLIN;
        wklist[i].event.handler = worker_handler;
        pthread_mutex_init(&wklist[i].lock,NULL);
        
        if(0 != pthread_create(&(wklist[i].thread_id),&attr,thread_worker,&wklist[i])){
            perror("pthread_create()");
            exit(EXIT_FAILURE);
        }
        WORKER_LOG_I("worker_init(wklist[%d]) worker thread created ok!\n",i);
        
    }
    worker_clear();
       
}



int worker_add(con* c){
    int i = id;
#ifdef WORKER_DEBUG
    LOG_I("id = %d\n",id);
#endif
    assert(NULL != c);
    pthread_mutex_lock(&wklist[i].lock);
    op_list(LIST_ADD,c,&wklist[i].con_head,&wklist[i].con_tail,0,NULL,0);
    pthread_mutex_unlock(&wklist[i].lock);
    c->w = &wklist[i];
    id = (id + 1) % WORK_MAX_ID;
    WORKER_LOG_I("worker_add(%d) wklist[%d]:%lu \n",c->ev.fd,i,wklist[i].thread_id);
    if(1 != write(wklist[i].pipefd[PIPE_WRITE],"c",1)){
        perror("write()");
        LOG_E("Failed to tell worker to handle the fd(%d):write()\n",c->ev.fd);
        event_delete(wklist[i].ev_base,c);
        con_delete(c);
    }
    return 0;
}

void worker_delete(con* con){
    worker* w = con->w;
    WORKER_LOG_I("worker_delete con[%d] and con = %p w = %p\n",con->ev.fd,con,w);
    assert(NULL != con && NULL != w);
    event_delete(w->ev_base,con);
    WORKER_LOG_I("worker_delete con[%d]\n",con->ev.fd);
    op_list(LIST_DELETE,con, &w->con_head_used, &w->con_tail_used,0,NULL,0);
    
    
}
int worker_mod(con* con){
    assert(NULL != con);
    worker* w = con->w;
    assert(NULL != w);
    WORKER_LOG_I("worker_mod con[%d]\n",con->ev.fd);
    return event_mod(w->ev_base,con);
}

