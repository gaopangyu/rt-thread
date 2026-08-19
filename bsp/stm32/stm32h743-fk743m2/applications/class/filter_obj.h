#ifndef __FILTER_OBJ_H__
#define __FILTER_OBJ_H__

#include "custom_obj.h"
#include "bind.h"
#include "lib.h"

#define FILTER_DEFAULT_PARENT_NAME "filter_default_parent"

struct filter_obj
{
    struct custom_obj    parent;
    const struct filter_obj_ops *ops;
};

typedef struct filter_obj *filter_obj_t;

struct filter_obj_ops
{
    filter_obj_t (*create)   (filter_obj_t self, char *name, void *config);
    rt_err_t    (*delete)   (filter_obj_t self);
    double_ret    (*set_param)  (filter_obj_t self, void *config);
};

void filter_obj_init(custom_obj_t custom_obj);

#endif /* __FILTER_OBJ_H__ */