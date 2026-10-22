#ifndef __EVENT_H__
#define __EVENT_H__

#include <rtthread.h>
#include "lib.h"

struct event_num_pack
{
    uint32_t number;
    struct event_pack event;
}__attribute__((packed));
typedef struct event_num_pack *event_num_pack_t;

struct event_history
{
    rt_uint8_t host_id;
    rt_uint8_t event_type; // 低4位 FILE_TYPE_*，0x0F 表示所有类型
    rt_uint16_t count;
    rt_uint32_t date; // 十进制整数，如 20260825 只读该日；0xFFFFFFFF 只读最近一天；0 不分日期
    rt_uint32_t reset_index; // 该类型从 number=index 起读（含）；0xFFFFFFFF 读最后一条
    rt_uint32_t fire_index;
    rt_uint32_t monitor_index;
    rt_uint32_t fault_index;
    rt_uint32_t action_index;
    rt_uint32_t linkage_index;
    rt_uint32_t mask_index;
    rt_uint32_t other_index;
    rt_uint32_t ctrl_index;
}__attribute__((packed));
typedef struct event_history *event_history_t;

char * event_get_file_name(enum event_file_type type);

#endif /* __EVENT_H__ */