#ifndef __FUNNEL_OBJ_H__
#define __FUNNEL_OBJ_H__

#include "custom_obj.h"
#include "bind.h"
#include "lib.h"

#define FUNNEL_DEFAULT_PARENT_NAME "funnel_default_parent"

struct funnel_obj
{
    struct custom_obj    parent;
    const struct funnel_obj_ops *ops;
};

typedef struct funnel_obj *funnel_obj_t;

struct funnel_obj_ops
{
    funnel_obj_t (*create)   (funnel_obj_t self, char *name, void *config);
    rt_err_t    (*delete)   (funnel_obj_t self);
    double_ret    (*set_param)  (funnel_obj_t self, void *config);
};

void funnel_obj_init(custom_obj_t custom_obj);

#endif /* __FUNNEL_OBJ_H__ */