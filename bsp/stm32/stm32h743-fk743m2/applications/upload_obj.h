#ifndef __UPLOAD_OBJ_H__
#define __UPLOAD_OBJ_H__

#include <rtthread.h>
#include <rtdevice.h>
#include "drv_usart_v2.h"
#include "custom_obj.h"
#include "bind.h"
#include "protocol_obj.h"

#define UPLOAD_MUART_PARENT_NAME "upload_muart_parent"

struct upload_obj
{
    struct custom_obj    parent;
    const struct upload_obj_ops *ops;
};

typedef struct upload_obj *upload_obj_t;

struct upload_obj_ops
{
    upload_obj_t (*create)   (upload_obj_t self, char *name, void *config);
    rt_err_t    (*delete)   (upload_obj_t self);
    double_ret    (*set_param)  (upload_obj_t self, void *config);
};

#endif /* __UPLOAD_OBJ_H__ */