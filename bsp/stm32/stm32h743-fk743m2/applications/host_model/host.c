#include "host.h"
#include <string.h>

host_t g_host[HOST_MAX] = 
{
    0
};

char *g_com[COM_MAX] = 
{
    RT_NULL,
    INTRFACE_RS232_PARENT_NAME,
    INTRFACE_RS232_PARENT_NAME,
    INTRFACE_RS232_PARENT_NAME,
    INTRFACE_RS232_PARENT_NAME,
    RT_NULL,
    RT_NULL,
};

rt_err_t generate_name_from_parent(char *out, rt_size_t out_size, const char *parent_name, rt_uint8_t host_id, rt_uint8_t sub_id)
{
    if(out == RT_NULL || out_size == 0 || parent_name == RT_NULL)
        return -RT_EINVAL;

    rt_size_t len = strrchr(parent_name, '_') - parent_name + 1;

    if( len == 0 || len > out_size - 3)
        return -RT_ERROR;

    rt_memcpy(out, parent_name, len);
    out[len++] = 0x30 + host_id;
    out[len++] = 0x30 + sub_id;
    out[len] = 0;

    return RT_EOK;
}

const char *host_protocol_parent_name(const char *protocol_ver)
{
    if (protocol_ver == RT_NULL || protocol_ver[0] == '\0')
        return RT_NULL;

    if (rt_strcmp(protocol_ver, "ev1") == 0)
        return PROTOCOL_EV1_PARENT_NAME;

    return RT_NULL;
}