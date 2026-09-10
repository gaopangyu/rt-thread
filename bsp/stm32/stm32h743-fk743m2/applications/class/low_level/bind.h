#ifndef __BIND_H__
#define __BIND_H__

#include "custom_obj.h"

rt_err_t bind_create(custom_obj_t pre_obj, custom_obj_t next_obj, rt_bool_t is_rx);
rt_err_t bind_delete(custom_obj_t pre_obj, custom_obj_t next_obj, rt_bool_t is_rx);
void bind_delete_all_for_obj(custom_obj_t self);

rt_bool_t get_bind_status(custom_obj_t pre_obj, custom_obj_t next_obj, rt_bool_t is_rx);

#endif /* __BIND_H__ */