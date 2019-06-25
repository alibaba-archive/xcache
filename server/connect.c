/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "connect.h"

#include "config.h"
#include "log.h"
#include "event.h"
#include "clock.h"

#include "bklist.h"
#include "slab.h"
#include "lru.h"
#include "stats.h"


#include "worker.h"
#include "protocol.h"
#include "local.h"





#include <sys/time.h>
#include <sys/resource.h>
#include <sys/epoll.h>
#include <sys/uio.h>

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#define _BSD_SOURCE             /* See feature_test_macros(7) */
#include <endian.h>




extern config g_config;
extern stats g_stats;


static con** conlist = NULL;
static int con_max_id = 500;
static void con_handler(void *arg,void* ep_event, int len);
static void con_handler_fake(void* arg, void* ep_event, int len) __attribute__((unused));

static void con_adjust(con* c);
static void con_reset(con* c);
static void con_pack(con* c, char* buf, int len);
static void con_pack_block(con* c, block* b);


//#define TEST_CASE_FAKE_HANDLE

//#define TEST_CASE_COPY


//#define TEST_CASE_READ_SLEEP  1 * 10 * 10
//#define TEST_CASE_READ_DETAL  100
//#define TEST_CASE_PACK_DETAL  10

#define TEST_CASE_LONG_CONNECT
#ifdef TEST_CASE_LONG_CONNECT
//#define TEST_CLIENT_CLEAR

#endif

#ifdef TEST_CASE_READ_SLEEP
#define READ_UNSLEEP   usleep(TEST_CASE_READ_SLEEP);
#else
#define READ_UNSLEEP   ;
#endif


#ifdef TEST_CASE_READ_DETAL
#define READ(fd, buf, count) \
    int ret = read(fd, buf, TEST_CASE_READ_DETAL > (count)? (count):TEST_CASE_READ_DETAL); \
    READ_UNSLEEP
#else
#define READ(fd, buf, count) \
    int ret = read(fd, buf, count); \
    READ_UNSLEEP

#endif

#ifdef TEST_CASE_PACK_DETAL
static void con_pack_2(con* c, char* buf, int len, int detal);
static void con_pack_2(con* c, char* buf, int len, int detal){
    assert(NULL != c && NULL != buf);
    assert(0 < detal);
    while(len > 0){
        con_pack(c,buf,detal > len? len : detal);
        buf += detal;
        len -= detal;
    }
}
#define CON_PACK(c, buf,len) con_pack_2(c, buf, len, TEST_CASE_PACK_DETAL)
#else
#define CON_PACK(c, buf,len) con_pack(c, buf, len)
#endif




static void con_adjust(con* c){
    assert(NULL != c);
    if(c->r_buf != c->r_curr){
        c->r_curr = memmove(c->r_buf,c->r_curr,c->r_used);
    }
#ifdef TEST_CASE_COPY
    if(c->r_used == c->r_size){
        c->r_size = 2 * c->r_size;
        c->r_curr = c->r_buf = realloc(c->r_buf, c->r_size);
        LOG_I("expand r_size from %d to %d\n",c->r_size/2,c->r_size);
        
    }
#else
    if(c->r_used + c->r_want > c->r_size){
        LOG_I("expand r_size from %d to %d\n",c->r_size,c->r_used + c->r_want);
        c->r_size = c->r_used + c->r_want;
        c->r_curr = c->r_buf = realloc(c->r_buf, c->r_size);  
    }
    assert(c->r_curr == c->r_buf);
    assert(NULL != c->r_buf);
    assert((c->r_size - c->r_used) >= c->r_want);
#endif
}

static void con_pack_block(con* c, block* b){
    assert(NULL != c);
    assert(NULL != b);
    if(0 != (b->flags & BLOCK_FLAGS_HAVE_CHILD)){
        CON_PACK(c,BLOCK_v(b),BLOCK_FATHER_nv(b));
        LOG_B_W(b,"pack father nvalue[%lu]\n",BLOCK_FATHER_nv(b));
        block** child_ptr = (block**)BLOCK_c(b);
        block* child = *child_ptr;
        assert(NULL != child);
        while(NULL != child){
            LOG_B_W(child,"pack child nvalue[%u]\n",child->nvalue);
            CON_PACK(c,BLOCK_CHILD_v(child),child->nvalue);
            child = child->next;
        }
        
    }else{
        CON_PACK(c,BLOCK_v(b),b->nvalue);
    }
}

