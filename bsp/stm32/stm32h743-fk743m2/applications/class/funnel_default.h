#ifndef __FUNNEL_DEFAULT_H__
#define __FUNNEL_DEFAULT_H__

#include "funnel_obj.h"
#include "event.h"

struct event_last_num
{
    rt_uint32_t reset;
    rt_uint32_t fire;
    rt_uint32_t monitor;
    rt_uint32_t fault;
    rt_uint32_t action;
    rt_uint32_t linkage;
    rt_uint32_t mask;
    rt_uint32_t other;
    rt_uint32_t ctrl;
};

struct funnel_default_config
{
    rt_uint8_t func_id;
    struct event_last_num event_last_num;
};
typedef struct funnel_default_config *funnel_default_config_t;

struct funnel_default_obj
{
    struct funnel_obj    parent;
    struct funnel_default_config config;

    rt_thread_t rx_tid;
    rt_thread_t rx_sub_event_tid;
    rt_thread_t tx_tid;
    rt_thread_t tx_sub_event_tid;

    struct event_last_num event_last_num;
};
typedef struct funnel_default_obj *funnel_default_obj_t;

// rt_uint32_t get_event_last_num(funnel_default_obj_t obj, enum event_file_type type);

#endif /* __FUNNEL_DEFAULT_H__ */