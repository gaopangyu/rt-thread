#include "host_std.h"
#include "store.h"
#include "json_util.h"

#include <string.h>

#define DBG_TAG "host.std"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static rt_bool_t host_std_net_rx_bind_status(host_t self)
{
    if (self == RT_NULL)
        return RT_FALSE;

    host_std_t host_std = (host_std_t)self;

    if (host_std->funnel_default == RT_NULL || host_std->upload_net == RT_NULL)
        return RT_FALSE;

    return bind_get_status((custom_obj_t)host_std->funnel_default,
                           (custom_obj_t)host_std->upload_net,
                           RT_TRUE);
}

static rt_err_t host_std_net_rx_bind_delete(host_t self)
{
    if(self == RT_NULL)
        return -RT_EINVAL;

    host_t host = self;
    host_std_t host_std = (host_std_t)host;

    bind_delete((custom_obj_t)host_std->funnel_default, (custom_obj_t)host_std->upload_net, RT_TRUE);

    return RT_EOK;
}

static rt_err_t host_std_net_rx_bind_create(host_t self)
{
    if(self == RT_NULL)
        return -RT_EINVAL;

    host_t host = self;
    host_std_t host_std = (host_std_t)host;

    bind_create((custom_obj_t)host_std->funnel_default, (custom_obj_t)host_std->upload_net, RT_TRUE);

    return RT_EOK;
}
static rt_size_t host_std_get_history(host_t self, rt_uint8_t *buffer, rt_size_t size, event_history_t event_history, rt_bool_t *is_end)
{
    if (self == RT_NULL)
        return event_put_end(buffer, size, is_end);

    return host_upload_get_history(((host_std_t)self)->upload_store, buffer, size, event_history, is_end);
}

static rt_err_t host_std_delete(host_t self)
{
    if (self == RT_NULL || self->id >= HOST_MAX)
        return -RT_EINVAL;

    host_t host = self;
    host_std_t host_std = (host_std_t)host;

    g_host[host->id] = RT_NULL;

    host_obj_delete(&host_std->funnel_default);
    host_obj_delete(&host_std->upload_store);
    host_obj_delete(&host_std->upload_net);
    host_obj_delete(&host_std->protocol_crt);
    host_obj_delete(&host_std->protocol_bus);
    host_obj_delete(&host_std->interface_crt);
    host_obj_delete(&host_std->interface_bus);

    rt_free(host_std);
    return RT_EOK;
}

static const struct host_ops host_std_ops =
{
    .net_rx_bind_status = host_std_net_rx_bind_status,
    .net_rx_bind_delete = host_std_net_rx_bind_delete,
    .net_rx_bind_create = host_std_net_rx_bind_create,
    .get_history = host_std_get_history,
    .host_delete = host_std_delete,
};

rt_err_t host_std_create(host_std_config_t config)
{
    if (config == RT_NULL)
        return -RT_EINVAL;

    if (config->host_id >= HOST_MAX || config->crt_com_id >= COM_MAX 
     || config->bus_com_id >= COM_MAX || config->host_type != HOST_TYPE_STD)
        return -RT_EINVAL;

    if (g_host[config->host_id] != RT_NULL)
        return RT_EOK;

    host_std_t host_std = (host_std_t)rt_calloc(1, sizeof(struct host_std));
    if (host_std == RT_NULL)
        return -RT_ENOMEM;

    host_t host = (host_t)host_std;
    host->id = config->host_id;
    host->type = HOST_TYPE_STD;

    const char *protocol_parent_name = host_protocol_parent_name(config->protocol_ver);

    if (protocol_parent_name == RT_NULL)
        LOG_W("host_std %d unsupported protocol_ver: %s", config->host_id, config->protocol_ver);

    if (config->interface_crt_cfg != RT_NULL)
        host_std->interface_crt = host_obj_create(g_com[config->crt_com_id], config->host_id,
                                               config->crt_com_id, config->interface_crt_cfg);
    if (config->interface_bus_cfg != RT_NULL)
        host_std->interface_bus = host_obj_create(g_com[config->bus_com_id], config->host_id,
                                               config->bus_com_id, config->interface_bus_cfg);
    if (config->protocol_crt_cfg != RT_NULL && host_std->interface_crt != RT_NULL)
        host_std->protocol_crt = host_obj_create(protocol_parent_name, config->host_id,
                                              config->crt_com_id, config->protocol_crt_cfg);
    if (config->protocol_bus_cfg != RT_NULL && host_std->interface_bus != RT_NULL)
        host_std->protocol_bus = host_obj_create(protocol_parent_name, config->host_id,
                                              config->bus_com_id, config->protocol_bus_cfg);
    if (config->funnel_default_cfg != RT_NULL)
        host_std->funnel_default = host_obj_create(FUNNEL_DEFAULT_PARENT_NAME, config->host_id,
                                                'x' - 0x30, config->funnel_default_cfg);

    if (config->upload_store_cfg != RT_NULL)
    {
        upload_store_config_t store_cfg = (upload_store_config_t)config->upload_store_cfg;

        rt_strncpy(store_cfg->dir, config->protocol_ver, sizeof(store_cfg->dir) - 1);
        store_cfg->dir[sizeof(store_cfg->dir) - 1] = '\0';
        host_std->upload_store = host_obj_create(UPLOAD_STORE_PARENT_NAME, config->host_id,
                                              'x' - 0x30, config->upload_store_cfg);
    }

    if (config->upload_net_cfg != RT_NULL)
        host_std->upload_net = host_obj_create(UPLOAD_NET_PARENT_NAME, config->host_id,
                                            'x' - 0x30, config->upload_net_cfg);


    bind_create((custom_obj_t)host_std->interface_crt, (custom_obj_t)host_std->protocol_crt, RT_TRUE);
    bind_create((custom_obj_t)host_std->protocol_crt, (custom_obj_t)host_std->funnel_default, RT_TRUE);

    bind_create((custom_obj_t)host_std->interface_bus, (custom_obj_t)host_std->protocol_bus, RT_TRUE);
    bind_create((custom_obj_t)host_std->protocol_bus, (custom_obj_t)host_std->funnel_default, RT_TRUE);

    bind_create((custom_obj_t)host_std->funnel_default, (custom_obj_t)host_std->upload_store, RT_TRUE);
    bind_create((custom_obj_t)host_std->funnel_default, (custom_obj_t)host_std->upload_net , RT_TRUE);

    bind_create((custom_obj_t)host_std->interface_bus, (custom_obj_t)host_std->protocol_bus, RT_FALSE);
    bind_create((custom_obj_t)host_std->protocol_bus, (custom_obj_t)host_std->funnel_default, RT_FALSE);
    bind_create((custom_obj_t)host_std->funnel_default, (custom_obj_t)host_std->upload_net , RT_FALSE);

    host->ops = (struct host_ops *)&host_std_ops;

    g_host[host->id] = host;
    LOG_I("host_std %d created", host->id);
    return RT_EOK;

exit:
    LOG_E("host_std %d create failed", config->host_id);
    host_std_delete(host);
    return -RT_ERROR;
}

