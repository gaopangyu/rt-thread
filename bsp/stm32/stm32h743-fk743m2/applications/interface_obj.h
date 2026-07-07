#ifndef __INTERFACE_OBJ_H__
#define __INTERFACE_OBJ_H__

#include <rtthread.h>
#include <rtdevice.h>
#include "drv_usart_v2.h"
#include "custom_obj.h"
#include "bind.h"

#define INTRFACE_RS232_PARENT_NAME "interface_rs232_parent"

struct interface_obj
{
    struct custom_obj    parent;
    const struct interface_obj_ops *ops;
};

typedef struct interface_obj *interface_obj_t;

struct interface_obj_ops
{
    interface_obj_t (*create)   (interface_obj_t self, char *name, void *config);
    rt_err_t    (*delete)   (interface_obj_t self);
    double_ret    (*set_param)  (interface_obj_t self, void *config);
};

#endif /* __INTERFACE_OBJ_H__ */