#include "host_std.h"
#include "store.h"
#include "json_util.h"

#include <string.h>

#define DBG_TAG "host.std"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static rt_bool_t host_std_muart_rx_bind_status(host_t self)
{
    if (self == RT_NULL)
        return RT_FALSE;

    host_std_t host_std = (host_std_t)self;

    if (host_std->default_funnel == RT_NULL || host_std->muart_upload == RT_NULL)
        return RT_FALSE;

    return get_bind_status((custom_obj_t)host_std->default_funnel,
                           (custom_obj_t)host_std->muart_upload,
                           RT_TRUE);
}

static rt_err_t host_std_muart_rx_bind_delete(host_t self)
{
    if(self == RT_NULL)
        return -RT_EINVAL;

    host_t host = self;
    host_std_t host_std = (host_std_t)host;

    bind_delete((custom_obj_t)host_std->default_funnel, (custom_obj_t)host_std->muart_upload, RT_TRUE);

    return RT_EOK;
}

static rt_err_t host_std_muart_rx_bind_create(host_t self)
{
    if(self == RT_NULL)
        return -RT_EINVAL;

    host_t host = self;
    host_std_t host_std = (host_std_t)host;

    bind_create((custom_obj_t)host_std->default_funnel, (custom_obj_t)host_std->muart_upload, RT_TRUE);

    return RT_EOK;
}
static rt_size_t host_std_get_history(host_t self, rt_uint8_t *buffer, rt_size_t size, event_history_t event_history, rt_bool_t *is_end)
{
    if (self == RT_NULL)
        return 0;

    return host_upload_get_history(((host_std_t)self)->store_upload, buffer, size, event_history, is_end);
}

static rt_err_t host_std_delete(host_t self)
{
    if (self == RT_NULL || self->id >= HOST_MAX)
        return -RT_EINVAL;

    host_t host = self;
    host_std_t host_std = (host_std_t)host;

    g_host[host->id] = RT_NULL;

    host_delete_funnel(&host_std->default_funnel);
    host_delete_upload(&host_std->store_upload);
    host_delete_upload(&host_std->muart_upload);
    host_delete_protocol(&host_std->crt_protocol);
    host_delete_protocol(&host_std->bus_protocol);
    host_delete_interface(&host_std->crt_interface);
    host_delete_interface(&host_std->bus_interface);

    rt_free(host_std);
    return RT_EOK;
}

