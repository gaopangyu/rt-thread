#ifndef __EVENT_H__
#define __EVENT_H__

#include <rtthread.h>

struct event_pack
{
    uint8_t event_ver;
    uint8_t timestamp[6];
    uint8_t sys_type;
    uint8_t sys_addr;
    uint8_t event_type;
    uint8_t event_description[18];
}__attribute__((packed));
typedef struct event_pack *event_pack_t;

struct event_num_pack
{
    uint32_t number;
    uint8_t event_ver;
    uint8_t timestamp[6];
    uint8_t sys_type;
    uint8_t sys_addr;
    uint8_t event_type;
    uint8_t event_description[18];
}__attribute__((packed));
typedef struct event_num_pack *event_num_pack_t;

enum event_file_type
{
    FILE_TYPE_RESET = 0x01, //复位
    FILE_TYPE_FIRE = 0x02, //火警
    FILE_TYPE_MONITOR = 0x03, //监管
    FILE_TYPE_FAULT = 0x04, //故障/恢复
    FILE_TYPE_ACTION = 0x05, //动作(启停)
    FILE_TYPE_LINKAGE = 0x06, //联动(反馈/取消)
    FILE_TYPE_MASK = 0x07, //屏蔽/取消
    FILE_TYPE_OTHER = 0x0C, //其他
    FILE_TYPE_CTRL = 0x0D, //控制
};

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

char * get_file_name(enum event_file_type type);

#endif /* __EVENT_H__ */