static rt_err_t host_std_load_json(cJSON *host_json, host_std_config_t host_std_cfg,
                                   struct interface_rs232_config *interface_bus_cfg,
                                   struct interface_rs232_config *interface_crt_cfg,
                                   struct protocol_ev1_config *protocol_bus_cfg,
                                   struct protocol_ev1_config *protocol_crt_cfg,
                                   struct upload_store_config *upload_store_cfg,
                                   struct funnel_default_config *funnel_default_cfg,
                                   struct upload_net_config *upload_net_cfg)
{
    rt_uint32_t v;
    cJSON *item;

    if (host_json == RT_NULL || host_std_cfg == RT_NULL)
        return -RT_EINVAL;

    if (!json_get_u32(host_json, "id", &v) || v >= HOST_MAX)
    {
        LOG_E("std host id must < HOST_MAX(%d)", HOST_MAX);
        return -RT_EINVAL;
    }

    host_std_cfg->host_type = HOST_TYPE_STD;
    host_std_cfg->host_id = (rt_uint8_t)v;
    json_copy_str(host_std_cfg->protocol_ver, sizeof(host_std_cfg->protocol_ver), host_json, "protocol_ver");
    if (host_std_cfg->protocol_ver[0] == '\0')
        rt_strncpy(host_std_cfg->protocol_ver, "ev1", sizeof(host_std_cfg->protocol_ver) - 1);

    item = cJSON_GetObjectItemCaseSensitive(host_json, "interface_crt");
    if (cJSON_IsObject(item))
    {
        if (json_get_u32(item, "id", &v))
            host_std_cfg->crt_com_id = (rt_uint8_t)v;
        host_parse_rs232(item, interface_crt_cfg);
        host_std_cfg->interface_crt_cfg = interface_crt_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "protocol_crt");
    if (cJSON_IsObject(item))
    {
        host_parse_protocol_ev1(item, protocol_crt_cfg);
        host_std_cfg->protocol_crt_cfg = protocol_crt_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "interface_bus");
    if (cJSON_IsObject(item))
    {
        if (json_get_u32(item, "id", &v))
            host_std_cfg->bus_com_id = (rt_uint8_t)v;
        host_parse_rs232(item, interface_bus_cfg);
        host_std_cfg->interface_bus_cfg = interface_bus_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "protocol_bus");
    if (cJSON_IsObject(item))
    {
        host_parse_protocol_ev1(item, protocol_bus_cfg);
        host_std_cfg->protocol_bus_cfg = protocol_bus_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "funnel_default");
    if (cJSON_IsObject(item))
    {
        if (json_get_u32(item, "func_id", &v))
            funnel_default_cfg->func_id = (rt_uint8_t)v;
        host_std_cfg->funnel_default_cfg = funnel_default_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "upload_store");
    if (cJSON_IsObject(item))
    {
        host_parse_store(item, upload_store_cfg);
        rt_strncpy(upload_store_cfg->dir, host_std_cfg->protocol_ver,
                   sizeof(upload_store_cfg->dir) - 1);
        upload_store_cfg->dir[sizeof(upload_store_cfg->dir) - 1] = '\0';
        store_get_event_last_num(upload_store_cfg->dir, upload_store_cfg->path,
                                 &funnel_default_cfg->event_last_num);
        host_std_cfg->upload_store_cfg = upload_store_cfg;
        if (host_std_cfg->funnel_default_cfg == RT_NULL)
            host_std_cfg->funnel_default_cfg = funnel_default_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "upload_net");
    if (cJSON_IsObject(item) && json_get_u32(item, "number", &v))
        upload_net_cfg->number = (rt_uint8_t)v;
    else
        upload_net_cfg->number = host_std_cfg->host_id;
    host_std_cfg->upload_net_cfg = upload_net_cfg;