static void con_pack(con* c, char* buf, int len){
    assert(NULL != c && NULL != buf && 0 < len);
    if(c->iov_used == c->iov_size){
        LOG_I("realloc iovlist: c->iov_size from %d to %d\n",c->iov_size,c->iov_size * 2);
        c->iov_size = 2 * c->iov_size;
        c->iovlist= realloc(c->iovlist,c->iov_size * sizeof(struct iovec));
        assert(NULL != c->iovlist);
        uint32_t i = 0;
        struct iovec* iov = c->iovlist;
        for(; i < c->msg_used + 1; ++i){
            c->msglist[i].msg_iov = iov;
            iov += c->msglist[i].msg_iovlen;
            
        }
    }
    struct msghdr* msg = &(c->msglist[c->msg_used]);
    
    if(IOV_MAX == msg->msg_iovlen){
        LOG_I("alloc a new msg, c->msg_used = %d\n",c->msg_used);
        c->msg_used ++;
        if(c->msg_used == c->msg_size){
            LOG_I("realloc msglist: c->msg_size from %d to %d\n",c->msg_size,c->msg_size * 2);
            c->msg_size = 2 * c->msg_size;
            c->msglist = realloc(c->msglist,c->msg_size * sizeof(struct msghdr));
            assert(NULL != c->msglist);
        }
        msg = &(c->msglist[c->msg_used]);
        assert(NULL != msg);
        // we must set to null, may be the msg is dirty before because of reuse.
        memset(msg, 0, sizeof(struct msghdr));
        
    }
    if(NULL == msg->msg_iov){
        LOG_I("alloc msg_iov to the new msg c->iov_used = %d\n",c->iov_used);
        msg->msg_iov = &(c->iovlist[c->iov_used]);
        assert(0 == msg->msg_iovlen);
    }
    assert(IOV_MAX != msg->msg_iovlen);
    msg->msg_iov[msg->msg_iovlen].iov_base = buf;
    msg->msg_iov[msg->msg_iovlen].iov_len = len;
    msg->msg_iovlen ++;
    c->iov_used ++;
}





static void con_contiune(con* c){
    assert(NULL != c);
    worker* w = c->w;
    con_reset(c);
    c->w = w;
    c->time = get_current_time_sec(); 
}

static void con_close(con* c){
    assert(NULL != c);
    worker_delete(c);
    con_delete(c);
}

static void con_handler_fake(void* arg, void* ep_event __attribute__((unused)), int len __attribute__((unused))){
    con* c = (con*)arg;
    assert(NULL != c);
    worker* w = c->w;
    assert(NULL != w);
    //usleep(100 * 1000);
    LOG_W("con_handler_fake!\n");
    con_close(c);

}

