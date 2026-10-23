#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <rtthread.h>

enum console_device_type
{
    CONSOLE_DEVICE_NONE = 0,
    CONSOLE_DEVICE_UART,
    CONSOLE_DEVICE_USB_CDC,
    CONSOLE_DEVICE_NET,
    CONSOLE_DEVICE_MAX
};
typedef enum console_device_type console_device_type_t;

void console_device_set(enum console_device_type);
enum console_device_type console_device_get(void);
void console_usb_state_notify(rt_bool_t configured);
void console_usb_sof_notify(void);

#endif /* __CONSOLE_H__ */