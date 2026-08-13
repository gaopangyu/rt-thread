#ifndef __UPLOAD_OBJ_H__
#define __UPLOAD_OBJ_H__

#include <rtthread.h>
#include <rtdevice.h>
#include <time.h>
#include "drv_usart_v2.h"
#include "custom_obj.h"
#include "bind.h"
#include "protocol_obj.h"

#define UPLOAD_MUART_PARENT_NAME    "upload_muart_parent"
#define UPLOAD_STORE_PARENT_NAME    "upload_store_parent"
#define UPLOAD_SCREEN_PARENT_NAME   "upload_screen_parent"

enum event_type
{
    EVENT_TYPE_NONE = 0,
    EVENT_TYPE_FIRE,
    EVENT_TYPE_FAULT,
    EVENT_TYPE_MAX,
    EVENT_TYPE_ALL = 0xFF,
};

struct sub_history
{
    rt_uint8_t host_id;
    enum event_type event_type;
    rt_bool_t time_increases;
    time_t start_time;
    time_t end_time;
    rt_uint8_t count;
};
typedef struct sub_history *sub_history_t;

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
    rt_size_t   (*get_history) (upload_obj_t self, rt_uint8_t *buffer, rt_size_t size, sub_history_t sub_history);
};

void upload_obj_init(custom_obj_t custom_obj);

#endif /* __UPLOAD_OBJ_H__ */