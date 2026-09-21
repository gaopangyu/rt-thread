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
    if (self == RT_NULL || buffer == RT_NULL || size == 0 || event_history == RT_NULL)
        return 0;

    host_std_t host_std = (host_std_t)self;

    if (host_std->store_upload == RT_NULL || host_std->store_upload->ops == RT_NULL ||
        host_std->store_upload->ops->get_history == RT_NULL)
        return 0;

    return host_std->store_upload->ops->get_history(host_std->store_upload, buffer, size, event_history, is_end);
}

static rt_err_t host_std_delete(host_t self)
{
    if (self == RT_NULL || self->id >= HOST_MAX)
        return -RT_EINVAL;

    host_t host = self;
    host_std_t host_std = (host_std_t)host;

    g_host[host->id] = RT_NULL;

    if (host_std->default_funnel != RT_NULL && host_std->default_funnel->ops != RT_NULL &&
        host_std->default_funnel->ops->delete != RT_NULL)
    {
        host_std->default_funnel->ops->delete(host_std->default_funnel);
        host_std->default_funnel = RT_NULL;
    }

    if (host_std->store_upload != RT_NULL && host_std->store_upload->ops != RT_NULL &&
        host_std->store_upload->ops->delete != RT_NULL)
    {
        host_std->store_upload->ops->delete(host_std->store_upload);
        host_std->store_upload = RT_NULL;
    }

    if (host_std->muart_upload != RT_NULL && host_std->muart_upload->ops != RT_NULL &&
        host_std->muart_upload->ops->delete != RT_NULL)
    {
        host_std->muart_upload->ops->delete(host_std->muart_upload);
        host_std->muart_upload = RT_NULL;
    }


    if (host_std->crt_protocol != RT_NULL && host_std->crt_protocol->ops != RT_NULL &&
        host_std->crt_protocol->ops->delete != RT_NULL)
    {
        host_std->crt_protocol->ops->delete(host_std->crt_protocol);
        host_std->crt_protocol = RT_NULL;
    }

    if (host_std->bus_protocol != RT_NULL && host_std->bus_protocol->ops != RT_NULL &&
        host_std->bus_protocol->ops->delete != RT_NULL)
    {
        host_std->bus_protocol->ops->delete(host_std->bus_protocol);
        host_std->bus_protocol = RT_NULL;
    }

    if (host_std->crt_interface != RT_NULL && host_std->crt_interface->ops != RT_NULL &&
        host_std->crt_interface->ops->delete != RT_NULL)
    {
        host_std->crt_interface->ops->delete(host_std->crt_interface);
        host_std->crt_interface = RT_NULL;
    }

    if (host_std->bus_interface != RT_NULL && host_std->bus_interface->ops != RT_NULL &&
        host_std->bus_interface->ops->delete != RT_NULL)
    {
        host_std->bus_interface->ops->delete(host_std->bus_interface);
        host_std->bus_interface = RT_NULL;
    }

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
    interface_obj_t crt_interface_parent = (interface_obj_t)rt_object_find(g_com[config->crt_com_id], RT_Object_Class_Custom);
    interface_obj_t bus_interface_parent = (interface_obj_t)rt_object_find(g_com[config->bus_com_id], RT_Object_Class_Custom);
    protocol_obj_t protocol_parent = RT_NULL;
    funnel_obj_t funnel_parent = (funnel_obj_t)rt_object_find(FUNNEL_DEFAULT_PARENT_NAME, RT_Object_Class_Custom);
    upload_obj_t store_parent = (upload_obj_t)rt_object_find(UPLOAD_STORE_PARENT_NAME, RT_Object_Class_Custom);
    upload_obj_t muart_parent = (upload_obj_t)rt_object_find(UPLOAD_MUART_PARENT_NAME, RT_Object_Class_Custom);

    char name[RT_NAME_MAX];

    if (protocol_parent_name != RT_NULL)
        protocol_parent = (protocol_obj_t)rt_object_find(protocol_parent_name, RT_Object_Class_Custom);
    else
        LOG_W("host_std %d unsupported protocol_ver: %s", config->host_id, config->protocol_ver);

    if (config->crt_interface_cfg != RT_NULL && crt_interface_parent != RT_NULL
     && crt_interface_parent->ops != RT_NULL && crt_interface_parent->ops->create != RT_NULL)
    {
        if (generate_name_from_parent(name, sizeof(name), g_com[config->crt_com_id], config->host_id, config->crt_com_id) == RT_EOK)
        {
            host_std->crt_interface = crt_interface_parent->ops->create(crt_interface_parent, name, config->crt_interface_cfg);
        }
    }

    if (config->bus_interface_cfg != RT_NULL && bus_interface_parent != RT_NULL
     && bus_interface_parent->ops != RT_NULL && bus_interface_parent->ops->create != RT_NULL)
    {
        if (generate_name_from_parent(name, sizeof(name), g_com[config->bus_com_id], config->host_id, config->bus_com_id) == RT_EOK)
        {
            host_std->bus_interface = bus_interface_parent->ops->create(bus_interface_parent, name, config->bus_interface_cfg);
        }
    }

    if (config->crt_protocol_cfg != RT_NULL && protocol_parent != RT_NULL
     && protocol_parent->ops != RT_NULL && protocol_parent->ops->create != RT_NULL)
    {
        if (generate_name_from_parent(name, sizeof(name), protocol_parent_name, config->host_id, config->crt_com_id) == RT_EOK)
        {
            host_std->crt_protocol = protocol_parent->ops->create(protocol_parent, name, config->crt_protocol_cfg);
        }
    }

    if (config->bus_protocol_cfg != RT_NULL && protocol_parent != RT_NULL
     && protocol_parent->ops != RT_NULL && protocol_parent->ops->create != RT_NULL)
    {
        if (generate_name_from_parent(name, sizeof(name), protocol_parent_name, config->host_id, config->bus_com_id) == RT_EOK)
        {
            host_std->bus_protocol = protocol_parent->ops->create(protocol_parent, name, config->bus_protocol_cfg);
        }
    }

    if (config->default_funnel_cfg != RT_NULL && funnel_parent != RT_NULL
     && funnel_parent->ops != RT_NULL && funnel_parent->ops->create != RT_NULL)
    {
        if (generate_name_from_parent(name, sizeof(name), FUNNEL_DEFAULT_PARENT_NAME, config->host_id, 'x' - 0x30) == RT_EOK)
        {
            host_std->default_funnel = funnel_parent->ops->create(funnel_parent, name, config->default_funnel_cfg);
        }
    }

    if (config->store_upload_cfg != RT_NULL && store_parent != RT_NULL
     && store_parent->ops != RT_NULL && store_parent->ops->create != RT_NULL)
    {
        upload_store_config_t store_cfg = (upload_store_config_t)config->store_upload_cfg;

        rt_strncpy(store_cfg->dir, config->protocol_ver, sizeof(store_cfg->dir) - 1);
        store_cfg->dir[sizeof(store_cfg->dir) - 1] = '\0';

        if (generate_name_from_parent(name, sizeof(name), UPLOAD_STORE_PARENT_NAME, config->host_id, 'x' - 0x30) == RT_EOK)
        {
            host_std->store_upload = store_parent->ops->create(store_parent, name, config->store_upload_cfg);
        }
    }


    if (config->muart_upload_cfg != RT_NULL && muart_parent != RT_NULL
        && muart_parent->ops != RT_NULL && muart_parent->ops->create != RT_NULL)
    {
        if (generate_name_from_parent(name, sizeof(name), UPLOAD_MUART_PARENT_NAME, config->host_id, 'x' - 0x30) == RT_EOK)
        {
            host_std->muart_upload = muart_parent->ops->create(muart_parent, name, config->muart_upload_cfg);
        }
    }


    bind_create((custom_obj_t)host_std->crt_interface, (custom_obj_t)host_std->crt_protocol, RT_TRUE);
    bind_create((custom_obj_t)host_std->crt_protocol, (custom_obj_t)host_std->default_funnel, RT_TRUE);

    bind_create((custom_obj_t)host_std->bus_interface, (custom_obj_t)host_std->bus_protocol, RT_TRUE);
    bind_create((custom_obj_t)host_std->bus_protocol, (custom_obj_t)host_std->default_funnel, RT_TRUE);

    bind_create((custom_obj_t)host_std->default_funnel, (custom_obj_t)host_std->store_upload, RT_TRUE);


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

static void host_std_parse_rs232(cJSON *obj, struct interface_rs232_config *cfg)
{
    rt_uint32_t v;

    if (obj == RT_NULL || cfg == RT_NULL)
        return;

    json_copy_str(cfg->uart_name, sizeof(cfg->uart_name), obj, "uart_name");

    if (json_get_u32(obj, "baud_rate", &v))
        cfg->baud_rate = v;
    if (json_get_u32(obj, "data_bits", &v))
        cfg->data_bits = (rt_uint8_t)v;
    if (json_get_u32(obj, "stop_bits", &v))
        cfg->stop_bits = (rt_uint8_t)v;
    if (json_get_u32(obj, "parity", &v))
        cfg->parity = (rt_uint8_t)v;
    if (json_get_u32(obj, "bit_order", &v))
        cfg->bit_order = (rt_uint8_t)v;
    if (json_get_u32(obj, "rx_bufsz", &v))
        cfg->rx_bufsz = (rt_size_t)v;
    if (json_get_u32(obj, "dma_ping_bufsz", &v))
        cfg->dma_ping_bufsz = (rt_size_t)v;
    if (json_get_u32(obj, "tx_bufsz", &v))
        cfg->tx_bufsz = (rt_size_t)v;
    if (json_get_u32(obj, "rx_en_pin", &v))
        cfg->rx_en_pin = (rt_base_t)v;
}

static void host_std_parse_protocol_ev1(cJSON *obj, struct protocol_ev1_config *cfg)
{
    if (obj == RT_NULL || cfg == RT_NULL)
        return;

    json_copy_str(cfg->lib_name, sizeof(cfg->lib_name), obj, "lib_name");
}

static void host_std_parse_store(cJSON *obj, struct upload_store_config *cfg)
{
    if (obj == RT_NULL || cfg == RT_NULL)
        return;

    json_copy_str(cfg->path, sizeof(cfg->path), obj, "path");
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
    rt_uint32_t v;
    cJSON *item;

    if (host_json == RT_NULL)
        return -RT_EINVAL;

    if (!json_get_u32(host_json, "id", &v) || v >= HOST_MAX)
    {
        LOG_E("std host id must < HOST_MAX(%d)", HOST_MAX);
        return -RT_EINVAL;
    }

    host_std_cfg.host_type = HOST_TYPE_STD;
    host_std_cfg.host_id = (rt_uint8_t)v;
    json_copy_str(host_std_cfg.protocol_ver, sizeof(host_std_cfg.protocol_ver), host_json, "protocol_ver");
    if (host_std_cfg.protocol_ver[0] == '\0')
        rt_strncpy(host_std_cfg.protocol_ver, "ev1", sizeof(host_std_cfg.protocol_ver) - 1);

    item = cJSON_GetObjectItemCaseSensitive(host_json, "crt_interface");
    if (cJSON_IsObject(item))
    {
        if (json_get_u32(item, "id", &v))
            host_std_cfg.crt_com_id = (rt_uint8_t)v;
        host_std_parse_rs232(item, &crt_interface_cfg);
        host_std_cfg.crt_interface_cfg = &crt_interface_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "crt_protocol");
    if (cJSON_IsObject(item))
    {
        host_std_parse_protocol_ev1(item, &crt_protocol_cfg);
        host_std_cfg.crt_protocol_cfg = &crt_protocol_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "bus_interface");
    if (cJSON_IsObject(item))
    {
        if (json_get_u32(item, "id", &v))
            host_std_cfg.bus_com_id = (rt_uint8_t)v;
        host_std_parse_rs232(item, &bus_interface_cfg);
        host_std_cfg.bus_interface_cfg = &bus_interface_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "bus_protocol");
    if (cJSON_IsObject(item))
    {
        host_std_parse_protocol_ev1(item, &bus_protocol_cfg);
        host_std_cfg.bus_protocol_cfg = &bus_protocol_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "funnel");
    if (cJSON_IsObject(item))
    {
        if (json_get_u32(item, "func_id", &v))
            default_funnel_cfg.func_id = (rt_uint8_t)v;
        host_std_cfg.default_funnel_cfg = &default_funnel_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "store");
    if (cJSON_IsObject(item))
    {
        host_std_parse_store(item, &store_upload_cfg);
        rt_strncpy(store_upload_cfg.dir, host_std_cfg.protocol_ver,
                   sizeof(store_upload_cfg.dir) - 1);
        store_upload_cfg.dir[sizeof(store_upload_cfg.dir) - 1] = '\0';
        store_get_event_last_num(store_upload_cfg.dir, store_upload_cfg.path,
                                 &default_funnel_cfg.event_last_num);
        host_std_cfg.store_upload_cfg = &store_upload_cfg;
        if (host_std_cfg.default_funnel_cfg == RT_NULL)
            host_std_cfg.default_funnel_cfg = &default_funnel_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "muart");
    if (cJSON_IsObject(item) && json_get_u32(item, "number", &v))
        muart_upload_cfg.number = (rt_uint8_t)v;
    else
        muart_upload_cfg.number = host_std_cfg.host_id;
    host_std_cfg.muart_upload_cfg = &muart_upload_cfg;

    return host_std_create(&host_std_cfg);
}
