#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <rtthread.h>

enum console_device_type
{
    CONSOLE_DEVICE_NONE = 0,
    CONSOLE_DEVICE_UART,
    CONSOLE_DEVICE_USB_CDC,
    CONSOLE_DEVICE_UPLOAD,
    CONSOLE_DEVICE_MAX
};
typedef enum console_device_type console_device_type_t;

void set_console_device(enum console_device_type);
enum console_device_type get_console_device(void);

#endif /* __CONSOLE_H__ */