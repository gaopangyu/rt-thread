#include "console.h"
#include <rtdevice.h>
#include <board.h>

#define DBG_TAG "console"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#ifdef RT_USING_FINSH
#include <finsh.h>
#include <shell.h>
#endif

#include "usbd_core.h"
#include "usbd_cdc_acm.h"
#include "usbd_msc.h"
#include <string.h>

extern void usbd_cdc_acm_serial_flush(uint8_t busid);
#define USB0_BUSID 0
#ifndef USB0_FINSH_CDC_NAME
#define USB0_FINSH_CDC_NAME "usb-acm0"
#endif

#define CONSOLE_DEVICE_NET_NAME "uart1"

static enum console_device_type console_device = CONSOLE_DEVICE_UART;
static enum console_device_type console_device_prev = CONSOLE_DEVICE_UART;
static struct rt_semaphore console_usb_sem;
static volatile rt_bool_t console_usb_sem_ready = RT_FALSE;
static volatile rt_uint8_t console_usb_pend; /* 0 none, 1 off, 2 on */
static volatile rt_tick_t console_usb_last_sof;
static rt_bool_t console_usb_auto_ok = RT_TRUE;   /* 手动切走后插着 USB 不再抢控制台 */
static rt_bool_t console_set_by_monitor = RT_FALSE;

#define CONSOLE_USB_PEND_NONE   0
#define CONSOLE_USB_PEND_OFF    1
#define CONSOLE_USB_PEND_ON     2
#define CONSOLE_USB_SOF_LOST_MS 300

static const char *console_device_name(enum console_device_type device)
{
    switch (device)
    {
    case CONSOLE_DEVICE_UART:    return "uart";
    case CONSOLE_DEVICE_USB_CDC: return "usb";
    case CONSOLE_DEVICE_NET:     return "net";
    default:                     return "none";
    }
}

void console_usb_state_notify(rt_bool_t configured)
{
    console_usb_pend = configured ? CONSOLE_USB_PEND_ON : CONSOLE_USB_PEND_OFF;
    if (console_usb_sem_ready)
        rt_sem_release(&console_usb_sem);
}

void console_usb_sof_notify(void)
{
    console_usb_last_sof = rt_tick_get();
}

static rt_bool_t console_is_usb_cdc_bound(void)
{
#if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
    rt_device_t cons = rt_console_get_device();
    if (cons != RT_NULL && rt_strcmp(cons->parent.name, USB0_FINSH_CDC_NAME) == 0)
        return RT_TRUE;
#endif
#ifdef RT_USING_FINSH
    {
        const char *n = finsh_get_device();
        if (n != RT_NULL && rt_strcmp(n, USB0_FINSH_CDC_NAME) == 0)
            return RT_TRUE;
    }
#endif
    return RT_FALSE;
}

static void console_unbind_usb_cdc(void)
{
    if (!console_is_usb_cdc_bound())
        return;

#if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
#endif
#ifdef RT_USING_FINSH
    finsh_set_device(RT_CONSOLE_DEVICE_NAME);
#endif
}

void console_device_set(enum console_device_type device)
{
    switch (device) {
        case CONSOLE_DEVICE_UART:
        {
            rt_kprintf("set console device to: uart\r\n");

            #if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
                rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
            #endif

            #ifdef RT_USING_FINSH
                finsh_set_device(RT_CONSOLE_DEVICE_NAME);
            #endif

            rt_kprintf("set console device from: %s\r\n", console_device_name(console_device));
            break;
        }
        case CONSOLE_DEVICE_USB_CDC:
        {
            int i;

            /* 重枚举后仍是同一个 usb-acm0，set_device 会直接返回，必须先卸再挂才能 reopen */
            console_unbind_usb_cdc();

            for (i = 0; i <= 500; i++)
            {
                if (i == 500)
                {
                    rt_kprintf("set console device: error: usb_cdc not configured\r\n");
                    return;
                }

                if (usb_device_is_configured(USB0_BUSID))
                {
                    rt_kprintf("set console device to: usb_cdc \r\n");
                    usbd_cdc_acm_serial_flush(USB0_BUSID);

                    #if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
                        rt_console_set_device(USB0_FINSH_CDC_NAME);
                    #endif

                    #ifdef RT_USING_FINSH
                        finsh_set_device(USB0_FINSH_CDC_NAME);
                    #endif

                    rt_kprintf("set console device from: %s\r\n", console_device_name(console_device));
                    break;
                }
                rt_thread_mdelay(20);
            }

            break;
        }
        case CONSOLE_DEVICE_NET:
        {
            rt_device_t uart = rt_device_find(CONSOLE_DEVICE_NET_NAME);
            if (uart == RT_NULL)
                return;

            struct serial_configure serial_cfg;
            rt_err_t err = rt_device_control(uart, RT_SERIAL_CTRL_GET_CONFIG, &serial_cfg);
            if (err != RT_EOK)
                return;

            serial_cfg.baud_rate = BAUD_RATE_2000000;
            rt_device_control(uart, RT_DEVICE_CTRL_CONFIG, &serial_cfg);

            rt_kprintf("set console device to: upload\r\n");

            #if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
                rt_console_set_device(CONSOLE_DEVICE_NET_NAME);
            #endif

            #ifdef RT_USING_FINSH
                finsh_set_device(CONSOLE_DEVICE_NET_NAME);
            #endif

            rt_kprintf("set console device from: %s\r\n", console_device_name(console_device));
            break;
        }
        default:
            return;
    }

    if (device != CONSOLE_DEVICE_USB_CDC)
        console_device_prev = device;
    console_device = device;
    if (!console_set_by_monitor)
        console_usb_auto_ok = (device == CONSOLE_DEVICE_USB_CDC) ? RT_TRUE : RT_FALSE;
}