static void con_handler(void* arg, void* ep_event, int len){
    assert(NULL != ep_event && len > 0);
    con* c = (con*)arg;
    assert(NULL != c);
    worker* w = c->w;
    assert(NULL != w);

#ifdef TEST_CASE_COPY
    while(state_pack_value > c->state){
#else
    while(state_parser_value_set_1 > c->state){
#endif
        if(state_new == c->state) c->state = state_parser_header;
        con_adjust(c);
#ifdef TEST_CASE_COPY
        READ(c->ev.fd,c->r_buf + c->r_used, c->r_size - c->r_used)
#else
        assert(0 < c->r_want);
        READ(c->ev.fd,c->r_buf + c->r_used, c->r_want)
#endif
        
        if(-1 == ret){
            if (EAGAIN == errno || EWOULDBLOCK == errno  || EINTR == errno){
                LOG_I("fd[%d]: con_handler EAGAIN just return and will read again!\n",c->ev.fd);
                c->ev.event_flags = EPOLLIN;
                if(0 != worker_mod(c)){
                    con_close(c);
                }
                return;
            }
            LOG_E("read fd[%d] errno = %d\n",c->ev.fd,errno);
            perror("read()");
            con_close(c);
            return;
         }else if(0 == ret){
            LOG_I("fd[%d]: con_handler get ret == 0 which means: client close!\n",c->ev.fd);
            con_close(c);
            return;
         }else{
            c->r_used += ret;
            if(state_parser_header == c->state){
                uint32_t size = sizeof(protocol_binary_request_header);
#ifdef TEST_CASE_COPY
                if(c->r_used < size){
                    continue;
                 }
#else
                c->r_want -= ret;
                if(0 < c->r_want){
                    continue;
                }
#endif
                protocol_binary_request_header* header = (protocol_binary_request_header*)c->r_curr;
                if(PROTOCOL_BINARY_REQ != header->request.magic){
                    LOG_E("error magic code = %c\n",header->request.magic);
                    con_close(c);
                    return;
                }
                if('g' != header->request.opcode \
                    && 's' != header->request.opcode \
                    && 'u' != header->request.opcode \
                    && 'd' != header->request.opcode \
                    && 'e' != header->request.opcode \
                    && 'l' != header->request.opcode \
                    && 'b' != header->request.opcode \
                    && 'm' != header->request.opcode \
                    && 'r' != header->request.opcode){
                    LOG_E("error opcode code = %c\n",header->request.opcode);
                    con_close(c);
                    return;
                }
                c->h = *header;
                c->h.request.keylen = ntohs(header->request.keylen);
                c->h.request.bodylen = ntohl(header->request.bodylen);
                if('g' == header->request.opcode || 's' == header->request.opcode || 'd' == header->request.opcode || 'u' == header->request.opcode){
                    if(0 == c->h.request.keylen){
                        LOG_E("c->h.request.keylen = 0 when opcode = %c\n",header->request.opcode);
                        con_close(c);
                        return;
                    }
                    if((c->h.request.keylen + sizeof(block)) > g_config.slab_max_size){
                        LOG_E("error keylen is too long!! %d > %lu\n",c->h.request.keylen, (g_config.slab_max_size - sizeof(block)));
                        con_close(c);
                        return;
                    }
                    if('s' == header->request.opcode){
                         if(0 == c->h.request.bodylen){
                            LOG_E("c->h.request.bodylen = 0 when opcode = %c\n",header->request.opcode);
                            con_close(c);
                            return;
                        }   
                    }
                    c->state = state_parser_key;
                    //now we can abandon the data
                    c->r_curr += size;
                    c->r_used -= size;
#ifndef TEST_CASE_COPY
                    c->r_want = c->h.request.keylen;
                    continue;
#endif   
                }
                if('e' == c->h.request.opcode){ 
                    if(0 != expand(c->h.request.bodylen)){
                        LOG_E("expand mem [%u] failed!\n",c->h.request.bodylen);

                    }
                    con_close(c);
                    return;
                }else if('m' == c->h.request.opcode){
                    slab_switch(1);
                    con_close(c);
                    return;
                }else if('r' == c->h.request.opcode){
                    c->h.request.opaque = ntohl(header->request.opaque);
                    c->h.request.cas = ntohl(header->request.cas);
                    c->h.request.reserved = ntohs(header->request.reserved);
                    if((1 < c->h.request.keylen) && (c->h.request.bodylen < c->h.request.opaque)) {
                        //                    min                  max                count               interval            switcher
                        set_lru(c->h.request.bodylen,c->h.request.opaque,c->h.request.keylen,c->h.request.cas,c->h.request.reserved);
                    }else{
                        LOG_E("parameter error when opcode = r\n");
                    }
                    con_close(c);
                    return;
                }else if('l' == c->h.request.opcode){
                    c->w_buf_1 = stats_all();
                    if(NULL == c->w_buf_1){
                        con_close(c);
                        return;
                    }
                    c->w_len = strlen(c->w_buf_1);
                    c->sub_state = state_pack_value_stats;
                    c->state = state_pack_value;
                    break;
                }
                else if('b' == c->h.request.opcode){
                    c->w_buf_1 = get_backup_start(&(c->w_len));
                    if(NULL == c->w_buf_1 || 0 == c->w_len){
                        get_backup_end(c->h.request.keylen);
                        con_close(c);
                        return;
                    }
                    c->sub_state = state_pack_value_backup;
                    c->state = state_pack_value;
                    break;
                }   
            }
            if(state_parser_key == c->state){
                if('g' == c->h.request.opcode ){
#ifdef TEST_CASE_COPY
                    if(c->r_used < c->h.request.keylen){
                         continue;
                     }

#else                    
                    c->r_want -=  ret;
                    if(0 < c->r_want){
                        continue;
                    }
#endif
                    // c->r_curr is the key!
                    c->b = get_block_start(c->h.request.keylen,c->r_curr);
                    //c->value = get(c->h.request.keylen,c->r_curr,&c->lvalue);
                    if(NULL == c->b){
                        char buffer[c->h.request.keylen + 1];
                        memcpy(buffer,c->r_curr,c->h.request.keylen);
                        buffer[c->h.request.keylen] = 0;
                        LOG_E("block[%d,%s] is not found!\n",c->h.request.keylen,buffer);
                    }
                    c->state = state_pack_value;
                    break;       
                }else if(('s' == c->h.request.opcode) || ('u' == c->h.request.opcode)){

#ifdef TEST_CASE_COPY
                    if(c->r_used < c->h.request.keylen + c->h.request.bodylen){
                        continue;
                    }
                    if(0 != set(c->h.request.keylen, c->r_curr,c->h.request.bodylen,c->r_curr + c->h.request.keylen)){
                        LOG_E("set cmd error!\n");
                    }
                    con_contiune(c);
                    return;
#else
                    c->r_want -=  ret;
                    if(0 < c->r_want){
                        continue;
                    }

                    c->state = state_parser_value_set_1;
                    break;
#endif
                    
                }else if('d' == c->h.request.opcode){
#ifdef TEST_CASE_COPY
                    if(c->r_used < c->h.request.keylen){
                         continue;
                     }

#else                    
                    c->r_want -=  ret;
                    if(0 < c->r_want){
                        continue;
                    }
#endif              
                    if(0 != del(c->h.request.keylen,c->r_curr)){
                        char buffer[c->h.request.keylen + 1];
                        memcpy(buffer,c->r_curr,c->h.request.keylen);
                        buffer[c->h.request.keylen] = 0;
                        LOG_E("delete block[%d,%s] is not found!\n",c->h.request.keylen,buffer);

                    }
                    con_contiune(c);
                    return;
                }
            }
         }
     }
#ifdef TEST_CASE_COPY
    while(state_pack_value <= c->state){
#else
    while(state_parser_value_set_1 <= c->state){
#endif
        if(state_parser_value_set_1 == c->state){
            block* b = slab_alloc(sizeof(block) + c->h.request.keylen + c->h.request.bodylen);
            if(NULL == b){
                LOG_E("end set with error slab_alloc failed!\n");
                stats_lock();
                g_stats.set_failed ++;
                stats_unlock();
                con_close(c);
                return;
            }
            block_set_key(b,c->h.request.keylen,c->r_curr);
            c->r_block = b;
            if(0 == b->nvalue){
                assert(0 == (c->r_block->flags & BLOCK_FLAGS_CHILD));
                if(c->r_block->flags & BLOCK_FLAGS_HAVE_CHILD){
                    b->nvalue = BLOCK_FATHER_nv(b);
                }else{
                    b->nvalue = c->h.request.bodylen;
                }
                
            }
            c->r_want = b->nvalue;
            assert(0 < c->r_want);
            c->r_buf_block = BLOCK_v(c->r_block);
            c->state = state_parser_value_set_2;
        }
        if(state_parser_value_set_2 == c->state){
                assert(0 < c->r_want);
                READ(c->ev.fd,c->r_buf_block,c->r_want);
                if(-1 == ret){
                    if (EAGAIN == errno || EWOULDBLOCK == errno || EINTR == errno){
                        LOG_I("fd[%d]: con_handler EAGAIN just return and will read again!\n",c->ev.fd);
                        c->ev.event_flags = EPOLLIN;
                        if(0 != worker_mod(c)){
                            if(c->r_block->flags & BLOCK_FLAGS_CHILD){
                                c->r_block = c->r_block->l_next;
                            }
                            slab_free(c->r_block);
                            con_close(c);  
                        }
                        return;
                    }
                    LOG_E("read fd[%d] errno = %d\n",c->ev.fd,errno);
                    perror("read()");
                    if(c->r_block->flags & BLOCK_FLAGS_CHILD){
                        c->r_block = c->r_block->l_next;
                    }
                    slab_free(c->r_block);
                    con_close(c);
                    return;
                 }else if(0 == ret){
                    LOG_I("fd[%d]: con_handler get ret == 0 which means: client close!\n",c->ev.fd);
                    if(c->r_block->flags & BLOCK_FLAGS_CHILD){
                        c->r_block = c->r_block->l_next;
                    }                    
                    slab_free(c->r_block);
                    con_close(c);
                    return;
                 }else{
                     c->r_want -= ret;
                     c->r_buf_block += ret;
                     if(0 < c->r_want){
                        continue;
                     }else{
                        if(c->r_block->flags & BLOCK_FLAGS_HAVE_CHILD){
                             LOG_B(c->r_block,"parsered!\n");
                            // trick here we set the father nvalue to the real total value
                             c->r_block->nvalue = c->h.request.bodylen;
                             
                             block** child_ptr = (block**)BLOCK_c(c->r_block);
                             c->r_block = *child_ptr;
                             c->r_buf_block = BLOCK_CHILD_v(c->r_block);
                             assert(NULL != c->r_buf_block);
                             c->r_want = c->r_block->nvalue;
                             assert(0 < c->r_want);
                             continue;
                        }else if(c->r_block->flags & BLOCK_FLAGS_CHILD){
                             LOG_B(c->r_block,"parsered!\n");
                             if(NULL != c->r_block->next){
                                 c->r_block = c->r_block->next;
                                 c->r_buf_block = BLOCK_CHILD_v(c->r_block);
                                 c->r_want = c->r_block->nvalue;
                                 continue;
                              }
                        }
                        if(c->r_block->flags & BLOCK_FLAGS_CHILD){
                            c->r_block = c->r_block->l_next;
                        }
                        bklist_lock(c->h.request.keylen,c->r_curr,&(c->hv));
                        block* b_old = bklist_find_no_lock(c->h.request.keylen, c->r_curr,c->hv);
                        if(NULL != b_old){
                            LOG_B_K(c->h.request.keylen,c->r_curr,"is already in bklist[%10x] with hv(%10x)\n",BKLIST_ID(c->hv),c->hv);
                            if('u' == c->h.request.opcode){
                                lru_del(b_old);
                                int ret_delete = bklist_delete_nolock(b_old,c->hv);
                                assert(0 == ret_delete);
                                slab_free(b_old);

                            }else{    
                                bklist_unlock(c->hv);
                                slab_free(c->r_block);
                                STATS_LOCK;
                                g_stats.set_failed ++;
                                STATS_UNLOCK;
                                con_contiune(c);
                                return;
                            }     
                        }
                        bklist_add_nolock(c->r_block,c->hv);
                        bklist_unlock(c->hv);    
                        STATS_LOCK;
                        g_stats.set_ok ++;
                        STATS_UNLOCK;
                        con_contiune(c);
                        return;
                     }
                 }
                
            
        }
        if(state_pack_value == c->state){
            protocol_binary_response_header* h = (protocol_binary_response_header*)c->w_buf;
            h->response.magic = PROTOCOL_BINARY_RES;
            h->response.opcode = c->h.request.opcode;
            if((state_pack_value_stats == c->sub_state) || (state_pack_value_backup == c->sub_state)){
                assert(NULL != c->w_buf_1);
                h->response.bodylen = htonl(c->w_len);
                h->response.cas = htobe64(c->w_len);
                CON_PACK(c,c->w_buf,sizeof(h->response));
                CON_PACK(c,c->w_buf_1,c->w_len);

            }else{
                h->response.keylen = htons(c->h.request.keylen);
                h->response.bodylen = htonl(c->b? c->b->nvalue : 0);
                h->response.extlen = 0;
                CON_PACK(c,c->w_buf,sizeof(h->response));
                CON_PACK(c,c->r_curr,c->h.request.keylen);
                if(c->b) con_pack_block(c,c->b);
            }
            
            //CON_PACK(c,BLOCK_v(c->b),c->b->nvalue);
            c->state = state_send_value;
          }
        if(state_send_value == c->state){
            if(c->msg_curr > c->msg_used){
                c->state = state_done;
                LOG_I("c->msg_size_send = %d\n",c->msg_size_send);
                if(state_pack_value_stats == c->sub_state){
                    assert(NULL != c->w_buf_1);
                    free(c->w_buf_1);
                    c->w_buf_1 = NULL;
                    c->w_len = 0;
                }else if(state_pack_value_backup == c->sub_state){
                    get_backup_end(c->h.request.keylen);
                    c->w_buf_1 = NULL;
                    c->w_len = 0;
                }else{
                    if(c->b) get_block_end(c->b);
                }
                con_contiune(c);
                return;
            }
            struct msghdr* msg = &c->msglist[c->msg_curr];
            ssize_t size = sendmsg(c->ev.fd, msg, MSG_NOSIGNAL);
            if(-1 == size){
                if (EAGAIN == errno || EWOULDBLOCK == errno || EINTR == errno){
                    LOG_I("fd[%d]: con_handler EAGAIN just return and will sendmsg again!\n",c->ev.fd);
                    c->ev.event_flags = EPOLLOUT;
                    if(0 != worker_mod(c)){
                        if(state_pack_value_stats == c->sub_state){
                            assert(NULL != c->w_buf_1);
                            free(c->w_buf_1);
                            c->w_buf_1 = NULL;
                            c->w_len = 0;
                        }else if(state_pack_value_backup == c->sub_state){
                            get_backup_end(c->h.request.keylen);
                            c->w_buf_1 = NULL;
                            c->w_len = 0;
                        }else{
                            if(c->b) get_block_end(c->b);
                        }
                        con_close(c);
                    }
                    return;
                }
                perror("sendmsg()");
                LOG_E("sendmsg errno = %d\n",errno);
                if(state_pack_value_stats == c->sub_state){
                    assert(NULL != c->w_buf_1);
                    free(c->w_buf_1);
                    c->w_buf_1 = NULL;
                    c->w_len = 0;
                }else if(state_pack_value_backup == c->sub_state){
                    get_backup_end(c->h.request.keylen);
                    c->w_buf_1 = NULL;
                    c->w_len = 0;
                }else{
                    if(c->b) get_block_end(c->b);
                }
                con_close(c);
                return;
            }else if(0 < size){
                c->msg_size_send += size;
                while(0 < msg->msg_iovlen && (unsigned int)(size) >= msg->msg_iov->iov_len){
                    size -= msg->msg_iov->iov_len;
                    msg->msg_iovlen --;
                    msg->msg_iov ++;
                }
                if(0 < size){
                    msg->msg_iov->iov_len -= size;
                    msg->msg_iov->iov_base = (caddr_t)(msg->msg_iov->iov_base) + size;
                    
                }
                if(0 == msg->msg_iovlen){
                    c->msg_curr ++;
                }
            }     
          
         }
        }
    
}




void con_init(){
    struct rlimit rl;
    con_max_id = g_config.con_max_id;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        con_max_id = rl.rlim_max;
    } else {
        LOG_W("just use the default!\n");
    }
    if(NULL == conlist){
        conlist = calloc(con_max_id,sizeof(con*));
        assert(NULL != conlist);
    }
    LOG_I("con_init OK con_max_id = %d \n",con_max_id);

    
}

void con_add(int sfd){
    assert(NULL != conlist && 0 <= sfd && sfd < con_max_id);
    con* c = conlist[sfd];
#ifdef TEST_CLIENT_CLEAR
    if(sfd > 3){
#else
    if(con_max_id - sfd < 10000){
#endif
        LOG_W("begin to worker_run_recycle\n");
        worker_run_recycle();
    }
    if(NULL == c){
        c = calloc(1,sizeof(con));
        #ifdef TEST_CASE_FAKE_HANDLE
        c->ev.handler = con_handler_fake;
        #else
        c->ev.handler = con_handler;
        #endif
        c->ev.event_flags = EPOLLIN;
        c->r_size = g_config.data_buffer_size;
        c->w_size = g_config.data_buffer_size;
        c->r_curr = c->r_buf = malloc(c->r_size);
        c->w_buf = malloc(c->w_size);
        c->r_want = sizeof(protocol_binary_request_header);
        //trick!! no need to memset 0
        assert(NULL != c->r_curr && NULL != c->w_buf);
    //    memset(c->r_curr,0,g_config.data_buffer_size);
    //    memset(c->w_buf,0,g_config.data_buffer_size);
        c->iovlist = calloc(g_config.data_iov_size, sizeof(struct iovec));
        c->iov_size = g_config.data_iov_size;
        assert(c->iovlist);
        c->msglist = calloc(g_config.data_msg_size, sizeof(struct msghdr));
        c->msg_size = g_config.data_msg_size;
        c->ev.fd = -1;
        assert(c->msglist);
        conlist[sfd] = c;
    }else{
        assert(-1 == c->used);
        assert(NULL == c->prev);
        assert(NULL == c->next);
        con_reset(c);
        
    }
    c->used = 1;
    c->ev.fd = sfd;
    LOG_I("con_add(c = %p sfd = %d), add the new connection to list, begin to tell worker to handle!\n",c, sfd);
    stats_lock();
    g_stats.thread_run ++;
    if(sfd > g_stats.max_cons) g_stats.max_cons = sfd;
    stats_unlock();
    c->time= get_current_time_sec();
    assert(9 != c->time);
    worker_add(c);
}

void con_reset(con* c){
    LOG_I("c = %p\n",c);
    #ifdef TEST_CASE_FAKE_HANDLE
    c->ev.handler = con_handler_fake;
    #else
    c->ev.handler = con_handler;
    #endif
    c->ev.event_flags = EPOLLIN;
    #if 0
    if(g_config.data_buffer_max <= c->r_size){
        LOG_W("r_size reset from %d to %d\n",c->r_size, g_config.data_buffer_size);
        c->r_size = g_config.data_buffer_size;
        c->r_buf = realloc(c->r_buf,c->r_size);
        
    }
    #endif
    c->r_curr = c->r_buf;
    c->r_used = 0;
    c->r_want = sizeof(protocol_binary_request_header);
    c->r_buf_block = NULL;
    c->r_block = NULL;
    c->hv = 0;
    c->w_buf_1 = NULL;
    c->sub_state = 0;
    c->state = 0;
    c->w_len = 0;
    assert(c->r_curr == c->r_buf && 0 == c->r_used);
   // memset(c->r_curr,0,c->r_size);
   // memset(c->w_buf,0,g_config.data_buffer_size);
    if(g_config.data_msg_max <= c->msg_size){
        LOG_W("msg_size reset from %d to %d\n",c->msg_size, g_config.data_msg_size);
        c->msg_size = g_config.data_msg_size;
        c->msglist = realloc(c->msglist, c->msg_size * sizeof(struct msghdr));   
    }
    c->msg_curr = c->msg_used = c->msg_size_send = 0;
    assert(c->msglist);
    // trick  here we just set the first msghdr to NULL rather than all msghdr in the msglist!!
    memset(c->msglist,0,sizeof(struct msghdr));
    
    if(g_config.data_iov_max <= c->iov_size){
        LOG_W("msg_size reset from %d to %d\n",c->iov_size, g_config.data_iov_size);
        c->iov_size = g_config.data_iov_size;
        c->iovlist = realloc(c->iovlist, c->iov_size * sizeof(struct iovec));
        
        
    }
    // no need to set null to all the iovlist. see con_pack will set the real value!
    c->iov_used = 0;
    assert(c->iovlist);
    c->w = NULL;
    c->state = state_new;
    c->time = 9;
    c->b = NULL;
    c->used = -1;
    // already been set NULL in worker_delete under lock
    //c->prev = c->next = NULL;
    LOG_I("reset c\n");
    
}
void con_delete(con* c){
    LOG_I("con_delete con[%d]\n",c->ev.fd);
    assert(NULL != c);
    con_reset(c);
    //trick!! the con_delete will be call from work threads and main net thread
    //when closed the c->ev.fd. the fd number is recycled by the system. the new client fd would be the same of the fd.
    //just like a lock! If c->ev.fd = -1. it will be a BUG!!! 
    close(c->ev.fd);
    //
    //c->ev.fd = -1;
    stats_signal();
    
}

