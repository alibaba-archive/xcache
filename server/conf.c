/*
* Copyright (C) 2019 Alibaba Group Holding Limited
*/

#include "conf.h"
#include "log.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>


#include <string.h>
#include <assert.h>


_conf_data proxy_conf = {"7fdf1336-f8a1-472e-a76d-661e2db92c33", "./.xcache_conf", 1024};
#define FREE_PATH {if(NULL != path_n) free(path_n);}
static int file_lock(int fd, int type){
    struct flock lock;
    lock.l_type = type;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;
    lock.l_pid = -1;
    if(0 > fcntl(fd,F_SETLKW,&lock)){
        LOG_E("fcntl %d failed!\n",type);
        return -1;
    }
    return 0;
}

static char* add_home_path(char* path_s){
    assert(NULL != path_s);
    struct passwd *pw = getpwuid(getuid());
    if(NULL == pw) return NULL;
    int len = strlen(pw->pw_dir);
    int len_s = strlen(path_s);
    char* path = malloc(len + len_s + 2);
    memcpy(path, pw->pw_dir, len);
    path[len] = '/';
    memcpy(path + len + 1, path_s, len_s);
    path[len + len_s + 1] = '\0';
    return path;
    
}
int conf_to_file(_conf_data* c, const char* value){
    assert(NULL != c);
    assert(NULL != value);
    char* path_n = add_home_path(c->path);
    char* path = c->path;
    if(NULL == path_n){
        LOG_E("get home path error, just use the default path %s\n",c->path);
    }else{
        path = path_n;
    }
    FILE* f = fopen(path,"w+");
    if(NULL == f){
        LOG_E("open %s failed!\n", path);
        FREE_PATH
        return -1;
    }
    int fd;
    fd = fileno(f);
    if (-1 == fd)
    {
        LOG_E("FILE f to fd error\n");
        FREE_PATH
        return -1;
    }
    if(-1 == file_lock(fd, F_WRLCK)){
        LOG_E("file_lock %s F_WRLCK error\n", path);
        FREE_PATH
        return -1;
    }
    uint32_t len = strlen(value);
    if(len != fwrite(value, 1, len, f)){
        LOG_E("fwrite  %s to %s failed!\n", value,path);
        FREE_PATH
        return -1;
    }
    if(-1 == file_lock(fd, F_UNLCK)){
        LOG_E("file_lock %s F_UNLCK error\n", path);
        FREE_PATH
        return -1;
    }
    fclose(f);
    FREE_PATH
    return 0;
    
}
char* conf_from_file(_conf_data* c){
    assert(NULL != c);
    char* path_n = add_home_path(c->path);
    char* path = c->path;
    if(NULL == path_n){
        LOG_E("get home path error, just use the default path %s\n",c->path);
    }else{
        path = path_n;
    }
    LOG_I("get from %s\n",path);
    FILE* f = fopen(path,"r");
    
    if(NULL == f){
        LOG_E("open %s failed!\n", path);
        FREE_PATH
        return NULL;
    }
    int fd;
    fd = fileno(f);
    if (-1 == fd)
    {
        LOG_E("FILE f to fd error\n");
        FREE_PATH
        return NULL;
    }
    char* value = calloc(1,c->max_len);
    if(NULL == value){
        LOG_E("calloc failed!\n");
        FREE_PATH
        return NULL;
    }
    if(-1 == file_lock(fd, F_RDLCK)){
        LOG_E("file_lock %s F_RDLCK error\n", path);
        FREE_PATH
        return NULL;
    }
    int len = fread(value, c->max_len, 1, f);
    if((0 == len) && !feof(f)){
        LOG_E("fread failed!\n");
        free(value);
        FREE_PATH
        return NULL;
    }
    if(-1 == file_lock(fd, F_UNLCK)){
        LOG_E("file_lock %s F_UNLCK error\n", path);
        FREE_PATH
        return NULL;
    }
    fclose(f);
    FREE_PATH
    return value;
}


