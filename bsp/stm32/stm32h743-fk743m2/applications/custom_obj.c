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
    if (arg == RT_NULL || arg[0] != '-' || arg[1] == '\0' || arg[2] != '\0')
        return RT_FALSE;

    if (flag_only_opts == RT_NULL)
        return RT_FALSE;

    for (const char *p = flag_only_opts; *p != '\0'; p++)
    {
        if (arg[1] == *p)
            return RT_TRUE;
    }

    return RT_FALSE;
}

const char *custom_obj_msh_get_pos(int argc, char *argv[], int index, const char *flag_only_opts)
{
    int pos_count = 0;

    if (index < 0)
        return RT_NULL;

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];

        if (arg[0] == '-')
        {
            if (custom_obj_msh_is_flag_only_opt(arg, flag_only_opts))
                continue;

            if (i + 1 < argc)
                i++;

            continue;
        }

        if (pos_count == index)
            return arg;

        pos_count++;
    }

    return RT_NULL;
}

static rt_err_t custom_obj_msh_check_name_common(const char *name)
{
    rt_size_t name_len;

    if (name == RT_NULL || name[0] == '-')
        return -RT_EINVAL;

    name_len = rt_strnlen(name, RT_NAME_MAX);
    if (name_len < 7 || name_len >= RT_NAME_MAX)
        return -RT_EINVAL;

    if (rt_strncmp(name + name_len - 7, "_parent", RT_NAME_MAX) == 0)
        return -RT_EINVAL;

    return RT_EOK;
}

rt_err_t custom_obj_msh_check_create_name(const char *name, const char *type_prefix)
{
    rt_err_t err = custom_obj_msh_check_name_common(name);

    if (err != RT_EOK)
        return err;

    if (type_prefix != RT_NULL &&
        rt_strncmp(name, type_prefix, rt_strnlen(type_prefix, RT_NAME_MAX)) != 0)
        return -RT_EINVAL;

    if (rt_object_find(name, RT_Object_Class_Custom) != RT_NULL)
        return -RT_EFULL;

    return RT_EOK;
}

rt_err_t custom_obj_msh_check_delete_name(const char *name)
{
    return custom_obj_msh_check_name_common(name);
}

