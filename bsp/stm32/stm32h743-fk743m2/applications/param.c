#include "param.h"
#include "host_std.h"
#include "store.h"

#include <cJSON.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#define DBG_TAG "param"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define PARAM_JSON_MAX      16384

static const char param_default_json[] =
"{\n"
"  \"hosts\": [\n"
"    {\n"
"      \"type\": \"std\",\n"
"      \"host_id\": 0,\n"
"      \"crt_com_id\": 1,\n"
"      \"bus_com_id\": 2,\n"
"      \"bus_interface\": {\n"
"        \"uart_name\": \"uart8\",\n"
"        \"baud_rate\": 115200,\n"
"        \"data_bits\": 8,\n"
"        \"stop_bits\": 1,\n"
"        \"parity\": 0,\n"
"        \"bit_order\": 0,\n"
"        \"rx_bufsz\": 10240,\n"
"        \"dma_ping_bufsz\": 1024,\n"
"        \"tx_bufsz\": 1024,\n"
"        \"rx_en_pin\": 80\n"
"      },\n"
"      \"protocol_ver\": \"ev1\",\n"
"      \"bus_protocol\": {\n"
"        \"lib_name\": \"lib010101_tx3016a_crt.so\"\n"
"      },\n"
"      \"funnel\": {\n"
"        \"func_id\": 0\n"
"      },\n"
"      \"store\": {\n"
"        \"path\": \"tx3016a\"\n"
"      },\n"
"      \"muart\": {\n"
"        \"number\": 0\n"
"      }\n"
"    }\n"
"  ]\n"
"}\n";

static rt_bool_t json_get_u32(cJSON *obj, const char *key, rt_uint32_t *out)
{
    cJSON *item;

    if (obj == RT_NULL || key == RT_NULL || out == RT_NULL)
        return RT_FALSE;

    item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(item))
        return RT_FALSE;

    *out = (rt_uint32_t)item->valuedouble;
    return RT_TRUE;
}

static void json_copy_str(char *dst, rt_size_t dst_sz, cJSON *obj, const char *key)
{
    cJSON *item;

    if (dst == RT_NULL || dst_sz == 0 || obj == RT_NULL || key == RT_NULL)
        return;

    item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsString(item) || item->valuestring == RT_NULL)
        return;

    rt_strncpy(dst, item->valuestring, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
}

static void param_parse_rs232(cJSON *obj, struct interface_rs232_config *cfg)
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

static void param_parse_protocol_ev1(cJSON *obj, struct protocol_ev1_config *cfg)
{
    if (obj == RT_NULL || cfg == RT_NULL)
        return;

    json_copy_str(cfg->lib_name, sizeof(cfg->lib_name), obj, "lib_name");
}

static void param_parse_store(cJSON *obj, struct upload_store_config *cfg)
{
    if (obj == RT_NULL || cfg == RT_NULL)
        return;

    json_copy_str(cfg->path, sizeof(cfg->path), obj, "path");
}

static enum host_type param_parse_host_type(cJSON *host_json)
{
    cJSON *item;

    if (host_json == RT_NULL)
        return HOST_TYPE_NONE;

    item = cJSON_GetObjectItemCaseSensitive(host_json, "type");
    if (cJSON_IsString(item) && item->valuestring != RT_NULL)
    {
        if (rt_strcmp(item->valuestring, "std") == 0)
            return HOST_TYPE_STD;
        return HOST_TYPE_NONE;
    }

    if (cJSON_IsNumber(item))
    {
        int t = (int)item->valuedouble;
        if (t > HOST_TYPE_NONE && t < HOST_TYPE_MAX)
            return (enum host_type)t;
    }

    return HOST_TYPE_NONE;
}

