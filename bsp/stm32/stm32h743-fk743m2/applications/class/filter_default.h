#ifndef __FILTER_DEFAULT_H__
#define __FILTER_DEFAULT_H__

#include "filter_obj.h"

struct filter_default_config
{
    rt_uint8_t func_id;
};

typedef struct filter_default_config *filter_default_config_t;

struct filter_default_obj
{
    struct filter_obj    parent;
    struct filter_default_config config;

    rt_thread_t rx_tid;
    rt_thread_t rx_sub_event_tid;
    rt_thread_t tx_tid;
    rt_thread_t tx_sub_event_tid;
};

typedef struct filter_default_obj *filter_default_obj_t;

#endif /* __FILTER_DEFAULT_H__ */