static const struct host_ops host_std_ops =
{
    .muart_rx_bind_status = host_std_muart_rx_bind_status,
    .muart_rx_bind_delete = host_std_muart_rx_bind_delete,
    .muart_rx_bind_create = host_std_muart_rx_bind_create,
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

    if (config->crt_interface_cfg != RT_NULL)
        host_std->crt_interface = host_create_interface(g_com[config->crt_com_id], config->host_id,
                                                        config->crt_com_id, config->crt_interface_cfg);
    if (config->bus_interface_cfg != RT_NULL)
        host_std->bus_interface = host_create_interface(g_com[config->bus_com_id], config->host_id,
                                                        config->bus_com_id, config->bus_interface_cfg);
    if (config->crt_protocol_cfg != RT_NULL && host_std->crt_interface != RT_NULL)
        host_std->crt_protocol = host_create_protocol(protocol_parent_name, config->host_id,
                                                      config->crt_com_id, config->crt_protocol_cfg);
    if (config->bus_protocol_cfg != RT_NULL && host_std->bus_interface != RT_NULL)
        host_std->bus_protocol = host_create_protocol(protocol_parent_name, config->host_id,
                                                      config->bus_com_id, config->bus_protocol_cfg);
    if (config->default_funnel_cfg != RT_NULL)
        host_std->default_funnel = host_create_funnel(FUNNEL_DEFAULT_PARENT_NAME, config->host_id,
                                                      'x' - 0x30, config->default_funnel_cfg);

    if (config->store_upload_cfg != RT_NULL)
    {
        upload_store_config_t store_cfg = (upload_store_config_t)config->store_upload_cfg;

        rt_strncpy(store_cfg->dir, config->protocol_ver, sizeof(store_cfg->dir) - 1);
        store_cfg->dir[sizeof(store_cfg->dir) - 1] = '\0';
        host_std->store_upload = host_create_upload(UPLOAD_STORE_PARENT_NAME, config->host_id,
                                                    'x' - 0x30, config->store_upload_cfg);
    }

    if (config->muart_upload_cfg != RT_NULL)
        host_std->muart_upload = host_create_upload(UPLOAD_MUART_PARENT_NAME, config->host_id,
                                                    'x' - 0x30, config->muart_upload_cfg);


    bind_create((custom_obj_t)host_std->crt_interface, (custom_obj_t)host_std->crt_protocol, RT_TRUE);
    bind_create((custom_obj_t)host_std->crt_protocol, (custom_obj_t)host_std->default_funnel, RT_TRUE);

    bind_create((custom_obj_t)host_std->bus_interface, (custom_obj_t)host_std->bus_protocol, RT_TRUE);
    bind_create((custom_obj_t)host_std->bus_protocol, (custom_obj_t)host_std->default_funnel, RT_TRUE);

    bind_create((custom_obj_t)host_std->default_funnel, (custom_obj_t)host_std->store_upload, RT_TRUE);
    bind_create((custom_obj_t)host_std->default_funnel, (custom_obj_t)host_std->muart_upload , RT_TRUE);

    bind_create((custom_obj_t)host_std->bus_interface, (custom_obj_t)host_std->bus_protocol, RT_FALSE);
    bind_create((custom_obj_t)host_std->bus_protocol, (custom_obj_t)host_std->default_funnel, RT_FALSE);
    bind_create((custom_obj_t)host_std->default_funnel, (custom_obj_t)host_std->muart_upload , RT_FALSE);

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
                                   struct interface_rs232_config *bus_interface_cfg,
                                   struct interface_rs232_config *crt_interface_cfg,
                                   struct protocol_ev1_config *bus_protocol_cfg,
                                   struct protocol_ev1_config *crt_protocol_cfg,
                                   struct upload_store_config *store_upload_cfg,
                                   struct funnel_default_config *default_funnel_cfg,
                                   struct upload_muart_config *muart_upload_cfg)
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

    item = cJSON_GetObjectItemCaseSensitive(host_json, "crt_interface");
    if (cJSON_IsObject(item))
    {
        if (json_get_u32(item, "id", &v))
            host_std_cfg->crt_com_id = (rt_uint8_t)v;
        host_parse_rs232(item, crt_interface_cfg);
        host_std_cfg->crt_interface_cfg = crt_interface_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "crt_protocol");
    if (cJSON_IsObject(item))
    {
        host_parse_protocol_ev1(item, crt_protocol_cfg);
        host_std_cfg->crt_protocol_cfg = crt_protocol_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "bus_interface");
    if (cJSON_IsObject(item))
    {
        if (json_get_u32(item, "id", &v))
            host_std_cfg->bus_com_id = (rt_uint8_t)v;
        host_parse_rs232(item, bus_interface_cfg);
        host_std_cfg->bus_interface_cfg = bus_interface_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "bus_protocol");
    if (cJSON_IsObject(item))
    {
        host_parse_protocol_ev1(item, bus_protocol_cfg);
        host_std_cfg->bus_protocol_cfg = bus_protocol_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "funnel");
    if (cJSON_IsObject(item))
    {
        if (json_get_u32(item, "func_id", &v))
            default_funnel_cfg->func_id = (rt_uint8_t)v;
        host_std_cfg->default_funnel_cfg = default_funnel_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "store");
    if (cJSON_IsObject(item))
    {
        host_parse_store(item, store_upload_cfg);
        rt_strncpy(store_upload_cfg->dir, host_std_cfg->protocol_ver,
                   sizeof(store_upload_cfg->dir) - 1);
        store_upload_cfg->dir[sizeof(store_upload_cfg->dir) - 1] = '\0';
        store_get_event_last_num(store_upload_cfg->dir, store_upload_cfg->path,
                                 &default_funnel_cfg->event_last_num);
        host_std_cfg->store_upload_cfg = store_upload_cfg;
        if (host_std_cfg->default_funnel_cfg == RT_NULL)
            host_std_cfg->default_funnel_cfg = default_funnel_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "muart");
    if (cJSON_IsObject(item) && json_get_u32(item, "number", &v))
        muart_upload_cfg->number = (rt_uint8_t)v;
    else
        muart_upload_cfg->number = host_std_cfg->host_id;
    host_std_cfg->muart_upload_cfg = muart_upload_cfg;

    return RT_EOK;
}

