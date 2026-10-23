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
#include "param.h"
#include "console.h"

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

//打印heap信息
static void print_heap_info(void)
{
    rt_size_t total = 0, used = 0, max_used = 0;
    rt_memory_info(&total, &used, &max_used);
    rt_kprintf("heap:\r\ntotal=%u\r\nused=%u\r\nmax=%u\r\nfree=%u\r\n",
                (unsigned)total, (unsigned)used, (unsigned)max_used,
                (unsigned)(total - used));
}

int main(void)
{
    // 按照日期获取历史事件不恢复绑定
    #ifdef ULOG_USING_FILTER
        ulog_global_filter_lvl_set(LOG_LVL_DBG);
        ulog_tag_lvl_filter_set("protocol_net_cmd", LOG_LVL_WARNING);
        ulog_tag_lvl_filter_set("upload_net", LOG_LVL_WARNING);
    #endif

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

    print_heap_info();

    param_set_all_from_file(PARAM_FILE_PATH);

    print_heap_info();

    rt_pin_mode(GET_PIN(D, 0 ), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(D, 1 ), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(C, 13), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(D, 3 ), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(D, 5 ), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(D, 6 ), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(D, 7 ), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(C, 11), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(D, 8 ), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(D, 9 ), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(D, 10), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(D, 11), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(C, 10), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(D, 13), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(D, 14), PIN_MODE_OUTPUT);
    rt_pin_mode(GET_PIN(C, 8 ), PIN_MODE_OUTPUT);

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

        rt_pin_write(GET_PIN(D, 0 ), PIN_HIGH);
        rt_pin_write(GET_PIN(D, 1 ), PIN_HIGH);
        rt_pin_write(GET_PIN(C, 13), PIN_HIGH);
        rt_pin_write(GET_PIN(D, 3 ), PIN_HIGH);
        rt_pin_write(GET_PIN(D, 5 ), PIN_HIGH);
        rt_pin_write(GET_PIN(D, 6 ), PIN_HIGH);
        rt_pin_write(GET_PIN(D, 7 ), PIN_HIGH);
        rt_pin_write(GET_PIN(C, 11), PIN_HIGH);
        rt_pin_write(GET_PIN(D, 8 ), PIN_HIGH);
        rt_pin_write(GET_PIN(D, 9 ), PIN_HIGH);
        rt_pin_write(GET_PIN(D, 10), PIN_HIGH);
        rt_pin_write(GET_PIN(D, 11), PIN_HIGH);
        rt_pin_write(GET_PIN(C, 10), PIN_HIGH);
        rt_pin_write(GET_PIN(D, 13), PIN_HIGH);
        rt_pin_write(GET_PIN(D, 14), PIN_HIGH);
        rt_pin_write(GET_PIN(C, 8 ), PIN_HIGH);
        rt_thread_mdelay(1000);
        rt_pin_write(GET_PIN(D, 0 ), PIN_LOW);
        rt_pin_write(GET_PIN(D, 1 ), PIN_LOW);
        rt_pin_write(GET_PIN(C, 13), PIN_LOW);
        rt_pin_write(GET_PIN(D, 3 ), PIN_LOW);
        rt_pin_write(GET_PIN(D, 5 ), PIN_LOW);
        rt_pin_write(GET_PIN(D, 6 ), PIN_LOW);
        rt_pin_write(GET_PIN(D, 7 ), PIN_LOW);
        rt_pin_write(GET_PIN(C, 11), PIN_LOW);
        rt_pin_write(GET_PIN(D, 8 ), PIN_LOW);
        rt_pin_write(GET_PIN(D, 9 ), PIN_LOW);
        rt_pin_write(GET_PIN(D, 10), PIN_LOW);
        rt_pin_write(GET_PIN(D, 11), PIN_LOW);
        rt_pin_write(GET_PIN(C, 10), PIN_LOW);
        rt_pin_write(GET_PIN(D, 13), PIN_LOW);
        rt_pin_write(GET_PIN(D, 14), PIN_LOW);
        rt_pin_write(GET_PIN(C, 8 ), PIN_LOW);
        rt_thread_mdelay(1000);
        
        // uint16_t illegal_code[] = {0xFFFF, 0xFFFF};
        // void (*func)(void) = (void (*)(void))illegal_code;
        // func();
    }
}

// 测试示例:
// 步骤一：
// interface_rs232_create interface_rs232_1 -u uart8 -p 80
// protocol_ev1_create protocol_ev1_1 -l lib010101_tx3016a_crt.so
// protocol_ev1_create protocol_ev1_2 -l lib010101_tx3016a_crt.so
// protocol_ev1_create protocol_ev1_3 -l lib010101_tx3016a_crt.so
// upload_net_create upload_net_1 -n 1
// upload_net_create upload_net_2 -n 2
// upload_net_create upload_net_3 -n 3
// bind_create interface_rs232_1 protocol_ev1_1 -r
// bind_create interface_rs232_1 protocol_ev1_1 -t
// bind_create interface_rs232_1 protocol_ev1_2 -r
// bind_create interface_rs232_1 protocol_ev1_3
// bind_create protocol_ev1_1 upload_net_1
// bind_create protocol_ev1_2 upload_net_2
// bind_create protocol_ev1_3 upload_net_3

