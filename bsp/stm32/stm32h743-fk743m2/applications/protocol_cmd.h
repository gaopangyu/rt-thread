#ifndef __PROTOCOL_CMD_H__
#define __PROTOCOL_CMD_H__

#include "protocol_obj.h"

struct protocol_cmd_obj
{
    struct protocol_obj    parent;

    rt_thread_t recv_sub_event_tid;
    rt_thread_t recv_tid;
    rt_sem_t recv_sem;
    rt_thread_t parse_tid;
};

typedef struct protocol_cmd_obj *protocol_cmd_obj_t;

#endif /* __PROTOCOL_CMD_H__ */