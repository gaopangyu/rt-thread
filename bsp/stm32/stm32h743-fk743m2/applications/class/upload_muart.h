#ifndef __UPLOAD_MUART_H__
#define __UPLOAD_MUART_H__

#include "upload_obj.h"
#include "ppp.h"

struct upload_muart_config
{
    rt_uint8_t number;  /* PPP 通道号，对应逻辑帧 uart_id */
};

typedef struct upload_muart_config *upload_muart_config_t;

struct upload_muart_obj
{
    struct upload_obj parent;
    struct upload_muart_config config;
    rt_list_t list;
    struct ppp_link link;           /* 本通道 PPP 会话，id = config.number */
    rt_thread_t rx_tid;             /* 绑定侧收包：bind → PPP 发出 */
    rt_thread_t rx_sub_event_tid;
    volatile rt_bool_t link_idle_flag;
};

typedef struct upload_muart_obj *upload_muart_obj_t;

#endif /* __UPLOAD_MUART_H__ */
