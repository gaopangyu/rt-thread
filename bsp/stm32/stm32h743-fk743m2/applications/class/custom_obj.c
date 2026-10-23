#include "custom_obj.h"

static int custom_obj_event_flag_index(rt_uint32_t event_flag)
{
    if (event_flag == 0)
        return -1;

    return __builtin_ffs((int)event_flag) - 1;
}

static struct bind_node *custom_obj_bind_node(custom_obj_t obj, rt_bool_t is_rx)
{
    return is_rx ? &obj->rxb : &obj->txb;
}

static rt_size_t custom_obj_bind_sum_link_bytes(rt_atomic_t *link_bytes, rt_uint32_t mask)
{
    rt_size_t total = 0;
    int i;

    for (i = 0; i < CUSTOM_PER_LAYER_OBJ_MAX; i++)
    {
        if ((mask >> i) & 1)
            total += rt_atomic_load(&link_bytes[i]);
    }

    return total;
}

static rt_size_t custom_obj_bind_link_bytes(rt_atomic_t *link_bytes, rt_uint32_t event_flag)
{
    int index = custom_obj_event_flag_index(event_flag);

    if (index < 0)
        return 0;

    return rt_atomic_load(&link_bytes[index]);
}

static void custom_obj_bind_add_ext(custom_obj_t obj, rt_size_t len, rt_bool_t is_rx, rt_bool_t is_recv)
{
    struct bind_node *bind;

    if (obj == RT_NULL || len == 0)
        return;

    bind = custom_obj_bind_node(obj, is_rx);
    rt_atomic_add(is_recv ? &bind->ext_recv_bytes : &bind->ext_send_bytes, len);
}

rt_err_t custom_obj_apply_event_flag(custom_obj_t self)
{
    int i;

    if (self == RT_NULL)
        return -RT_EINVAL;

    rt_enter_critical();
    for (i = 0; i < CUSTOM_PER_LAYER_OBJ_MAX; i++)
    {
        rt_uint32_t event_flag = 1u << i;

        if ((*self->event_mask & event_flag) == 0)
        {
            self->event_flag = event_flag;
            *self->event_mask |= event_flag;
            self->obj_list[i] = self;
            rt_exit_critical();
            return RT_EOK;
        }
    }
    rt_exit_critical();

    return -RT_EINVAL;
}

rt_err_t custom_obj_unapply_event_flag(custom_obj_t self)
{
    int index;

    if (self == RT_NULL)
        return -RT_EINVAL;

    rt_enter_critical();
    index = custom_obj_event_flag_index(self->event_flag);
    if (index >= 0)
        self->obj_list[index] = RT_NULL;
    *self->event_mask &= ~self->event_flag;
    self->event_flag = 0;
    rt_exit_critical();

    return RT_EOK;
}

custom_obj_t custom_obj_get_by_event_flag(struct custom_obj **obj_list, rt_uint32_t *event_flag, rt_bool_t clear)
{
    int i;

    if (obj_list == RT_NULL || event_flag == RT_NULL)
        return RT_NULL;

    rt_enter_critical();
    for (i = 0; i < CUSTOM_PER_LAYER_OBJ_MAX; i++)
    {
        if ((*event_flag >> i) & 1)
        {
            if (clear)
                *event_flag &= ~(1u << i);
            rt_exit_critical();
            return obj_list[i];
        }
    }
    rt_exit_critical();

    return RT_NULL;
}

void custom_obj_bind_add_ext_recv(custom_obj_t obj, rt_size_t len, rt_bool_t is_rx)
{
    custom_obj_bind_add_ext(obj, len, is_rx, RT_TRUE);
}

void custom_obj_bind_add_ext_send(custom_obj_t obj, rt_size_t len, rt_bool_t is_rx)
{
    custom_obj_bind_add_ext(obj, len, is_rx, RT_FALSE);
}

void custom_obj_bind_xfer_link(custom_obj_t pre, custom_obj_t next, rt_size_t len, rt_bool_t is_rx)
{
    struct bind_node *pre_bind;
    struct bind_node *next_bind;
    int pre_idx;
    int next_idx;

    if (pre == RT_NULL || next == RT_NULL || len == 0)
        return;

    next_idx = custom_obj_event_flag_index(next->event_flag);
    pre_idx = custom_obj_event_flag_index(pre->event_flag);
    if (next_idx < 0 || pre_idx < 0)
        return;

    pre_bind = custom_obj_bind_node(pre, is_rx);
    next_bind = custom_obj_bind_node(next, is_rx);
    rt_atomic_add(&pre_bind->link_send_bytes[next_idx], len);
    rt_atomic_add(&next_bind->link_recv_bytes[pre_idx], len);
}

static rt_size_t custom_obj_bind_get_path_recv_bytes(custom_obj_t obj, custom_obj_t parent,
                                                     custom_obj_t branch_child, rt_bool_t is_rx)
{
    struct bind_node *bind;

    if (obj == RT_NULL)
        return 0;

    bind = custom_obj_bind_node(obj, is_rx);
    if (is_rx)
    {
        if (parent == RT_NULL)
            return (obj->type == Custom_Object_Class_Interface) ? rt_atomic_load(&bind->ext_recv_bytes) : 0;

        return custom_obj_bind_link_bytes(bind->link_recv_bytes, parent->event_flag);
    }

    if (parent == RT_NULL)
        return (obj->type == Custom_Object_Class_Interface) ?
               custom_obj_bind_sum_link_bytes(bind->link_recv_bytes, bind->next_event_mask) : 0;

    if (obj->type == Custom_Object_Class_Upload)
        return custom_obj_bind_link_bytes(bind->link_send_bytes, parent->event_flag);

    if (branch_child != RT_NULL)
        return custom_obj_bind_link_bytes(bind->link_recv_bytes, branch_child->event_flag);

    return custom_obj_bind_sum_link_bytes(bind->link_recv_bytes, bind->next_event_mask);
}

static rt_size_t custom_obj_bind_get_path_send_bytes(custom_obj_t obj, custom_obj_t parent, rt_bool_t is_rx)
{
    struct bind_node *bind;

    if (obj == RT_NULL)
        return 0;

    bind = custom_obj_bind_node(obj, is_rx);
    if (is_rx)
    {
        if (obj->type == Custom_Object_Class_Upload)
            return rt_atomic_load(&bind->ext_send_bytes);

        if (bind->next_event_mask == 0)
            return 0;

        return custom_obj_bind_sum_link_bytes(bind->link_send_bytes, bind->next_event_mask);
    }

    if (obj->type == Custom_Object_Class_Interface)
        return rt_atomic_load(&bind->ext_send_bytes);

    /* 发送方向的根节点，数据从 next 流向自己，记在 link_recv */
    if (parent == RT_NULL)
        return custom_obj_bind_sum_link_bytes(bind->link_recv_bytes, bind->next_event_mask);

    return custom_obj_bind_link_bytes(bind->link_send_bytes, parent->event_flag);
}

rt_size_t custom_obj_bind_format_path_label(char *buf, rt_size_t size, custom_obj_t obj,
                                            custom_obj_t parent, custom_obj_t branch_child,
                                            rt_bool_t is_rx)
{
    if (buf == RT_NULL || size == 0 || obj == RT_NULL)
        return 0;

    return rt_snprintf(buf, size, "(%d)%s(%d)",
                       (int)custom_obj_bind_get_path_recv_bytes(obj, parent, branch_child, is_rx),
                       ((rt_object_t)obj)->name,
                       (int)custom_obj_bind_get_path_send_bytes(obj, parent, is_rx));
}
