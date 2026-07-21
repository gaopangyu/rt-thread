#if 0

#ifndef __INTERFACE_RS232_H__
#define __INTERFACE_RS232_H__

#include "interface_obj.h"

struct interface_rs232_config
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

typedef struct interface_rs232_config *interface_rs232_config_t;

struct interface_rs232_obj
{
    struct interface_obj    parent;

    rt_size_t rx_bufsz;
    rt_uint8_t *rx_buffer;
    rt_device_t uart;
    struct interface_rs232_config config;
    rt_sem_t rx_ind_sem;
    rt_thread_t rx_tid;
    // rt_thread_t rx_sub_event_tid;
    rt_thread_t tx_tid;
    rt_thread_t tx_sub_event_tid;
};

typedef struct interface_rs232_obj *interface_rs232_obj_t;

#endif /* __INTERFACE_RS232_H__ */

#endif