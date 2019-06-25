/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#ifndef _X_CONNECT_H_
#define _X_CONNECT_H_


#include <stdint.h>
#include "event.h"
#include "protocol.h"
#include "block.h"





enum con_state{
    state_new = 0,
    state_parser_header,
    state_parser_key,
    state_parser_cmd,
    state_parser_value_set_1,
    state_parser_value_set_2,
    state_pack_value,
    state_pack_value_stats,
    state_pack_value_backup,
    state_send_value,
    state_send_value_1,
    state_send_value_2,
    state_done
};


typedef struct _con{
    event ev;
    void* w;
    struct _con* next;
    struct _con* prev;
    char* key;

    uint8_t state;
    int8_t sub_state;
    protocol_binary_request_header h;
    block* b;



    
    char* r_buf;
    char* r_curr;
    uint32_t r_size;
    uint32_t r_used;
    uint32_t r_want;
    char* r_buf_block;
    block* r_block;
    uint32_t hv;
    

    char* w_buf;
    uint32_t w_size;

    char* w_buf_1;
    uint64_t w_len;


    struct iovec *iovlist;
    uint32_t    iov_size;
    uint32_t    iov_used;

    struct msghdr *msglist;
    uint32_t    msg_size;
    uint32_t    msg_used;
    uint32_t    msg_curr;
    uint32_t    msg_size_send;

    int used;
    time_t time;
    
}con;

void con_init();
void con_add(int sfd);
void con_delete(con* c);

#endif