// 步骤二:
// interface_rs232_create interface_rs232_1 -u uart8 -p 80 -r 10240 -d 5120
// protocol_ev1_create protocol_ev1_1 -l libtx3016a.so
// protocol_ev1_create protocol_ev1_2 -l libtx3016a_1.so
// protocol_ev1_create protocol_ev1_3 -l libtx3016a_2.so
// upload_net_create upload_net_1 -n 1
// upload_net_create upload_net_2 -n 2
// upload_net_create upload_net_3 -n 3
// bind_create interface_rs232_1 protocol_ev1_1 -r
// bind_create protocol_ev1_1 upload_net_1 -r
// bind_create interface_rs232_1 protocol_ev1_2 -t
// bind_create protocol_ev1_2 upload_net_2 -t
// bind_create interface_rs232_1 protocol_ev1_3 -r -t
// bind_create protocol_ev1_3 upload_net_3 -r -t
// 步骤三:追加在步骤二后面测试bind_path_list
// interface_rs232_create interface_rs232_2 -u uart2 -p 81 -r 10240 -d 5120
// bind_create interface_rs232_2 protocol_ev1_1
// bind_create protocol_ev1_1 upload_net_2
// 步骤四:追加在步骤三后面测试并发
// interface_rs232_create interface_rs232_3 -u uart1 -p 77 -r 10240 -d 5120
// bind_create interface_rs232_3 protocol_ev1_1



// 测试日志系统
// interface_rs232_create interface_rs232_1 -u uart8 -p 80 -r 10240 -d 5120
// protocol_ev1_create protocol_ev1_1 -l libtx3016a.so
// upload_store_create upload_store_1 -d ev1 -p tx3016a
// bind_create interface_rs232_1 protocol_ev1_1
// bind_create protocol_ev1_1 upload_store_1
// upload_store_delete -n upload_store_1

// 测试屏幕
// interface_rs232_create interface_rs232_1 -u uart8 -p 80 -r 10240 -d 5120
// protocol_ev1_create protocol_ev1_1 -l libtx3016a.so
// create_upload_screen upload_screen_1 -u uart2 -p 80 -r 10240 -d 5120
// bind_create interface_rs232_1 protocol_ev1_1
// bind_create protocol_ev1_1 upload_screen_1
// delete_upload_screen -n upload_screen_1


//777
// protocol_ev1_delete -n protocol_ev1_1
// interface_rs232_delete -n interface_rs232_1
// upload_net_delete -n  upload_net_1
// interface_rs232_delete -n interface_rs232_2
// protocol_ev1_delete -n protocol_ev1_2
// upload_net_delete -n  upload_net_2
// upload_net_delete -n  upload_net_3
// protocol_ev1_delete -n protocol_ev1_3
// interface_rs232_delete -n interface_rs232_3

//666 OK
// protocol_ev1_delete -n protocol_ev1_1
// protocol_ev1_delete -n protocol_ev1_2
// protocol_ev1_delete -n protocol_ev1_3
// interface_rs232_delete -n interface_rs232_1
// interface_rs232_delete -n interface_rs232_2
// interface_rs232_delete -n interface_rs232_3
// upload_net_delete -n  upload_net_1
// upload_net_delete -n  upload_net_2
// upload_net_delete -n  upload_net_3

//555 OK
// protocol_ev1_delete -n protocol_ev1_1
// protocol_ev1_delete -n protocol_ev1_2
// protocol_ev1_delete -n protocol_ev1_3
// upload_net_delete -n  upload_net_1
// upload_net_delete -n  upload_net_2
// upload_net_delete -n  upload_net_3
// interface_rs232_delete -n interface_rs232_1
// interface_rs232_delete -n interface_rs232_2
// interface_rs232_delete -n interface_rs232_3

//444 OK
// upload_net_delete -n  upload_net_1
// upload_net_delete -n  upload_net_2
// upload_net_delete -n  upload_net_3
// protocol_ev1_delete -n protocol_ev1_1
// protocol_ev1_delete -n protocol_ev1_2
// protocol_ev1_delete -n protocol_ev1_3
// interface_rs232_delete -n interface_rs232_1
// interface_rs232_delete -n interface_rs232_2
// interface_rs232_delete -n interface_rs232_3

//333 OK
// upload_net_delete -n  upload_net_1
// upload_net_delete -n  upload_net_2
// upload_net_delete -n  upload_net_3
// interface_rs232_delete -n interface_rs232_1
// interface_rs232_delete -n interface_rs232_2
// interface_rs232_delete -n interface_rs232_3
// protocol_ev1_delete -n protocol_ev1_1
// protocol_ev1_delete -n protocol_ev1_2
// protocol_ev1_delete -n protocol_ev1_3

//222 OK
// interface_rs232_delete -n interface_rs232_1
// interface_rs232_delete -n interface_rs232_2
// interface_rs232_delete -n interface_rs232_3
// upload_net_delete -n  upload_net_1
// upload_net_delete -n  upload_net_2
// upload_net_delete -n  upload_net_3
// protocol_ev1_delete -n protocol_ev1_1
// protocol_ev1_delete -n protocol_ev1_2
// protocol_ev1_delete -n protocol_ev1_3

//111 OK
// interface_rs232_delete -n interface_rs232_1
// interface_rs232_delete -n interface_rs232_2
// interface_rs232_delete -n interface_rs232_3
// protocol_ev1_delete -n protocol_ev1_1
// protocol_ev1_delete -n protocol_ev1_2
// protocol_ev1_delete -n protocol_ev1_3
// upload_net_delete -n  upload_net_1
// upload_net_delete -n  upload_net_2
// upload_net_delete -n  upload_net_3