static rt_err_t param_create_host_std(cJSON *host_json)
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

    if (!json_get_u32(host_json, "host_id", &v))
    {
        LOG_E("std host missing host_id");
        return -RT_EINVAL;
    }

    host_std_cfg.host_type = HOST_TYPE_STD;
    host_std_cfg.host_id = (rt_uint8_t)v;
    json_copy_str(host_std_cfg.protocol_ver, sizeof(host_std_cfg.protocol_ver), host_json, "protocol_ver");
    if (host_std_cfg.protocol_ver[0] == '\0')
        rt_strncpy(host_std_cfg.protocol_ver, "ev1", sizeof(host_std_cfg.protocol_ver) - 1);

    if (json_get_u32(host_json, "crt_com_id", &v))
        host_std_cfg.crt_com_id = (rt_uint8_t)v;
    if (json_get_u32(host_json, "bus_com_id", &v))
        host_std_cfg.bus_com_id = (rt_uint8_t)v;

    item = cJSON_GetObjectItemCaseSensitive(host_json, "crt_interface");
    if (cJSON_IsObject(item))
    {
        param_parse_rs232(item, &crt_interface_cfg);
        host_std_cfg.crt_interface_cfg = &crt_interface_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "crt_protocol");
    if (cJSON_IsObject(item))
    {
        param_parse_protocol_ev1(item, &crt_protocol_cfg);
        host_std_cfg.crt_protocol_cfg = &crt_protocol_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "bus_interface");
    if (cJSON_IsObject(item))
    {
        param_parse_rs232(item, &bus_interface_cfg);
        host_std_cfg.bus_interface_cfg = &bus_interface_cfg;
    }

    item = cJSON_GetObjectItemCaseSensitive(host_json, "bus_protocol");
    if (cJSON_IsObject(item))
    {
        param_parse_protocol_ev1(item, &bus_protocol_cfg);
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
        param_parse_store(item, &store_upload_cfg);
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

static rt_err_t param_create_hosts(cJSON *root)
{
    cJSON *hosts;
    cJSON *host_json;
    int i, count;
    rt_err_t ret = RT_EOK;

    hosts = cJSON_GetObjectItemCaseSensitive(root, "hosts");
    if (!cJSON_IsArray(hosts))
    {
        LOG_E("param.json missing hosts array");
        return -RT_EINVAL;
    }

    count = cJSON_GetArraySize(hosts);
    if (count <= 0)
    {
        LOG_W("param.json hosts is empty");
        return RT_EOK;
    }

    for (i = 0; i < count; i++)
    {
        enum host_type type;
        rt_err_t err = RT_EOK;

        host_json = cJSON_GetArrayItem(hosts, i);
        if (!cJSON_IsObject(host_json))
        {
            LOG_W("hosts[%d] is not object, skip", i);
            continue;
        }

        type = param_parse_host_type(host_json);
        switch (type)
        {
        case HOST_TYPE_STD:
            err = param_create_host_std(host_json);
            break;
        default:
            LOG_W("hosts[%d] unsupported type, skip", i);
            continue;
        }

        if (err != RT_EOK)
        {
            LOG_E("hosts[%d] create failed: %d", i, err);
            ret = err;
        }
    }

    return ret;
}

static rt_err_t param_read_file(char **out)
{
    struct stat st;
    int fd;
    int n;
    char *buf;

    *out = RT_NULL;

    if (stat(PARAM_JSON_PATH, &st) != 0)
        return -RT_ERROR;

    if (st.st_size <= 0 || st.st_size > PARAM_JSON_MAX)
    {
        LOG_E("%s size invalid: %ld", PARAM_JSON_PATH, (long)st.st_size);
        return -RT_ERROR;
    }

    fd = open(PARAM_JSON_PATH, O_RDONLY, 0);
    if (fd < 0)
        return -RT_ERROR;

    buf = rt_malloc(st.st_size + 1);
    if (buf == RT_NULL)
    {
        close(fd);
        return -RT_ENOMEM;
    }

    n = read(fd, buf, st.st_size);
    close(fd);
    if (n <= 0)
    {
        rt_free(buf);
        return -RT_ERROR;
    }

    buf[n] = '\0';
    *out = buf;
    return RT_EOK;
}

static rt_err_t param_write_default(void)
{
    int fd;
    int n;
    rt_size_t len;

    ensure_dir("/init");

    fd = open(PARAM_JSON_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0)
    {
        LOG_E("create %s failed", PARAM_JSON_PATH);
        return -RT_ERROR;
    }

    len = rt_strlen(param_default_json);
    n = write(fd, param_default_json, len);
    close(fd);

    if (n != (int)len)
    {
        LOG_E("write %s failed", PARAM_JSON_PATH);
        return -RT_ERROR;
    }

    LOG_I("created default %s", PARAM_JSON_PATH);
    return RT_EOK;
}

rt_err_t param_apply(void)
{
    char *file_buf = RT_NULL;
    const char *json_text;
    cJSON *root;
    rt_err_t err;

    if (param_read_file(&file_buf) == RT_EOK)
    {
        json_text = file_buf;
        LOG_I("load %s", PARAM_JSON_PATH);
    }
    else
    {
        param_write_default();
        json_text = param_default_json;
        LOG_I("use default param");
    }

    root = cJSON_Parse(json_text);
    rt_free(file_buf);
    if (root == RT_NULL)
    {
        LOG_E("parse param.json failed");
        return -RT_ERROR;
    }

    err = param_create_hosts(root);
    cJSON_Delete(root);
    return err;
}