enum console_device_type console_device_get(void)
{
    return console_device;
}

static void console_usb_monitor(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        rt_uint8_t pend;
        rt_bool_t cfg;
        rt_bool_t sof_lost;

        rt_sem_take(&console_usb_sem, rt_tick_from_millisecond(200));

        pend = console_usb_pend;
        console_usb_pend = CONSOLE_USB_PEND_NONE;
        cfg = usb_device_is_configured(USB0_BUSID) ? RT_TRUE : RT_FALSE;

        sof_lost = RT_FALSE;
        if (console_device == CONSOLE_DEVICE_USB_CDC && console_usb_last_sof != 0)
        {
            if ((rt_tick_get() - console_usb_last_sof) > rt_tick_from_millisecond(CONSOLE_USB_SOF_LOST_MS))
                sof_lost = RT_TRUE;
        }

        /* 拔线：DTR=0 / disconnect / 连续无 SOF。不要用 RESET/configured=0（下次插入也会发生） */
        if (pend == CONSOLE_USB_PEND_OFF || sof_lost)
        {
            if (console_device == CONSOLE_DEVICE_USB_CDC)
            {
                enum console_device_type prev = console_device_prev;

                if (prev == CONSOLE_DEVICE_NONE || prev == CONSOLE_DEVICE_USB_CDC)
                    prev = CONSOLE_DEVICE_UART;
                LOG_I("usb disconnected, console -> prev");
                console_set_by_monitor = RT_TRUE;
                console_device_set(prev);
                console_set_by_monitor = RT_FALSE;
            }
            console_usb_auto_ok = RT_TRUE;
            console_usb_last_sof = 0;
        }

        /* 主机打开 COM（DTR）后再切 USB，避免枚举未完成就发数据导致回显错位 */
        if (pend == CONSOLE_USB_PEND_ON && console_usb_auto_ok)
        {
            if (!cfg)
            {
                rt_thread_mdelay(50);
                cfg = usb_device_is_configured(USB0_BUSID) ? RT_TRUE : RT_FALSE;
            }
            if (!cfg || !console_usb_auto_ok)
                continue;
            if (console_device != CONSOLE_DEVICE_USB_CDC && console_device != CONSOLE_DEVICE_NONE)
                console_device_prev = console_device;
            rt_thread_mdelay(50);
            if (!usb_device_is_configured(USB0_BUSID) || !console_usb_auto_ok)
                continue;
            LOG_I("usb dtr, console -> usb_cdc");
            console_set_by_monitor = RT_TRUE;
            console_device_set(CONSOLE_DEVICE_USB_CDC);
            console_set_by_monitor = RT_FALSE;
            console_usb_last_sof = rt_tick_get();
        }
    }
}

static int console_init(void)
{
    rt_thread_t tid;

    rt_sem_init(&console_usb_sem, "cons_usb", 0, RT_IPC_FLAG_PRIO);
    console_usb_sem_ready = RT_TRUE;

    tid = rt_thread_create("console_usb_monitor", console_usb_monitor, RT_NULL, 2048, 25, 10);
    if (tid == RT_NULL)
    {
        LOG_E("create console_usb_monitor thread failed");
        return -RT_ERROR;
    }
    rt_thread_startup(tid);
    return RT_EOK;
}
INIT_ENV_EXPORT(console_init);

#ifdef RT_USING_FINSH
#include <finsh.h>
#include <stdlib.h>

static int console_device_from_str(const char *s, enum console_device_type *out)
{
    unsigned long v;
    char *end = RT_NULL;

    if (s == RT_NULL || s[0] == '\0' || out == RT_NULL)
        return -1;

    if (rt_strcmp(s, "uart") == 0)
    {
        *out = CONSOLE_DEVICE_UART;
        return 0;
    }
    if (rt_strcmp(s, "usb") == 0 || rt_strcmp(s, "usb_cdc") == 0)
    {
        *out = CONSOLE_DEVICE_USB_CDC;
        return 0;
    }
    if (rt_strcmp(s, "upload") == 0)
    {
        *out = CONSOLE_DEVICE_NET;
        return 0;
    }

    v = strtoul(s, &end, 0);
    if (end == s || *end != '\0' || v <= CONSOLE_DEVICE_NONE || v >= CONSOLE_DEVICE_MAX)
        return -1;

    *out = (enum console_device_type)v;
    return 0;
}

static int cmd_console_device_get(int argc, char **argv)
{
    enum console_device_type device;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    device = console_device_get();
    rt_kprintf("console: %s (%d)\n", console_device_name(device), (int)device);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_console_device_get, console_device_get, Show console device);

static int cmd_console_device_set(int argc, char **argv)
{
    enum console_device_type device;

    if (argc < 2)
    {
        rt_kprintf("Usage: console_device_set <uart|usb|upload>\n");
        return -RT_ERROR;
    }

    if (console_device_from_str(argv[1], &device) != 0)
    {
        rt_kprintf("Invalid console device: %s\n", argv[1]);
        rt_kprintf("Usage: console_device_set <uart|usb|upload>\n");
        return -RT_ERROR;
    }

    console_device_set(device);
    device = console_device_get();
    rt_kprintf("console: %s (%d)\n", console_device_name(device), (int)device);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_console_device_set, console_device_set, Set console device: uart|usb|upload);
#endif