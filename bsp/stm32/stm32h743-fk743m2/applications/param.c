#include "param.h"
#include "host_std.h"
#include "store.h"
#include "json_util.h"

#include <cJSON.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#define DBG_TAG "param"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define PARAM_JSON_MAX      16384

static const char param_mcu_json_default[] =
"{\r\n"
"    \"hosts\": [\r\n"
"        {\r\n"
"            \"switch\": false,\r\n"
"            \"id\": 0,\r\n"
"            \"type\": \"std\",\r\n"
"            \"protocol_ver\": \"ev1\",\r\n"
"            \"host_type\": \"xxx\",\r\n"
"            \"central_num\": 0,\r\n"
"            \"regional_num\": 0,\r\n"
"            \"local_num\": 0,\r\n"
"            \"keyboard_addr\": 0,\r\n"
"            \"key_addr\": 0,\r\n"
"            \"crt_interface\": {\r\n"
"                \"id\": 0,\r\n"
"                \"uart_name\": \"uartx\",\r\n"
"                \"baud_rate\": 115200,\r\n"
"                \"data_bits\": 8,\r\n"
"                \"stop_bits\": 1,\r\n"
"                \"parity\": 0,\r\n"
"                \"bit_order\": 0,\r\n"
"                \"rx_bufsz\": 10240,\r\n"
"                \"dma_ping_bufsz\": 1024,\r\n"
"                \"tx_bufsz\": 1024,\r\n"
"                \"rx_en_pin\": 80,\r\n"
"                \"term_switch\": false\r\n"
"            },\r\n"
"            \"crt_protocol\": {\r\n"
"                \"lib_name\": \"xxx_crt.so\"\r\n"
"            },\r\n"
"            \"bus_interface\": {\r\n"
"                \"id\": 0,\r\n"
"                \"uart_name\": \"uartx\",\r\n"
"                \"baud_rate\": 115200,\r\n"
"                \"data_bits\": 8,\r\n"
"                \"stop_bits\": 1,\r\n"
"                \"parity\": 0,\r\n"
"                \"bit_order\": 0,\r\n"
"                \"rx_bufsz\": 10240,\r\n"
"                \"dma_ping_bufsz\": 1024,\r\n"
"                \"tx_bufsz\": 1024,\r\n"
"                \"rx_en_pin\": 80,\r\n"
"                \"term_switch\": false\r\n"
"            },\r\n"
"            \"bus_protocol\": {\r\n"
"                \"lib_name\": \"xxx_bus.so\"\r\n"
"            },\r\n"
"            \"funnel\": {\r\n"
"                \"func_id\": 0\r\n"
"            },\r\n"
"            \"store\": {\r\n"
"                \"path\": \"xxx\"\r\n"
"            },\r\n"
"            \"muart\": {\r\n"
"                \"number\": 0\r\n"
"            }\r\n"
"        },\r\n"
"        {\r\n"
"            \"switch\": false,\r\n"
"            \"id\": 1,\r\n"
"            \"type\": \"std\",\r\n"
"            \"protocol_ver\": \"ev1\",\r\n"
"            \"host_type\": \"xxx\",\r\n"
"            \"central_num\": 0,\r\n"
"            \"regional_num\": 0,\r\n"
"            \"local_num\": 0,\r\n"
"            \"keyboard_addr\": 0,\r\n"
"            \"key_addr\": 0,\r\n"
"            \"crt_interface\": {\r\n"
"                \"id\": 0,\r\n"
"                \"uart_name\": \"uartx\",\r\n"
"                \"baud_rate\": 115200,\r\n"
"                \"data_bits\": 8,\r\n"
"                \"stop_bits\": 1,\r\n"
"                \"parity\": 0,\r\n"
"                \"bit_order\": 0,\r\n"
"                \"rx_bufsz\": 10240,\r\n"
"                \"dma_ping_bufsz\": 1024,\r\n"
"                \"tx_bufsz\": 1024,\r\n"
"                \"rx_en_pin\": 80,\r\n"
"                \"term_switch\": false\r\n"
"            },\r\n"
"            \"crt_protocol\": {\r\n"
"                \"lib_name\": \"xxx_crt.so\"\r\n"
"            },\r\n"
"            \"bus_interface\": {\r\n"
"                \"id\": 0,\r\n"
"                \"uart_name\": \"uartx\",\r\n"
"                \"baud_rate\": 115200,\r\n"
"                \"data_bits\": 8,\r\n"
"                \"stop_bits\": 1,\r\n"
"                \"parity\": 0,\r\n"
"                \"bit_order\": 0,\r\n"
"                \"rx_bufsz\": 10240,\r\n"
"                \"dma_ping_bufsz\": 1024,\r\n"
"                \"tx_bufsz\": 1024,\r\n"
"                \"rx_en_pin\": 80,\r\n"
"                \"term_switch\": false\r\n"
"            },\r\n"
"            \"bus_protocol\": {\r\n"
"                \"lib_name\": \"xxx_bus.so\"\r\n"
"            },\r\n"
"            \"funnel\": {\r\n"
"                \"func_id\": 0\r\n"
"            },\r\n"
"            \"store\": {\r\n"
"                \"path\": \"xxx\"\r\n"
"            },\r\n"
"            \"muart\": {\r\n"
"                \"number\": 0\r\n"
"            }\r\n"
"        },\r\n"
"        {\r\n"
"            \"switch\": false,\r\n"
"            \"id\": 2,\r\n"
"            \"type\": \"std\",\r\n"
"            \"protocol_ver\": \"ev1\",\r\n"
"            \"host_type\": \"xxx\",\r\n"
"            \"central_num\": 0,\r\n"
"            \"regional_num\": 0,\r\n"
"            \"local_num\": 0,\r\n"
"            \"keyboard_addr\": 0,\r\n"
"            \"key_addr\": 0,\r\n"
"            \"crt_interface\": {\r\n"
"                \"id\": 0,\r\n"
"                \"uart_name\": \"uartx\",\r\n"
"                \"baud_rate\": 115200,\r\n"
"                \"data_bits\": 8,\r\n"
"                \"stop_bits\": 1,\r\n"
"                \"parity\": 0,\r\n"
"                \"bit_order\": 0,\r\n"
"                \"rx_bufsz\": 10240,\r\n"
"                \"dma_ping_bufsz\": 1024,\r\n"
"                \"tx_bufsz\": 1024,\r\n"
"                \"rx_en_pin\": 80,\r\n"
"                \"term_switch\": false\r\n"
"            },\r\n"
"            \"crt_protocol\": {\r\n"
"                \"lib_name\": \"xxx_crt.so\"\r\n"
"            },\r\n"
"            \"bus_interface\": {\r\n"
"                \"id\": 0,\r\n"
"                \"uart_name\": \"uartx\",\r\n"
"                \"baud_rate\": 115200,\r\n"
"                \"data_bits\": 8,\r\n"
"                \"stop_bits\": 1,\r\n"
"                \"parity\": 0,\r\n"
"                \"bit_order\": 0,\r\n"
"                \"rx_bufsz\": 10240,\r\n"
"                \"dma_ping_bufsz\": 1024,\r\n"
"                \"tx_bufsz\": 1024,\r\n"
"                \"rx_en_pin\": 80,\r\n"
"                \"term_switch\": false\r\n"
"            },\r\n"
"            \"bus_protocol\": {\r\n"
"                \"lib_name\": \"xxx_bus.so\"\r\n"
"            },\r\n"
"            \"funnel\": {\r\n"
"                \"func_id\": 0\r\n"
"            },\r\n"
"            \"store\": {\r\n"
"                \"path\": \"xxx\"\r\n"
"            },\r\n"
"            \"muart\": {\r\n"
"                \"number\": 0\r\n"
"            }\r\n"
"        },\r\n"
"        {\r\n"
"            \"switch\": false,\r\n"
"            \"id\": 3,\r\n"
"            \"type\": \"std\",\r\n"
"            \"protocol_ver\": \"ev1\",\r\n"
"            \"host_type\": \"xxx\",\r\n"
"            \"central_num\": 0,\r\n"
"            \"regional_num\": 0,\r\n"
"            \"local_num\": 0,\r\n"
"            \"keyboard_addr\": 0,\r\n"
"            \"key_addr\": 0,\r\n"
"            \"crt_interface\": {\r\n"
"                \"id\": 0,\r\n"
"                \"uart_name\": \"uartx\",\r\n"
"                \"baud_rate\": 115200,\r\n"
"                \"data_bits\": 8,\r\n"
"                \"stop_bits\": 1,\r\n"
"                \"parity\": 0,\r\n"
"                \"bit_order\": 0,\r\n"
"                \"rx_bufsz\": 10240,\r\n"
"                \"dma_ping_bufsz\": 1024,\r\n"
"                \"tx_bufsz\": 1024,\r\n"
"                \"rx_en_pin\": 80,\r\n"
"                \"term_switch\": false\r\n"
"            },\r\n"
"            \"crt_protocol\": {\r\n"
"                \"lib_name\": \"xxx_crt.so\"\r\n"
"            },\r\n"
"            \"bus_interface\": {\r\n"
"                \"id\": 0,\r\n"
"                \"uart_name\": \"uartx\",\r\n"
"                \"baud_rate\": 115200,\r\n"
"                \"data_bits\": 8,\r\n"
"                \"stop_bits\": 1,\r\n"
"                \"parity\": 0,\r\n"
"                \"bit_order\": 0,\r\n"
"                \"rx_bufsz\": 10240,\r\n"
"                \"dma_ping_bufsz\": 1024,\r\n"
"                \"tx_bufsz\": 1024,\r\n"
"                \"rx_en_pin\": 80,\r\n"
"                \"term_switch\": false\r\n"
"            },\r\n"
"            \"bus_protocol\": {\r\n"
"                \"lib_name\": \"xxx_bus.so\"\r\n"
"            },\r\n"
"            \"funnel\": {\r\n"
"                \"func_id\": 0\r\n"
"            },\r\n"
"            \"store\": {\r\n"
"                \"path\": \"xxx\"\r\n"
"            },\r\n"
"            \"muart\": {\r\n"
"                \"number\": 0\r\n"
"            }\r\n"
"        }\r\n"
"    ],\r\n"
"    \"platforms\": [\r\n"
"        {\r\n"
"            \"switch\": false,\r\n"
"            \"id\": 0,\r\n"
"            \"name\": \"海云平台\",\r\n"
"            \"ip\": \"127.0.0.1\",\r\n"
"            \"port\": 18088,\r\n"
"            \"protocol\": 1,\r\n"
"            \"data_source\": [\r\n"
"                true,\r\n"
"                false,\r\n"
"                false,\r\n"
"                false\r\n"
"            ]\r\n"
"        },\r\n"
"        {\r\n"
"            \"switch\": false,\r\n"
"            \"id\": 1,\r\n"
"            \"name\": \"海云平台\",\r\n"
"            \"ip\": \"127.0.0.1\",\r\n"
"            \"port\": 18088,\r\n"
"            \"protocol\": 1,\r\n"
"            \"data_source\": [\r\n"
"                false,\r\n"
"                true,\r\n"
"                false,\r\n"
"                false\r\n"
"            ]\r\n"
"        },\r\n"
"        {\r\n"
"            \"switch\": false,\r\n"
"            \"id\": 2,\r\n"
"            \"name\": \"海云平台\",\r\n"
"            \"ip\": \"127.0.0.1\",\r\n"
"            \"port\": 18088,\r\n"
"            \"protocol\": 1,\r\n"
"            \"data_source\": [\r\n"
"                false,\r\n"
"                false,\r\n"
"                true,\r\n"
"                false\r\n"
"            ]\r\n"
"        },\r\n"
"        {\r\n"
"            \"switch\": false,\r\n"
"            \"id\": 3,\r\n"
"            \"name\": \"海云平台\",\r\n"
"            \"ip\": \"127.0.0.1\",\r\n"
"            \"port\": 18088,\r\n"
"            \"protocol\": 1,\r\n"
"            \"data_source\": [\r\n"
"                false,\r\n"
"                false,\r\n"
"                false,\r\n"
"                true\r\n"
"            ]\r\n"
"        }\r\n"
"    ],\r\n"
"    \"eth\": {\r\n"
"        \"dhcp\": true,\r\n"
"        \"ip\": \"127.0.0.1\",\r\n"
"        \"mask\": \"255.255.255.0\",\r\n"
"        \"gateway\": \"192.168.1.1\"\r\n"
"    }\r\n"
"}\r\n";

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

