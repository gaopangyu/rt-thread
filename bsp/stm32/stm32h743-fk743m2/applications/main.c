/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * 2019-09-17     tyustli   first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "usbd_core.h"
#include "interface_rs232.h"

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

#define USB0_BUSID 0
#ifndef USB0_FINSH_CDC_NAME
#define USB0_FINSH_CDC_NAME "usb-acm0"
#endif


int main(void)
{
    #ifdef ULOG_USING_FILTER
        ulog_global_filter_lvl_set(LOG_LVL_DBG);
    #endif


    // for (int i = 0; i < 500; i++) {
    //     if (usb_device_is_configured(USB0_BUSID)) {
    //         #if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
    //             rt_console_set_device(USB0_FINSH_CDC_NAME);
    //         #endif
    //         #ifdef RT_USING_FINSH
    //             finsh_set_device(USB0_FINSH_CDC_NAME);
    //         #endif
    //         break;
    //     }
    //     rt_thread_mdelay(20);
    // }

    // rt_thread_mdelay(500);


    /* defined the LED0 pin: PB7 */
    #define LED0_PIN    GET_PIN(D, 12) //GET_PIN(H, 7)
    rt_pin_mode(LED0_PIN, PIN_MODE_OUTPUT);

    #define MP_STAT_PIN   GET_PIN(B, 2)
    rt_pin_mode(MP_STAT_PIN, PIN_MODE_INPUT);

    // GET_PIN(F, 0): 80 485_A1B1
    // GET_PIN(F, 1): 81 485_A2B2
    // GET_PIN(F, 2): 82 232_T1R1
    // GET_PIN(F, 3): 83 232_T2R2
    // GET_PIN(E, 13): 77 485_A3B3
    // GET_PIN(B, 7): 23 MCU_STAT
    // GET_PIN(A, 10): 10  USB_ID
    rt_kprintf("GET_PIN(F, 0): %d\r\n", GET_PIN(F, 0));
    rt_kprintf("GET_PIN(F, 1): %d\r\n", GET_PIN(F, 1));
    rt_kprintf("GET_PIN(F, 2): %d\r\n", GET_PIN(F, 2));
    rt_kprintf("GET_PIN(F, 3): %d\r\n", GET_PIN(F, 3));
    rt_kprintf("GET_PIN(E, 13): %d\r\n", GET_PIN(E, 13));
    rt_kprintf("GET_PIN(B, 7): %d\r\n", GET_PIN(B, 7));
    rt_kprintf("GET_PIN(A, 10): %d\r\n", GET_PIN(A, 10));

    struct timeval now;
    gettimeofday(&now, RT_NULL);
    LOG_I("time: %lld.%03ld", (long long)now.tv_sec, (long)now.tv_usec / 1000);

    #include "host_std.h"
    rt_uint8_t host_id = 0;
    rt_uint8_t platform_id = 0;

    struct interface_rs232_config bus_interface_cfg = 
    {
        .uart_name = "uart8",
        .baud_rate = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = 0,
        .bit_order = 0,
        .rx_bufsz = 10240,
        .dma_ping_bufsz = 1024,
        .tx_bufsz = 1024,
        .rx_en_pin = 80
    };

    struct protocol_ev1_config bus_protocol_cfg =
    {
        .lib_name = "libtx3016a.so"
    };

    struct upload_store_config store_upload_cfg =
    {
        .dir = "ev1",
        .path = "tx3016a"
    };

    struct upload_muart_config muart_upload_cfg =
    {
        .number = host_id * 10 + platform_id
    };

    struct host_std_config host_std_cfg = {0};
    host_std_cfg.host_id = 0;
    host_std_cfg.host_type = HOST_TYPE_STD;
    host_std_cfg.crt_com_id = 0;
    host_std_cfg.crt_interface_cfg = RT_NULL;
    host_std_cfg.crt_protocol_cfg = RT_NULL;
    host_std_cfg.bus_com_id = 1;
    host_std_cfg.bus_interface_cfg = &bus_interface_cfg;
    host_std_cfg.bus_protocol_cfg = &bus_protocol_cfg;
    host_std_cfg.store_upload_cfg = &store_upload_cfg;
    host_std_cfg.muart_upload_cfg[platform_id] = &muart_upload_cfg;
    
    host_std_create(&host_std_cfg);

    while (1) {

        // if(rt_pin_read(MP_STAT_PIN) == PIN_LOW)
        // {
        //     rt_pin_write(LED0_PIN, PIN_LOW);
        //     rt_kprintf("MP_STAT_PIN LOW\r\n");
        // }
        // else
        // {
        //     rt_pin_write(LED0_PIN, PIN_HIGH);
        //     rt_kprintf("MP_STAT_PIN HIGH\r\n");
        // }

        // rt_thread_mdelay(1000);

        rt_pin_write(LED0_PIN, PIN_HIGH);
        rt_thread_mdelay(100);
        rt_pin_write(LED0_PIN, PIN_LOW);
        rt_thread_mdelay(100);
        
        // uint16_t illegal_code[] = {0xFFFF, 0xFFFF};
        // void (*func)(void) = (void (*)(void))illegal_code;
        // func();
    }
}

