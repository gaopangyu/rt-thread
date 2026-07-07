#ifndef __UPLOAD_MUART_H__
#define __UPLOAD_MUART_H__

#include "upload_obj.h"

struct upload_muart_config
{
    rt_uint8_t number;
};

typedef struct upload_muart_config *upload_muart_config_t;

struct upload_muart_obj
{
    struct upload_obj    parent;
    rt_list_t list;
    struct upload_muart_config config;
    rt_sem_t   ack_sem;
    rt_uint8_t            ack_status;   /* 最近 ACK 状态，见 MUART_PKT_ACK_* */
    rt_uint8_t            seq;          /* 当前发送流水号 0~0xF，ACK 成功后递增 */
    rt_uint8_t            rx_seq;       /* 最近已处理的数据包流水号 */
    rt_thread_t rx_tid;
    rt_thread_t rx_sub_event_tid;
};

typedef struct upload_muart_obj *upload_muart_obj_t;

#endif /* __UPLOAD_MUART_H__ */