#include "console.h"
#include <rtdevice.h>
#include <board.h>

#define DBG_TAG "console"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

#include "usbd_core.h"
#include "usbd_cdc_acm.h"
#include "usbd_msc.h"
#define USB0_BUSID 0
#ifndef USB0_FINSH_CDC_NAME
#define USB0_FINSH_CDC_NAME "usb-acm0"
#endif

#define CONSOLE_DEVICE_UPLOAD_NAME "uart1"

static enum console_device_type console_device = CONSOLE_DEVICE_UART;

void set_console_device(enum console_device_type device)
{
    switch (device) {
        case CONSOLE_DEVICE_UART:
        {
            #if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
                rt_kprintf("set console device: uart\r\n");
                rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
            #endif

            #ifdef RT_USING_FINSH
                rt_kprintf("set finsh device: uart\r\n");
                finsh_set_device(RT_CONSOLE_DEVICE_NAME);
            #endif

            break;
        }
        case CONSOLE_DEVICE_USB_CDC:
        {
            int i;
            for (i = 0; i <= 500; i++) 
            {
                if (i == 500)
                {
                    rt_kprintf("set console device: error: usb_cdc not configured\r\n");
                    return;
                }

                if (usb_device_is_configured(USB0_BUSID)) 
                {
                    #if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
                        rt_kprintf("set console device: usb_cdc\r\n");
                        rt_console_set_device(USB0_FINSH_CDC_NAME);
                    #endif

                    #ifdef RT_USING_FINSH
                        rt_kprintf("set finsh device: usb_cdc\r\n");
                        finsh_set_device(USB0_FINSH_CDC_NAME);
                    #endif

                    break;
                }
                rt_thread_mdelay(20);
            }

            break;
        }
        case CONSOLE_DEVICE_UPLOAD:
        {
            rt_device_t uart = rt_device_find(CONSOLE_DEVICE_UPLOAD_NAME);
            if (uart == RT_NULL)
                return;

            struct serial_configure serial_cfg;
            rt_err_t err = rt_device_control(uart, RT_SERIAL_CTRL_GET_CONFIG, &serial_cfg);
            if (err != RT_EOK)
                return;

            serial_cfg.baud_rate = BAUD_RATE_2000000;
            rt_device_control(uart, RT_DEVICE_CTRL_CONFIG, &serial_cfg);

            #if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
                rt_kprintf("set console device: upload\r\n");
                rt_console_set_device(CONSOLE_DEVICE_UPLOAD_NAME);
            #endif

            #ifdef RT_USING_FINSH
                rt_kprintf("set finsh device: upload\r\n");
                finsh_set_device(CONSOLE_DEVICE_UPLOAD_NAME);
            #endif

            break;
        }
        default:
            return;
    }

    console_device = device;
}

enum console_device_type get_console_device(void)
{
    return console_device;
}

#ifdef RT_USING_FINSH
#include <finsh.h>
#include <stdlib.h>

static const char *console_device_name(enum console_device_type device)
{
    switch (device)
    {
    case CONSOLE_DEVICE_UART:    return "uart";
    case CONSOLE_DEVICE_USB_CDC: return "usb";
    case CONSOLE_DEVICE_UPLOAD:  return "upload";
    default:                     return "none";
    }
}

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
        *out = CONSOLE_DEVICE_UPLOAD;
        return 0;
    }

    v = strtoul(s, &end, 0);
    if (end == s || *end != '\0' || v <= CONSOLE_DEVICE_NONE || v >= CONSOLE_DEVICE_MAX)
        return -1;

    *out = (enum console_device_type)v;
    return 0;
}

static int cmd_get_console_device(int argc, char **argv)
{
    enum console_device_type device;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    device = get_console_device();
    rt_kprintf("console: %s (%d)\n", console_device_name(device), (int)device);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_get_console_device, get_console_device, Show console device);

static int cmd_set_console_device(int argc, char **argv)
{
    enum console_device_type device;

    if (argc < 2)
    {
        rt_kprintf("Usage: set_console_device <uart|usb|upload>\n");
        return -RT_ERROR;
    }

    if (console_device_from_str(argv[1], &device) != 0)
    {
        rt_kprintf("Invalid console device: %s\n", argv[1]);
        rt_kprintf("Usage: set_console_device <uart|usb|upload>\n");
        return -RT_ERROR;
    }

    set_console_device(device);
    device = get_console_device();
    rt_kprintf("console: %s (%d)\n", console_device_name(device), (int)device);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_set_console_device, set_console_device, Set console device: uart|usb|upload);
#endif