    return RT_EOK;
}

static rt_err_t host_std_set_param(host_std_t host_std, host_std_config_t cfg)
{
    rt_err_t ret = RT_EOK;

    if (host_std == RT_NULL || cfg == RT_NULL)
        return -RT_EINVAL;

    if (host_obj_set_param(host_std->interface_crt, cfg->interface_crt_cfg, "interface_crt") != RT_EOK)
        ret = -RT_ERROR;
    if (host_obj_set_param(host_std->interface_bus, cfg->interface_bus_cfg, "interface_bus") != RT_EOK)
        ret = -RT_ERROR;
    if (host_obj_set_param(host_std->protocol_crt, cfg->protocol_crt_cfg, "protocol_crt") != RT_EOK)
        ret = -RT_ERROR;
    if (host_obj_set_param(host_std->protocol_bus, cfg->protocol_bus_cfg, "protocol_bus") != RT_EOK)
        ret = -RT_ERROR;
    if (host_obj_set_param(host_std->funnel_default, cfg->funnel_default_cfg, "funnel_default") != RT_EOK)
        ret = -RT_ERROR;
    if (host_obj_set_param(host_std->upload_store, cfg->upload_store_cfg, "upload_store") != RT_EOK)
        ret = -RT_ERROR;
    if (host_obj_set_param(host_std->upload_net, cfg->upload_net_cfg, "upload_net") != RT_EOK)
        ret = -RT_ERROR;

    return ret;
}

rt_err_t host_std_create_from_json(cJSON *host_json)
{
    struct interface_rs232_config interface_bus_cfg = {0};
    struct interface_rs232_config interface_crt_cfg = {0};
    struct protocol_ev1_config protocol_bus_cfg = {0};
    struct protocol_ev1_config protocol_crt_cfg = {0};
    struct upload_store_config upload_store_cfg = {0};
    struct funnel_default_config funnel_default_cfg =
    {
        .func_id = 0,
        .event_last_num = EVENT_LAST_NUM_DEFAULT
    };
    struct upload_net_config upload_net_cfg = {0};
    struct host_std_config host_std_cfg = {0};
    rt_err_t err;

    err = host_std_load_json(host_json, &host_std_cfg,
                             &interface_bus_cfg, &interface_crt_cfg,
                             &protocol_bus_cfg, &protocol_crt_cfg,
                             &upload_store_cfg, &funnel_default_cfg,
                             &upload_net_cfg);
    if (err != RT_EOK)
        return err;

    return host_std_create(&host_std_cfg);
}

rt_err_t host_std_set_param_from_json(cJSON *host_json)
{
    struct interface_rs232_config interface_bus_cfg = {0};
    struct interface_rs232_config interface_crt_cfg = {0};
    struct protocol_ev1_config protocol_bus_cfg = {0};
    struct protocol_ev1_config protocol_crt_cfg = {0};
    struct upload_store_config upload_store_cfg = {0};
    struct funnel_default_config funnel_default_cfg =
    {
        .func_id = 0,
        .event_last_num = EVENT_LAST_NUM_DEFAULT
    };
    struct upload_net_config upload_net_cfg = {0};
    struct host_std_config host_std_cfg = {0};
    host_t host;
    rt_err_t err;

    err = host_std_load_json(host_json, &host_std_cfg,
                             &interface_bus_cfg, &interface_crt_cfg,
                             &protocol_bus_cfg, &protocol_crt_cfg,
                             &upload_store_cfg, &funnel_default_cfg,
                             &upload_net_cfg);
    if (err != RT_EOK)
        return err;

    host = g_host[host_std_cfg.host_id];
    if (!json_is_enabled(host_json, "switch", RT_FALSE))
    {
        if (host == RT_NULL)
            return RT_EOK;
        if (host->type != HOST_TYPE_STD)
        {
            LOG_E("host %d is not std", host_std_cfg.host_id);
            return -RT_EINVAL;
        }
        LOG_I("host_std %d switch off, delete", host->id);
        return host_std_delete(host);
    }

    if (host == RT_NULL)
        return host_std_create(&host_std_cfg);

    if (host->type != HOST_TYPE_STD)
    {
        LOG_E("host %d is not std", host_std_cfg.host_id);
        return -RT_EINVAL;
    }

    return host_std_set_param((host_std_t)host, &host_std_cfg);
}
