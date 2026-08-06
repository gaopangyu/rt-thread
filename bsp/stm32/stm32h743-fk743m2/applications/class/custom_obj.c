#include "custom_obj.h"

rt_err_t apply_for_event_flag(custom_obj_t self)
{
    if (self == RT_NULL)
        return -RT_EINVAL;

    rt_enter_critical();
    for(int i = 0; i < CUSTOM_PER_LAYER_OBJ_MAX; i++)
    {
        rt_uint32_t event_flag = 1 << i;
        if((*self->event_mask & event_flag) == 0)
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

rt_err_t unapply_for_event_flag(custom_obj_t self)
{
    if (self == RT_NULL)
        return -RT_EINVAL;

    rt_enter_critical();
    self->obj_list[(__builtin_ffs(self->event_flag)-1)%CUSTOM_PER_LAYER_OBJ_MAX] = RT_NULL; 
    *self->event_mask &= ~self->event_flag;
    self->event_flag = 0;
    rt_exit_critical();

    return RT_EOK;
}

custom_obj_t get_obj_from_event_flag(custom_obj_t self, struct custom_obj **obj_list, rt_uint32_t *event_flag, rt_bool_t clear)
{
    if( self == RT_NULL || obj_list == RT_NULL || event_flag == RT_NULL )
        return RT_NULL;

    int index = -1;

    rt_enter_critical();
    for(int i = 0; i < CUSTOM_PER_LAYER_OBJ_MAX; i++)
    {
        if( (*event_flag >> i) & 1 )
        {
            if(clear)
                *event_flag &= ~(1 << i);
            index = i;
            break;
        }
    }
    rt_exit_critical();

    if( index < 0 )
        return RT_NULL;

    return obj_list[index];
}

const char *custom_obj_msh_get_opt(int argc, char *argv[], const char *opt)
{
    rt_size_t opt_len;

    if (opt == RT_NULL || opt[0] == '\0')
        return RT_NULL;

    opt_len = rt_strlen(opt);

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];

        if (arg[0] != '-' || rt_strncmp(arg + 1, opt, opt_len) != 0 || arg[opt_len + 1] != '\0')
            continue;

        if (i + 1 >= argc)
            return RT_NULL;

        return argv[i + 1];
    }

    return RT_NULL;
}

rt_bool_t custom_obj_msh_has_opt(int argc, char *argv[], const char *opt)
{
    rt_size_t opt_len;

    if (opt == RT_NULL || opt[0] == '\0')
        return RT_FALSE;

    opt_len = rt_strlen(opt);

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];

        if (arg[0] != '-' || rt_strncmp(arg + 1, opt, opt_len) != 0 || arg[opt_len + 1] != '\0')
            continue;

        return RT_TRUE;
    }

    return RT_FALSE;
}

static rt_bool_t custom_obj_msh_is_flag_only_opt(const char *arg, const char *flag_only_opts)
{
    const char *p;
    rt_size_t len;

    if (arg == RT_NULL || arg[0] != '-' || flag_only_opts == RT_NULL)
        return RT_FALSE;

    for (p = flag_only_opts; *p != '\0'; p++)
    {
        len = 1;
        if (rt_strncmp(arg + 1, p, len) == 0 && arg[len + 1] == '\0')
            return RT_TRUE;
    }

    return RT_FALSE;
}

const char *custom_obj_msh_get_pos(int argc, char *argv[], int index, const char *flag_only_opts)
{
    int pos = 0;

    for (int i = 1; i < argc; i++)
    {
        if (custom_obj_msh_is_flag_only_opt(argv[i], flag_only_opts))
            continue;

        if (argv[i][0] == '-' && argv[i][1] != '\0' && flag_only_opts != RT_NULL)
        {
            if (custom_obj_msh_is_flag_only_opt(argv[i], flag_only_opts))
                continue;
            if (i + 1 < argc)
                i++;
            continue;
        }

        if (pos == index)
            return argv[i];

        pos++;
    }

    return RT_NULL;
}

static rt_err_t custom_obj_msh_check_name_common(const char *name)
{
    if (name == RT_NULL || name[0] == '\0')
        return -RT_EINVAL;

    if (rt_object_find(name, RT_Object_Class_Custom) == RT_NULL)
        return -RT_EINVAL;

    return RT_EOK;
}

rt_err_t custom_obj_msh_check_create_name(const char *name, const char *type_prefix)
{
    rt_size_t prefix_len;

    if (name == RT_NULL || type_prefix == RT_NULL)
        return -RT_EINVAL;

    prefix_len = rt_strlen(type_prefix);
    if (rt_strncmp(name, type_prefix, prefix_len) != 0)
        return -RT_EINVAL;

    if (rt_object_find(name, RT_Object_Class_Custom) != RT_NULL)
        return -RT_EFULL;

    return RT_EOK;
}

rt_err_t custom_obj_msh_check_delete_name(const char *name)
{
    return custom_obj_msh_check_name_common(name);
}

static int custom_obj_bind_peer_index(rt_uint32_t event_flag)
{
    if (event_flag == 0)
        return -1;

    return __builtin_ffs((int)event_flag) - 1;
}

static struct bind_node *custom_obj_bind_node(custom_obj_t obj, rt_bool_t is_rx)
{
    return is_rx ? &obj->rxb : &obj->txb;
}

