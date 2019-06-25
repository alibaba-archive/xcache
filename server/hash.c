/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "hash.h"

#include "config.h"
#include "murmur3_hash.h"




uint32_t hash (const void *key, size_t length){
#ifdef test_case_hash
    return length;
#endif
    return MurmurHash3_x86_32(key,length);
}