// 测试示例:
// 步骤一：
// create_interface_rs232 interface_rs232_1 -u uart8 -p 80
// create_protocol_ev1 protocol_ev1_1 -l libtx3016a.so
// create_protocol_ev1 protocol_ev1_2 -l libtx3016a.so
// create_protocol_ev1 protocol_ev1_3 -l libtx3016a.so
// create_upload_muart upload_muart_1 -n 1
// create_upload_muart upload_muart_2 -n 2
// create_upload_muart upload_muart_3 -n 3
// create_bind interface_rs232_1 protocol_ev1_1 -r
// create_bind interface_rs232_1 protocol_ev1_1 -t
// create_bind interface_rs232_1 protocol_ev1_2 -r
// create_bind interface_rs232_1 protocol_ev1_3
// create_bind protocol_ev1_1 upload_muart_1
// create_bind protocol_ev1_2 upload_muart_2
// create_bind protocol_ev1_3 upload_muart_3

// 步骤二:
// create_interface_rs232 interface_rs232_1 -u uart8 -p 80 -r 10240 -d 5120
// create_protocol_ev1 protocol_ev1_1 -l libtx3016a.so
// create_protocol_ev1 protocol_ev1_2 -l libtx3016a_1.so
// create_protocol_ev1 protocol_ev1_3 -l libtx3016a_2.so
// create_upload_muart upload_muart_1 -n 1
// create_upload_muart upload_muart_2 -n 2
// create_upload_muart upload_muart_3 -n 3
// create_bind interface_rs232_1 protocol_ev1_1 -r
// create_bind protocol_ev1_1 upload_muart_1 -r
// create_bind interface_rs232_1 protocol_ev1_2 -t
// create_bind protocol_ev1_2 upload_muart_2 -t
// create_bind interface_rs232_1 protocol_ev1_3 -r -t
// create_bind protocol_ev1_3 upload_muart_3 -r -t
// 步骤三:追加在步骤二后面测试list_bind_path
// create_interface_rs232 interface_rs232_2 -u uart2 -p 81 -r 10240 -d 5120
// create_bind interface_rs232_2 protocol_ev1_1
// create_bind protocol_ev1_1 upload_muart_2
// 步骤四:追加在步骤三后面测试并发
// create_interface_rs232 interface_rs232_3 -u uart1 -p 77 -r 10240 -d 5120
// create_bind interface_rs232_3 protocol_ev1_1



// 测试日志系统
// create_interface_rs232 interface_rs232_1 -u uart8 -p 80 -r 10240 -d 5120
// create_protocol_ev1 protocol_ev1_1 -l libtx3016a.so
// create_upload_store upload_store_1 -d ev1 -p tx3016a
// create_bind interface_rs232_1 protocol_ev1_1
// create_bind protocol_ev1_1 upload_store_1
// delete_upload_store -n upload_store_1