static rt_err_t host_std_set_param(host_std_t host_std, host_std_config_t cfg)
{
    rt_err_t ret = RT_EOK;

    if (host_std == RT_NULL || cfg == RT_NULL)
        return -RT_EINVAL;

    if (host_set_interface_param(host_std->crt_interface, cfg->crt_interface_cfg, "crt_interface") != RT_EOK)
        ret = -RT_ERROR;
    if (host_set_interface_param(host_std->bus_interface, cfg->bus_interface_cfg, "bus_interface") != RT_EOK)
        ret = -RT_ERROR;
    if (host_set_protocol_param(host_std->crt_protocol, cfg->crt_protocol_cfg, "crt_protocol") != RT_EOK)
        ret = -RT_ERROR;
    if (host_set_protocol_param(host_std->bus_protocol, cfg->bus_protocol_cfg, "bus_protocol") != RT_EOK)
        ret = -RT_ERROR;
    if (host_set_funnel_param(host_std->default_funnel, cfg->default_funnel_cfg, "funnel") != RT_EOK)
        ret = -RT_ERROR;
    if (host_set_upload_param(host_std->store_upload, cfg->store_upload_cfg, "store") != RT_EOK)
        ret = -RT_ERROR;
    if (host_set_upload_param(host_std->muart_upload, cfg->muart_upload_cfg, "muart") != RT_EOK)
        ret = -RT_ERROR;

    return ret;
}

rt_err_t host_std_create_from_json(cJSON *host_json)
{
    struct interface_rs232_config bus_interface_cfg = {0};
    struct interface_rs232_config crt_interface_cfg = {0};
    struct protocol_ev1_config bus_protocol_cfg = {0};
    struct protocol_ev1_config crt_protocol_cfg = {0};
    struct upload_store_config store_upload_cfg = {0};
    struct funnel_default_config default_funnel_cfg =
    {
        .func_id = 0,
        .event_last_num = EVENT_LAST_NUM_DEFAULT
    };
    struct upload_muart_config muart_upload_cfg = {0};
    struct host_std_config host_std_cfg = {0};
    rt_err_t err;

    err = host_std_load_json(host_json, &host_std_cfg,
                             &bus_interface_cfg, &crt_interface_cfg,
                             &bus_protocol_cfg, &crt_protocol_cfg,
                             &store_upload_cfg, &default_funnel_cfg,
                             &muart_upload_cfg);
    if (err != RT_EOK)
        return err;

    return host_std_create(&host_std_cfg);
}

rt_err_t host_std_set_param_from_json(cJSON *host_json)
{
    struct interface_rs232_config bus_interface_cfg = {0};
    struct interface_rs232_config crt_interface_cfg = {0};
    struct protocol_ev1_config bus_protocol_cfg = {0};
    struct protocol_ev1_config crt_protocol_cfg = {0};
    struct upload_store_config store_upload_cfg = {0};
    struct funnel_default_config default_funnel_cfg =
    {
        .func_id = 0,
        .event_last_num = EVENT_LAST_NUM_DEFAULT
    };
    struct upload_muart_config muart_upload_cfg = {0};
    struct host_std_config host_std_cfg = {0};
    host_t host;
    rt_err_t err;

    err = host_std_load_json(host_json, &host_std_cfg,
                             &bus_interface_cfg, &crt_interface_cfg,
                             &bus_protocol_cfg, &crt_protocol_cfg,
                             &store_upload_cfg, &default_funnel_cfg,
                             &muart_upload_cfg);
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
