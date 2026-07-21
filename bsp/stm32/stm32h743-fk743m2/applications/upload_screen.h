#ifndef __UPLOAD_SCREEN_H__
#define __UPLOAD_SCREEN_H__

#include "upload_obj.h"

struct upload_screen_config
{
    char uart_name[RT_NAME_MAX];
    rt_uint32_t baud_rate;
    rt_uint8_t data_bits;
    rt_uint8_t stop_bits;
    rt_uint8_t parity;
    rt_uint8_t bit_order;
    rt_size_t rx_bufsz;
    rt_size_t dma_ping_bufsz;
    rt_size_t tx_bufsz;
    rt_base_t rx_en_pin;
};

typedef struct upload_screen_config *upload_screen_config_t;

struct upload_screen_obj
{
    struct upload_obj    parent;

    rt_size_t rx_bufsz;
    rt_uint8_t *rx_buffer;
    rt_device_t uart;
    struct upload_screen_config config;
    rt_sem_t rx_ind_sem;
    rt_thread_t rx_tid;
    // rt_thread_t rx_sub_event_tid;
    rt_thread_t tx_tid;
    rt_thread_t tx_sub_event_tid;
};

typedef struct upload_screen_obj *upload_screen_obj_t;

#endif /* __UPLOAD_SCREEN_H__ */