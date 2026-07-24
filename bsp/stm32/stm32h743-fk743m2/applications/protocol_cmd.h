#ifndef __PROTOCOL_CMD_H__
#define __PROTOCOL_CMD_H__

#include "protocol_obj.h"

struct protocol_cmd_config
{
    char lib_name[RT_NAME_MAX];
};

typedef struct protocol_cmd_config *protocol_cmd_config_t;

struct protocol_cmd_obj
{
    struct protocol_obj    parent;
    struct protocol_cmd_config config;
    void *lib_handle;
    init_func_t lib_init;
    destroy_initentry_func_t lib_destroy_initentry;
    process_data_func_t lib_process_data;
    destroy_event_func_t lib_destroy_event;
    remote_control_func_t lib_remote_ctrl;
    destroy_ctrl_resp_func_t lib_destroy_ctrl_resp;
    rt_thread_t rx_tid;
    rt_thread_t rx_sub_event_tid;
    rt_thread_t tx_tid;
    rt_thread_t tx_sub_event_tid;
};

typedef struct protocol_cmd_obj *protocol_cmd_obj_t;

#endif /* __PROTOCOL_CMD_H__ */