// 测试屏幕
// create_interface_rs232 interface_rs232_1 -u uart8 -p 80 -r 10240 -d 5120
// create_protocol_ev1 protocol_ev1_1 -l libtx3016a.so
// create_upload_screen upload_screen_1 -u uart2 -p 80 -r 10240 -d 5120
// create_bind interface_rs232_1 protocol_ev1_1
// create_bind protocol_ev1_1 upload_screen_1
// delete_upload_screen -n upload_screen_1


//777
// delete_protocol_ev1 -n protocol_ev1_1
// delete_interface_rs232 -n interface_rs232_1
// delete_upload_muart -n  upload_muart_1
// delete_interface_rs232 -n interface_rs232_2
// delete_protocol_ev1 -n protocol_ev1_2
// delete_upload_muart -n  upload_muart_2
// delete_upload_muart -n  upload_muart_3
// delete_protocol_ev1 -n protocol_ev1_3
// delete_interface_rs232 -n interface_rs232_3

//666 OK
// delete_protocol_ev1 -n protocol_ev1_1
// delete_protocol_ev1 -n protocol_ev1_2
// delete_protocol_ev1 -n protocol_ev1_3
// delete_interface_rs232 -n interface_rs232_1
// delete_interface_rs232 -n interface_rs232_2
// delete_interface_rs232 -n interface_rs232_3
// delete_upload_muart -n  upload_muart_1
// delete_upload_muart -n  upload_muart_2
// delete_upload_muart -n  upload_muart_3

//555 OK
// delete_protocol_ev1 -n protocol_ev1_1
// delete_protocol_ev1 -n protocol_ev1_2
// delete_protocol_ev1 -n protocol_ev1_3
// delete_upload_muart -n  upload_muart_1
// delete_upload_muart -n  upload_muart_2
// delete_upload_muart -n  upload_muart_3
// delete_interface_rs232 -n interface_rs232_1
// delete_interface_rs232 -n interface_rs232_2
// delete_interface_rs232 -n interface_rs232_3

//444 OK
// delete_upload_muart -n  upload_muart_1
// delete_upload_muart -n  upload_muart_2
// delete_upload_muart -n  upload_muart_3
// delete_protocol_ev1 -n protocol_ev1_1
// delete_protocol_ev1 -n protocol_ev1_2
// delete_protocol_ev1 -n protocol_ev1_3
// delete_interface_rs232 -n interface_rs232_1
// delete_interface_rs232 -n interface_rs232_2
// delete_interface_rs232 -n interface_rs232_3

//333 OK
// delete_upload_muart -n  upload_muart_1
// delete_upload_muart -n  upload_muart_2
// delete_upload_muart -n  upload_muart_3
// delete_interface_rs232 -n interface_rs232_1
// delete_interface_rs232 -n interface_rs232_2
// delete_interface_rs232 -n interface_rs232_3
// delete_protocol_ev1 -n protocol_ev1_1
// delete_protocol_ev1 -n protocol_ev1_2
// delete_protocol_ev1 -n protocol_ev1_3

//222 OK
// delete_interface_rs232 -n interface_rs232_1
// delete_interface_rs232 -n interface_rs232_2
// delete_interface_rs232 -n interface_rs232_3
// delete_upload_muart -n  upload_muart_1
// delete_upload_muart -n  upload_muart_2
// delete_upload_muart -n  upload_muart_3
// delete_protocol_ev1 -n protocol_ev1_1
// delete_protocol_ev1 -n protocol_ev1_2
// delete_protocol_ev1 -n protocol_ev1_3

//111 OK
// delete_interface_rs232 -n interface_rs232_1
// delete_interface_rs232 -n interface_rs232_2
// delete_interface_rs232 -n interface_rs232_3
// delete_protocol_ev1 -n protocol_ev1_1
// delete_protocol_ev1 -n protocol_ev1_2
// delete_protocol_ev1 -n protocol_ev1_3
// delete_upload_muart -n  upload_muart_1
// delete_upload_muart -n  upload_muart_2
// delete_upload_muart -n  upload_muart_3



