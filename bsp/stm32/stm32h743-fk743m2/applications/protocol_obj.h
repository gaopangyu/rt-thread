#ifndef __PROTOCOL_OBJ_H__
#define __PROTOCOL_OBJ_H__

#include "custom_obj.h"
#include "bind.h"
#include "lib.h"

#define PROTOCOL_LOGV1_PARENT_NAME "protocol_logv1_parent"

struct protocol_obj
{
    struct custom_obj    parent;
    const struct protocol_obj_ops *ops;
};

typedef struct protocol_obj *protocol_obj_t;

struct protocol_obj_ops
{
    protocol_obj_t (*create)   (protocol_obj_t self, char *name, void *config);
    rt_err_t    (*delete)   (protocol_obj_t self);
    double_ret    (*set_param)  (protocol_obj_t self, void *config);
};

#endif /* __PROTOCOL_OBJ_H__ */