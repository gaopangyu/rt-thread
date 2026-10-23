#include "msh_util.h"

#include <stdlib.h>

static int msh_find_opt(int argc, char *argv[], const char *opt)
{
    rt_size_t opt_len;
    int i;

    if (opt == RT_NULL || opt[0] == '\0')
        return -1;

    opt_len = rt_strlen(opt);
    for (i = 1; i < argc; i++)
    {
        const char *arg = argv[i];

        if (arg[0] == '-' && rt_strncmp(arg + 1, opt, opt_len) == 0 && arg[opt_len + 1] == '\0')
            return i;
    }

    return -1;
}

static rt_bool_t msh_is_flag_only_opt(const char *arg, const char *flag_only_opts)
{
    const char *p;

    if (arg == RT_NULL || arg[0] != '-' || arg[1] == '\0' || arg[2] != '\0' || flag_only_opts == RT_NULL)
        return RT_FALSE;

    for (p = flag_only_opts; *p != '\0'; p++)
    {
        if (arg[1] == *p)
            return RT_TRUE;
    }

    return RT_FALSE;
}

const char *msh_get_opt(int argc, char *argv[], const char *opt)
{
    int index = msh_find_opt(argc, argv, opt);

    if (index < 0 || index + 1 >= argc)
        return RT_NULL;

    return argv[index + 1];
}

rt_bool_t msh_has_opt(int argc, char *argv[], const char *opt)
{
    return msh_find_opt(argc, argv, opt) >= 0;
}

const char *msh_get_pos(int argc, char *argv[], int index, const char *flag_only_opts)
{
    int pos = 0;
    int i;

    for (i = 1; i < argc; i++)
    {
        if (msh_is_flag_only_opt(argv[i], flag_only_opts))
            continue;

        if (argv[i][0] == '-' && argv[i][1] != '\0' && flag_only_opts != RT_NULL)
        {
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

rt_err_t msh_check_create_name(const char *name, const char *type_prefix)
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

rt_err_t msh_check_delete_name(const char *name)
{
    if (name == RT_NULL || name[0] == '\0')
        return -RT_EINVAL;

    if (rt_object_find(name, RT_Object_Class_Custom) == RT_NULL)
        return -RT_EINVAL;

    return RT_EOK;
}

int msh_parse_u32(const char *s, rt_uint32_t *out)
{
    char *end = RT_NULL;
    unsigned long v;

    if (s == RT_NULL || s[0] == '\0' || out == RT_NULL)
        return -1;

    v = strtoul(s, &end, 0);
    if (end == s || *end != '\0')
        return -1;

    *out = (rt_uint32_t)v;
    return 0;
}