void custom_obj_bind_ext_add_recv(custom_obj_t obj, rt_size_t len, rt_bool_t is_rx)
{
    struct bind_node *bind;

    if (obj == RT_NULL || len == 0)
        return;

    bind = custom_obj_bind_node(obj, is_rx);
    rt_atomic_add(&bind->ext_recv_bytes, len);
}

void custom_obj_bind_ext_add_send(custom_obj_t obj, rt_size_t len, rt_bool_t is_rx)
{
    struct bind_node *bind;

    if (obj == RT_NULL || len == 0)
        return;

    bind = custom_obj_bind_node(obj, is_rx);
    rt_atomic_add(&bind->ext_send_bytes, len);
}

void custom_obj_bind_link_xfer(custom_obj_t pre, custom_obj_t next, rt_size_t len, rt_bool_t is_rx)
{
    struct bind_node *pre_bind;
    struct bind_node *next_bind;
    int pre_idx;
    int next_idx;

    if (pre == RT_NULL || next == RT_NULL || len == 0)
        return;

    next_idx = custom_obj_bind_peer_index(next->event_flag);
    pre_idx = custom_obj_bind_peer_index(pre->event_flag);
    if (next_idx < 0 || pre_idx < 0)
        return;

    pre_bind = custom_obj_bind_node(pre, is_rx);
    next_bind = custom_obj_bind_node(next, is_rx);
    rt_atomic_add(&pre_bind->link_send_bytes[next_idx], len);
    rt_atomic_add(&next_bind->link_recv_bytes[pre_idx], len);
}

static rt_size_t custom_obj_bind_sum_link_bytes(rt_atomic_t *link_bytes, rt_uint32_t mask)
{
    rt_size_t total = 0;

    for (int i = 0; i < CUSTOM_PER_LAYER_OBJ_MAX; i++)
    {
        if ((mask >> i) & 1)
            total += rt_atomic_load(&link_bytes[i]);
    }

    return total;
}

rt_size_t custom_obj_bind_path_recv_bytes(custom_obj_t obj, custom_obj_t parent,
                                          custom_obj_t branch_child, rt_bool_t is_rx)
{
    struct bind_node *bind;
    int idx;

    if (obj == RT_NULL)
        return 0;

    bind = custom_obj_bind_node(obj, is_rx);
    if (is_rx)
    {
        if (parent == RT_NULL)
        {
            if (obj->type == Custom_Object_Class_Interface)
                return rt_atomic_load(&bind->ext_recv_bytes);
            return 0;
        }

        idx = custom_obj_bind_peer_index(parent->event_flag);
        if (idx < 0)
            return 0;

        return rt_atomic_load(&bind->link_recv_bytes[idx]);
    }

    if (parent == RT_NULL)
    {
        if (obj->type == Custom_Object_Class_Interface)
            return custom_obj_bind_sum_link_bytes(bind->link_recv_bytes, bind->next_event_mask);
        return 0;
    }

    if (obj->type == Custom_Object_Class_Upload)
    {
        idx = custom_obj_bind_peer_index(parent->event_flag);
        if (idx < 0)
            return 0;

        return rt_atomic_load(&bind->link_send_bytes[idx]);
    }

    if (branch_child != RT_NULL)
    {
        idx = custom_obj_bind_peer_index(branch_child->event_flag);
        if (idx < 0)
            return 0;

        return rt_atomic_load(&bind->link_recv_bytes[idx]);
    }

    return custom_obj_bind_sum_link_bytes(bind->link_recv_bytes, bind->next_event_mask);
}

rt_size_t custom_obj_bind_path_send_bytes(custom_obj_t obj, custom_obj_t parent, rt_bool_t is_rx)
{
    struct bind_node *bind;
    int idx;

    if (obj == RT_NULL)
        return 0;

    bind = custom_obj_bind_node(obj, is_rx);
    if (is_rx)
    {
        if (obj->type == Custom_Object_Class_Upload)
            return rt_atomic_load(&bind->ext_send_bytes);

        if (bind->next_event_mask != 0)
            return custom_obj_bind_sum_link_bytes(bind->link_send_bytes, bind->next_event_mask);

        return 0;
    }

    if (obj->type == Custom_Object_Class_Interface)
        return rt_atomic_load(&bind->ext_send_bytes);

    /* TX root (e.g. protocol_cmd): data flows next -> self, counted in link_recv */
    if (parent == RT_NULL)
        return custom_obj_bind_sum_link_bytes(bind->link_recv_bytes, bind->next_event_mask);

    idx = custom_obj_bind_peer_index(parent->event_flag);
    if (idx < 0)
        return 0;

    return rt_atomic_load(&bind->link_send_bytes[idx]);
}

rt_size_t custom_obj_bind_format_path_label(char *buf, rt_size_t size, custom_obj_t obj,
                                            custom_obj_t parent, custom_obj_t branch_child,
                                            rt_bool_t is_rx)
{
    if (buf == RT_NULL || size == 0 || obj == RT_NULL)
        return 0;

    return rt_snprintf(buf, size, "(%d)%s(%d)",
                       (int)custom_obj_bind_path_recv_bytes(obj, parent, branch_child, is_rx),
                       ((rt_object_t)obj)->name,
                       (int)custom_obj_bind_path_send_bytes(obj, parent, is_rx));
}
