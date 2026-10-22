#include "host.h"
#include "json_util.h"
#include "class/interface_rs232.h"
#include "class/protocol_ev1.h"
#include "class/upload_store.h"

#include <string.h>

#define DBG_TAG "host"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

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

/* 四类对象都是 custom_obj 后面紧跟 ops，前三个操作签名一致。 */
struct host_obj
{
    struct custom_obj parent;
    const struct host_obj_ops *ops;
};

struct host_obj_ops
{
    void *(*create)(void *self, char *name, void *config);
    rt_err_t (*delete)(void *self);
    double_ret (*set_param)(void *self, void *config);
};

void *host_obj_create(const char *parent_name, rt_uint8_t host_id, rt_uint8_t sub_id, void *cfg)
{
    struct host_obj *parent;
    char name[RT_NAME_MAX];

    if (parent_name == RT_NULL || cfg == RT_NULL)
        return RT_NULL;

    parent = (struct host_obj *)rt_object_find(parent_name, RT_Object_Class_Custom);
    if (parent == RT_NULL || parent->ops == RT_NULL || parent->ops->create == RT_NULL)
        return RT_NULL;
    if (generate_name_from_parent(name, sizeof(name), parent_name, host_id, sub_id) != RT_EOK)
        return RT_NULL;

    return parent->ops->create(parent, name, cfg);
}

void host_obj_delete(void *obj)
{
    void **slot = obj;
    struct host_obj *self;

    if (slot == RT_NULL || *slot == RT_NULL)
        return;

    self = *slot;
    if (self->ops != RT_NULL && self->ops->delete != RT_NULL)
        self->ops->delete(self);
    *slot = RT_NULL;
}

rt_err_t host_obj_set_param(void *obj, void *cfg, const char *name)
{
    struct host_obj *self = obj;
    double_ret ret;

    if (cfg == RT_NULL)
        return RT_EOK;
    if (self == RT_NULL || self->ops == RT_NULL || self->ops->set_param == RT_NULL)
    {
        LOG_W("%s set_param skipped", name);
        return RT_EOK;
    }

    ret = self->ops->set_param(self, cfg);
    if (ret.fail != 0)
    {
        LOG_E("%s set_param fail:%d succ:%d", name, ret.fail, ret.succ);
        return -RT_ERROR;
    }

    LOG_I("%s set_param succ:%d", name, ret.succ);
    return RT_EOK;
}

rt_size_t host_upload_get_history(upload_obj_t obj, rt_uint8_t *buffer, rt_size_t size,
                                  event_history_t event_history, rt_bool_t *is_end)
{
    rt_size_t n;

    if (buffer == RT_NULL || size == 0)
        return 0;
    if (obj == RT_NULL || event_history == RT_NULL
        || obj->ops == RT_NULL || obj->ops->get_history == RT_NULL)
        return event_put_end(buffer, size, is_end);

    n = obj->ops->get_history(obj, buffer, size, event_history, is_end);
    if (n == 0)
        return event_put_end(buffer, size, is_end);
    return n;
}

void host_parse_rs232(cJSON *obj, struct interface_rs232_config *cfg)
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

void host_parse_protocol_ev1(cJSON *obj, struct protocol_ev1_config *cfg)
{
    if (obj == RT_NULL || cfg == RT_NULL)
        return;

    json_copy_str(cfg->lib_name, sizeof(cfg->lib_name), obj, "lib_name");
}

void host_parse_store(cJSON *obj, struct upload_store_config *cfg)
{
    if (obj == RT_NULL || cfg == RT_NULL)
        return;

    json_copy_str(cfg->path, sizeof(cfg->path), obj, "path");
}