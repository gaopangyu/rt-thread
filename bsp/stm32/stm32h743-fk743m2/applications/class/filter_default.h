#ifndef __FILTER_LOGV1_H__
#define __FILTER_LOGV1_H__

#include "filter_obj.h"

struct filter_default_config
{
    char lib_name[RT_NAME_MAX];
};

typedef struct filter_default_config *filter_default_config_t;

struct filter_default_obj
{
    struct filter_obj    parent;
    struct filter_default_config config;
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

typedef struct filter_default_obj *filter_default_obj_t;

#endif /* __FILTER_LOGV1_H__ */