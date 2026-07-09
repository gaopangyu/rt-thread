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
// create_protocol_logv1 protocol_logv1_1 -l libtx3016a.so
// create_protocol_logv1 protocol_logv1_2 -l libtx3016a.so
// create_protocol_logv1 protocol_logv1_3 -l libtx3016a.so
// create_upload_muart upload_muart_1 -n 1
// create_upload_muart upload_muart_2 -n 2
// create_upload_muart upload_muart_3 -n 3
// create_bind interface_rs232_1 protocol_logv1_1 -r
// create_bind interface_rs232_1 protocol_logv1_1 -t
// create_bind interface_rs232_1 protocol_logv1_2 -r
// create_bind interface_rs232_1 protocol_logv1_3
// create_bind protocol_logv1_1 upload_muart_1
// create_bind protocol_logv1_2 upload_muart_2
// create_bind protocol_logv1_3 upload_muart_3
// 步骤二:
// create_interface_rs232 interface_rs232_1 -u uart8 -p 80 -r 10240 -d 5120
// create_protocol_logv1 protocol_logv1_1 -l libtx3016a.so
// create_protocol_logv1 protocol_logv1_2 -l libtx3016a_1.so
// create_protocol_logv1 protocol_logv1_3 -l libtx3016a_2.so
// create_upload_muart upload_muart_1 -n 1
// create_upload_muart upload_muart_2 -n 2
// create_upload_muart upload_muart_3 -n 3
// create_bind interface_rs232_1 protocol_logv1_1 -r
// create_bind protocol_logv1_1 upload_muart_1 -r
// create_bind interface_rs232_1 protocol_logv1_2 -t
// create_bind protocol_logv1_2 upload_muart_2 -t
// create_bind interface_rs232_1 protocol_logv1_3 -r -t
// create_bind protocol_logv1_3 upload_muart_3 -r -t
// 步骤三:追加在步骤二后面测试list_bind_path
// create_interface_rs232 interface_rs232_2 -u uart2 -p 81 -r 10240 -d 5120
// create_bind interface_rs232_2 protocol_logv1_1
// create_bind protocol_logv1_1 upload_muart_2
// 步骤四:追加在步骤三后面测试并发
// create_interface_rs232 interface_rs232_3 -u uart1 -p 77 -r 10240 -d 5120
// create_bind interface_rs232_3 protocol_logv1_1

