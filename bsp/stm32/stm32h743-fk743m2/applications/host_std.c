#include "host_std.h"

rt_err_t create_host_std(host_std_config_t config)
{
    if(config == RT_NULL)
        return RT_ERROR;

    if(config->host_id >= HOST_MAX || config->host_type != HOST_TYPE_STD)
        return RT_ERROR;

    if(g_host[config->host_id] != RT_NULL)
        return RT_EOK;

    host_std_t host_std = rt_calloc(1, sizeof(host_std_t));
    if(host_std == RT_NULL)
        return RT_ERROR;

    host_t host = (host_t)host_std;

    host->id = config->host_id;
    host->type = HOST_TYPE_STD;



    g_host[host->id] = host;

    return RT_EOK;

exit:

    rt_free(host);
    return RT_ERROR;
}