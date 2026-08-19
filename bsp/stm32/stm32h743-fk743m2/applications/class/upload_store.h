#ifndef __UPLOAD_STORE_H__
#define __UPLOAD_STORE_H__

#include "upload_obj.h"

struct upload_store_config
{
    char dir[RT_NAME_MAX];
    char path[RT_NAME_MAX];
};

typedef struct upload_store_config *upload_store_config_t;

struct upload_store_obj
{
    struct upload_obj    parent;
    struct upload_store_config config;

    char ramfs_path[RT_NAME_MAX];
    char diskfs_path[RT_NAME_MAX];

    rt_mutex_t ramfs_fd_mutex;
    rt_mutex_t diskfs_fd_mutex;

    rt_thread_t rx_tid;
    rt_thread_t rx_sub_event_tid;

    rt_uint32_t reset_last_num;
    rt_uint32_t fire_last_num;
    rt_uint32_t monitor_last_num;
    rt_uint32_t fault_last_num;
    rt_uint32_t action_last_num;
    rt_uint32_t linkage_last_num;
    rt_uint32_t mask_last_num;
    rt_uint32_t other_last_num;
    rt_uint32_t ctrl_last_num;
};

typedef struct upload_store_obj *upload_store_obj_t;

#endif /* __STORE_H__ */