static rt_err_t param_check_id_array(cJSON *arr, const char *name, int max)
{
    int i, count;

    if (!cJSON_IsArray(arr))
    {
        LOG_E("param_mcu.json missing %s array", name);
        return -RT_EINVAL;
    }

    count = cJSON_GetArraySize(arr);
    if (count > max)
    {
        LOG_E("%s count %d must <= %d", name, count, max);
        return -RT_EINVAL;
    }

    for (i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        rt_uint32_t id;

        if (!cJSON_IsObject(item))
        {
            LOG_E("%s[%d] is not object", name, i);
            return -RT_EINVAL;
        }

        if (!json_get_u32(item, "id", &id) || id >= (rt_uint32_t)max)
        {
            LOG_E("%s[%d] id must < %d", name, i, max);
            return -RT_EINVAL;
        }
    }

    return RT_EOK;
}

static rt_err_t param_check_root(cJSON *root)
{
    cJSON *hosts;
    cJSON *platforms;
    int i, pcount;
    rt_err_t err;

    if (root == RT_NULL)
        return -RT_EINVAL;

    hosts = cJSON_GetObjectItemCaseSensitive(root, "hosts");
    err = param_check_id_array(hosts, "hosts", HOST_MAX);
    if (err != RT_EOK)
        return err;

    platforms = cJSON_GetObjectItemCaseSensitive(root, "platforms");
    err = param_check_id_array(platforms, "platforms", PLATFORM_MAX);
    if (err != RT_EOK)
        return err;

    pcount = cJSON_GetArraySize(platforms);
    for (i = 0; i < pcount; i++)
    {
        cJSON *plat = cJSON_GetArrayItem(platforms, i);
        cJSON *ds = cJSON_GetObjectItemCaseSensitive(plat, "data_source");
        int ds_count;

        if (ds == RT_NULL)
            continue;
        if (!cJSON_IsArray(ds))
        {
            LOG_E("platforms[%d] data_source is not array", i);
            return -RT_EINVAL;
        }

        ds_count = cJSON_GetArraySize(ds);
        if (ds_count > HOST_MAX)
        {
            LOG_E("platforms[%d] data_source count %d must <= HOST_MAX(%d)",
                  i, ds_count, HOST_MAX);
            return -RT_EINVAL;
        }
    }

    return RT_EOK;
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
        LOG_E("param_mcu.json missing hosts array");
        return -RT_EINVAL;
    }

    count = cJSON_GetArraySize(hosts);
    if (count > HOST_MAX)
    {
        LOG_E("hosts count %d must <= HOST_MAX(%d)", count, HOST_MAX);
        return -RT_EINVAL;
    }
    if (count <= 0)
    {
        LOG_W("param_mcu.json hosts is empty");
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

        if (!json_is_enabled(host_json, "switch", RT_TRUE))
        {
            LOG_I("hosts[%d] switch off, skip", i);
            continue;
        }

        type = param_parse_host_type(host_json);
        switch (type)
        {
        case HOST_TYPE_STD:
            err = host_std_create_from_json(host_json);
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

static rt_err_t param_write_text(const char *text)
{
    int fd;
    int n;
    rt_size_t len;

    if (text == RT_NULL)
        return -RT_EINVAL;

    ensure_dir("/init");

    fd = open(PARAM_JSON_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0)
    {
        LOG_E("create %s failed", PARAM_JSON_PATH);
        return -RT_ERROR;
    }

    len = rt_strlen(text);
    n = write(fd, text, len);
    close(fd);

    if (n != (int)len)
    {
        LOG_E("write %s failed", PARAM_JSON_PATH);
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t param_write_default(void)
{
    rt_err_t err;

    err = param_write_text(param_mcu_json_default);
    if (err == RT_EOK)
        LOG_I("created default %s", PARAM_JSON_PATH);
    return err;
}

static rt_err_t param_save_root(cJSON *root)
{
    char *printed;
    rt_size_t len;
    rt_err_t err;

    if (root == RT_NULL)
        return -RT_EINVAL;

    printed = cJSON_Print(root);
    if (printed == RT_NULL)
        return -RT_ENOMEM;

    len = rt_strlen(printed);
    if (len == 0 || len > PARAM_JSON_MAX)
    {
        LOG_E("param json size invalid: %u", (unsigned)len);
        cJSON_free(printed);
        return -RT_ERROR;
    }

    err = param_check_root(root);
    if (err != RT_EOK)
    {
        cJSON_free(printed);
        return err;
    }

    err = param_write_text(printed);
    cJSON_free(printed);
    return err;
}

static cJSON *param_load_root(void)
{
    char *file_buf = RT_NULL;
    const char *json_text;
    cJSON *root;

    if (param_read_file(&file_buf) == RT_EOK)
    {
        json_text = file_buf;
        LOG_I("load %s", PARAM_JSON_PATH);
    }
    else
    {
        param_write_default();
        json_text = param_mcu_json_default;
        LOG_I("use default param");
    }

    root = cJSON_Parse(json_text);
    rt_free(file_buf);
    if (root == RT_NULL)
        return RT_NULL;

    if (param_check_root(root) != RT_EOK)
    {
        cJSON_Delete(root);
        return RT_NULL;
    }

    return root;
}

static rt_err_t param_ui_apply_host_platforms(cJSON *root, rt_uint32_t host_id, cJSON *ui_platform)
{
    cJSON *platforms;
    int i, n;

    if (!cJSON_IsArray(ui_platform))
        return RT_EOK;

    if (host_id >= HOST_MAX)
    {
        LOG_E("ui host id %u out of range", host_id);
        return -RT_EINVAL;
    }

    platforms = cJSON_GetObjectItemCaseSensitive(root, "platforms");
    if (!cJSON_IsArray(platforms))
    {
        LOG_E("param missing platforms array");
        return -RT_EINVAL;
    }

    n = cJSON_GetArraySize(ui_platform);
    if (n > PLATFORM_MAX)
    {
        LOG_E("ui host.platform count %d must <= PLATFORM_MAX(%d)", n, PLATFORM_MAX);
        return -RT_EINVAL;
    }

    for (i = 0; i < n; i++)
    {
        cJSON *en = cJSON_GetArrayItem(ui_platform, i);
        cJSON *plat;
        cJSON *ds;
        rt_bool_t enabled;

        if (cJSON_IsTrue(en))
            enabled = RT_TRUE;
        else if (cJSON_IsFalse(en))
            enabled = RT_FALSE;
        else if (cJSON_IsNumber(en))
            enabled = en->valuedouble != 0;
        else
            continue;

        plat = json_array_find_by_id(platforms, (rt_uint32_t)i);
        if (!cJSON_IsObject(plat))
        {
            LOG_W("platforms id %d not found", i);
            continue;
        }

        ds = cJSON_GetObjectItemCaseSensitive(plat, "data_source");
        if (!cJSON_IsArray(ds))
        {
            ds = cJSON_CreateArray();
            json_set_item(plat, "data_source", ds);
        }

        if (cJSON_GetArraySize(ds) > HOST_MAX)
        {
            LOG_E("platforms id %d data_source count must <= HOST_MAX(%d)", i, HOST_MAX);
            return -RT_EINVAL;
        }

        while (cJSON_GetArraySize(ds) < HOST_MAX)
            cJSON_AddItemToArray(ds, cJSON_CreateBool(0));

        cJSON_ReplaceItemInArray(ds, (int)host_id, cJSON_CreateBool(enabled ? 1 : 0));
    }

    return RT_EOK;
}

static rt_err_t param_ui_apply_host(cJSON *root, cJSON *ui_host)
{
    static const char * const copy_keys[] =
    {
        "switch", "id", "type", "protocol_ver", "host_type",
        "central_num", "regional_num", "local_num",
        "keyboard_addr", "key_addr"
    };
    cJSON *hosts;
    cJSON *dst;
    cJSON *ui_platform;
    cJSON *item;
    rt_uint32_t host_id;
    rt_size_t i;
    rt_err_t err;

    if (!cJSON_IsObject(ui_host))
        return -RT_EINVAL;

    if (!json_get_u32(ui_host, "id", &host_id) || host_id >= HOST_MAX)
    {
        LOG_E("ui host id must < HOST_MAX(%d)", HOST_MAX);
        return -RT_EINVAL;
    }

    hosts = cJSON_GetObjectItemCaseSensitive(root, "hosts");
    dst = json_array_find_by_id(hosts, host_id);
    if (!cJSON_IsObject(dst))
    {
        LOG_E("hosts id %u not found", host_id);
        return -RT_EINVAL;
    }

    for (i = 0; i < sizeof(copy_keys) / sizeof(copy_keys[0]); i++)
        json_copy_key(dst, ui_host, copy_keys[i]);

    json_merge_nested(dst, ui_host, "crt_interface");
    json_merge_nested(dst, ui_host, "bus_interface");

    item = cJSON_GetObjectItemCaseSensitive(ui_host, "host_type");
    if (cJSON_IsString(item) && item->valuestring != RT_NULL)
    {
        char lib_name[64];
        cJSON *nested;

        json_set_string(dst, "host_type", item->valuestring);

        nested = json_ensure_object(dst, "crt_protocol");
        rt_snprintf(lib_name, sizeof(lib_name), "%s_crt.so", item->valuestring);
        json_set_string(nested, "lib_name", lib_name);

        nested = json_ensure_object(dst, "bus_protocol");
        rt_snprintf(lib_name, sizeof(lib_name), "%s_bus.so", item->valuestring);
        json_set_string(nested, "lib_name", lib_name);

        nested = json_ensure_object(dst, "store");
        json_set_string(nested, "path", item->valuestring);
    }

    ui_platform = cJSON_GetObjectItemCaseSensitive(ui_host, "platform");
    err = param_ui_apply_host_platforms(root, host_id, ui_platform);
    if (err != RT_EOK)
        return err;

    LOG_I("apply ui host id %u", host_id);
    return RT_EOK;
}

static rt_err_t param_ui_apply_platform(cJSON *root, cJSON *ui_platform)
{
    static const char * const copy_keys[] =
    {
        "switch", "id", "name", "ip", "port", "protocol"
    };
    cJSON *platforms;
    cJSON *dst;
    rt_uint32_t platform_id;
    rt_size_t i;

    if (!cJSON_IsObject(ui_platform))
        return -RT_EINVAL;

    if (!json_get_u32(ui_platform, "id", &platform_id) || platform_id >= PLATFORM_MAX)
    {
        LOG_E("ui platform id must < PLATFORM_MAX(%d)", PLATFORM_MAX);
        return -RT_EINVAL;
    }

    platforms = cJSON_GetObjectItemCaseSensitive(root, "platforms");
    dst = json_array_find_by_id(platforms, platform_id);
    if (!cJSON_IsObject(dst))
    {
        LOG_E("platforms id %u not found", platform_id);
        return -RT_EINVAL;
    }

    for (i = 0; i < sizeof(copy_keys) / sizeof(copy_keys[0]); i++)
        json_copy_key(dst, ui_platform, copy_keys[i]);

    LOG_I("apply ui platform id %u", platform_id);
    return RT_EOK;
}

static rt_err_t param_ui_apply_eth(cJSON *root, cJSON *ui_eth)
{
    cJSON *dst;

    if (!cJSON_IsObject(ui_eth))
        return -RT_EINVAL;

    dst = cJSON_GetObjectItemCaseSensitive(root, "eth");
    if (!cJSON_IsObject(dst))
    {
        json_set_item(root, "eth", cJSON_Duplicate(ui_eth, 1));
        LOG_I("apply ui eth (create)");
        return RT_EOK;
    }

    json_merge_object(dst, ui_eth);
    LOG_I("apply ui eth");
    return RT_EOK;
}

rt_err_t param_set_from_ui(const char *json_text)
{
    cJSON *ui_root;
    cJSON *mcu_root;
    cJSON *item;
    rt_err_t err = -RT_EINVAL;
    rt_bool_t applied = RT_FALSE;

    if (json_text == RT_NULL)
        return -RT_EINVAL;

    ui_root = cJSON_Parse(json_text);
    if (ui_root == RT_NULL)
    {
        LOG_E("parse ui param failed");
        return -RT_ERROR;
    }

    mcu_root = param_load_root();
    if (mcu_root == RT_NULL)
    {
        LOG_E("parse param_mcu.json failed");
        cJSON_Delete(ui_root);
        return -RT_ERROR;
    }

    item = cJSON_GetObjectItemCaseSensitive(ui_root, "host");
    if (item != RT_NULL)
    {
        err = param_ui_apply_host(mcu_root, item);
        if (err != RT_EOK)
            goto exit;
        applied = RT_TRUE;
    }

    item = cJSON_GetObjectItemCaseSensitive(ui_root, "platform");
    if (item != RT_NULL)
    {
        err = param_ui_apply_platform(mcu_root, item);
        if (err != RT_EOK)
            goto exit;
        applied = RT_TRUE;
    }

    item = cJSON_GetObjectItemCaseSensitive(ui_root, "eth");
    if (item != RT_NULL)
    {
        err = param_ui_apply_eth(mcu_root, item);
        if (err != RT_EOK)
            goto exit;
        applied = RT_TRUE;
    }

    if (!applied)
    {
        LOG_E("ui param has no host/platform/eth");
        err = -RT_EINVAL;
        goto exit;
    }

    err = param_save_root(mcu_root);
    if (err == RT_EOK)
        LOG_I("saved %s", PARAM_JSON_PATH);

exit:
    cJSON_Delete(ui_root);
    cJSON_Delete(mcu_root);
    return err;
}

rt_err_t param_set_from_mcu_json(void)
{
    cJSON *root;
    rt_err_t err;

    root = param_load_root();
    if (root == RT_NULL)
    {
        LOG_E("parse param_mcu.json failed");
        return -RT_ERROR;
    }

    err = param_create_hosts(root);
    cJSON_Delete(root);
    return err;
}
