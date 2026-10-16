#include "param.h"
#include "host_std.h"
#include "store.h"
#include "json_util.h"

#include <cJSON.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

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

static rt_err_t param_read_file(const char *path, char **out)
{
    struct stat st;
    int fd;
    int n;
    char *buf;

    *out = RT_NULL;

    if (path == RT_NULL)
        return -RT_EINVAL;

    if (stat(path, &st) != 0)
        return -RT_ERROR;

    if (st.st_size <= 0 || st.st_size > PARAM_JSON_MAX)
    {
        LOG_E("%s size invalid: %ld", path, (long)st.st_size);
        return -RT_ERROR;
    }

    fd = open(path, O_RDONLY, 0);
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

static rt_err_t param_write_text(const char *path, const char *text)
{
    int fd;
    int n;
    rt_size_t len;

    if (path == RT_NULL || text == RT_NULL)
        return -RT_EINVAL;

    ensure_dir("/init");

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0)
    {
        LOG_E("create %s failed", path);
        return -RT_ERROR;
    }

    len = rt_strlen(text);
    n = write(fd, text, len);
    close(fd);

    if (n != (int)len)
    {
        LOG_E("write %s failed", path);
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t param_write_default(const char *path)
{
    rt_err_t err;

    err = param_write_text(path, param_mcu_json_default);
    if (err == RT_EOK)
        LOG_I("created default %s", path);
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

    err = param_write_text(PARAM_FILE_PATH, printed);
    cJSON_free(printed);
    return err;
}

static cJSON *param_load_root(const char *path)
{
    char *file_buf = RT_NULL;
    const char *json_text;
    cJSON *root;

    if (path == RT_NULL)
        return RT_NULL;

    if (param_read_file(path, &file_buf) == RT_EOK)
    {
        json_text = file_buf;
        LOG_I("load %s", path);
    }
    else
    {
        param_write_default(path);
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

    err = host_std_set_param_from_json(dst);
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

    mcu_root = param_load_root(PARAM_FILE_PATH);
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
        LOG_I("saved %s", PARAM_FILE_PATH);

exit:
    cJSON_Delete(ui_root);
    cJSON_Delete(mcu_root);
    return err;
}

rt_err_t param_set_from_file(char *path)
{
    cJSON *root;
    rt_err_t err;

    if (path == RT_NULL)
        return -RT_EINVAL;

    root = param_load_root(path);
    if (root == RT_NULL)
    {
        LOG_E("parse %s failed", path);
        return -RT_ERROR;
    }

    err = param_create_hosts(root);
    cJSON_Delete(root);
    return err;
}

#ifdef RT_USING_FINSH
static rt_err_t param_parse_name_id(const char *s, char *name, rt_size_t name_sz,
                                    rt_bool_t *has_id, rt_uint32_t *id)
{
    const char *lb;
    const char *rb;
    rt_size_t nlen;
    char *end;
    unsigned long v;

    if (s == RT_NULL || name == RT_NULL || name_sz == 0 || has_id == RT_NULL || id == RT_NULL)
        return -RT_EINVAL;

    *has_id = RT_FALSE;
    *id = 0;

    lb = strchr(s, '[');
    if (lb == RT_NULL)
    {
        rt_strncpy(name, s, name_sz - 1);
        name[name_sz - 1] = '\0';
        return RT_EOK;
    }

    rb = strchr(lb, ']');
    if (rb == RT_NULL || rb[1] != '\0' || lb == s)
        return -RT_EINVAL;

    nlen = (rt_size_t)(lb - s);
    if (nlen == 0 || nlen >= name_sz)
        return -RT_EINVAL;

    rt_memcpy(name, s, nlen);
    name[nlen] = '\0';

    v = strtoul(lb + 1, &end, 10);
    if (end != rb)
        return -RT_EINVAL;

    *has_id = RT_TRUE;
    *id = (rt_uint32_t)v;
    return RT_EOK;
}

static void param_normalize_section_name(char *name)
{
    if (name != RT_NULL && rt_strcmp(name, "platform") == 0)
        rt_strncpy(name, "platforms", sizeof("platforms"));
}

static rt_err_t param_resolve_section(cJSON *root, int argc, char **argv, int *next_arg, cJSON **section)
{
    char name[16];
    rt_bool_t has_id;
    rt_uint32_t id;
    cJSON *item;
    int max;

    if (root == RT_NULL || argv == RT_NULL || next_arg == RT_NULL || section == RT_NULL || argc < 2)
        return -RT_EINVAL;

    if (param_parse_name_id(argv[1], name, sizeof(name), &has_id, &id) != RT_EOK)
    {
        rt_kprintf("invalid selector: %s\n", argv[1]);
        return -RT_EINVAL;
    }

    *next_arg = 2;
    if (!has_id && argc >= 3)
    {
        char *end;
        unsigned long v = strtoul(argv[2], &end, 10);
        if (end != argv[2] && *end == '\0')
        {
            has_id = RT_TRUE;
            id = (rt_uint32_t)v;
            *next_arg = 3;
        }
    }

    param_normalize_section_name(name);
    item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (item == RT_NULL)
    {
        rt_kprintf("unknown section: %s\n", name);
        return -RT_EINVAL;
    }

    if (cJSON_IsArray(item))
    {
        max = (rt_strcmp(name, "hosts") == 0) ? HOST_MAX : PLATFORM_MAX;
        if (!has_id)
        {
            rt_kprintf("need id, e.g. %s[0]\n", name);
            return -RT_EINVAL;
        }
        if (id >= (rt_uint32_t)max)
        {
            rt_kprintf("%s id %u must < %d\n", name, id, max);
            return -RT_EINVAL;
        }

        *section = json_array_find_by_id(item, id);
        if (*section == RT_NULL)
        {
            rt_kprintf("%s id %u not found\n", name, id);
            return -RT_EINVAL;
        }
        return RT_EOK;
    }

    if (has_id)
    {
        rt_kprintf("%s is not array\n", name);
        return -RT_EINVAL;
    }

    *section = item;
    return RT_EOK;
}

static const char *param_reply_key(const char *selector)
{
    if (selector == RT_NULL)
        return RT_NULL;
    if (rt_strncmp(selector, "hosts", 5) == 0 || rt_strcmp(selector, "host") == 0)
        return "host";
    if (rt_strncmp(selector, "platforms", 9) == 0 || rt_strncmp(selector, "platform", 8) == 0)
        return "platform";
    if (rt_strcmp(selector, "eth") == 0)
        return "eth";
    return RT_NULL;
}

rt_size_t param_get_from_file(char *path, const char *selector, char *out, rt_size_t out_sz)
{
    cJSON *root;
    cJSON *section;
    cJSON *wrap;
    char *argv[2];
    int next_arg;
    const char *key;
    char *printed;
    rt_size_t n;

    if (path == RT_NULL || selector == RT_NULL || out == RT_NULL || out_sz == 0)
        return 0;

    key = param_reply_key(selector);
    if (key == RT_NULL)
        return 0;

    root = param_load_root(path);
    if (root == RT_NULL)
        return 0;

    argv[0] = "get_param";
    argv[1] = (char *)selector;
    if (param_resolve_section(root, 2, argv, &next_arg, &section) != RT_EOK)
    {
        cJSON_Delete(root);
        return 0;
    }

    wrap = cJSON_CreateObject();
    if (wrap == RT_NULL)
    {
        cJSON_Delete(root);
        return 0;
    }
    cJSON_AddItemToObject(wrap, key, cJSON_Duplicate(section, 1));
    cJSON_Delete(root);

    printed = cJSON_PrintUnformatted(wrap);
    cJSON_Delete(wrap);
    if (printed == RT_NULL)
        return 0;

    n = rt_strlen(printed);
    if (n == 0 || n >= out_sz)
    {
        LOG_E("param get json size %u", (unsigned)n);
        cJSON_free(printed);
        return 0;
    }

    rt_memcpy(out, printed, n);
    cJSON_free(printed);
    return n;
}

static cJSON *param_child(cJSON *cur, const char *tok)
{
    char name[32];
    rt_bool_t has_id;
    rt_uint32_t id;
    char *end;
    unsigned long v;

    if (cur == RT_NULL || tok == RT_NULL)
        return RT_NULL;

    if (cJSON_IsArray(cur))
    {
        if (param_parse_name_id(tok, name, sizeof(name), &has_id, &id) == RT_EOK && has_id)
            return json_array_find_by_id(cur, id);

        v = strtoul(tok, &end, 10);
        if (end != tok && *end == '\0')
            return cJSON_GetArrayItem(cur, (int)v);
        return RT_NULL;
    }

    if (!cJSON_IsObject(cur))
        return RT_NULL;

    if (param_parse_name_id(tok, name, sizeof(name), &has_id, &id) != RT_EOK)
        return RT_NULL;

    cur = cJSON_GetObjectItemCaseSensitive(cur, name);
    if (!has_id)
        return cur;
    if (cJSON_IsArray(cur))
        return json_array_find_by_id(cur, id);
    return RT_NULL;
}

static cJSON *param_make_value(cJSON *old_item, const char *s)
{
    char *end;
    double d;

    if (s == RT_NULL)
        return RT_NULL;

    if (old_item != RT_NULL)
    {
        if (cJSON_IsBool(old_item))
        {
            if (rt_strcmp(s, "true") == 0 || rt_strcmp(s, "1") == 0)
                return cJSON_CreateBool(1);
            if (rt_strcmp(s, "false") == 0 || rt_strcmp(s, "0") == 0)
                return cJSON_CreateBool(0);
            return RT_NULL;
        }
        if (cJSON_IsNumber(old_item))
        {
            d = strtod(s, &end);
            if (end == s || *end != '\0')
                return RT_NULL;
            return cJSON_CreateNumber(d);
        }
        if (cJSON_IsString(old_item))
            return cJSON_CreateString(s);
    }

    if (rt_strcmp(s, "true") == 0)
        return cJSON_CreateBool(1);
    if (rt_strcmp(s, "false") == 0)
        return cJSON_CreateBool(0);

    d = strtod(s, &end);
    if (end != s && *end == '\0')
        return cJSON_CreateNumber(d);

    return cJSON_CreateString(s);
}

static rt_err_t param_set_leaf(cJSON *parent, const char *tok, const char *value_str)
{
    char name[32];
    rt_bool_t has_id;
    rt_uint32_t id;
    cJSON *old;
    cJSON *neu;
    char *end;
    unsigned long v;

    if (parent == RT_NULL || tok == RT_NULL || value_str == RT_NULL)
        return -RT_EINVAL;

    if (cJSON_IsArray(parent))
    {
        if (param_parse_name_id(tok, name, sizeof(name), &has_id, &id) == RT_EOK && has_id)
            v = id;
        else
        {
            v = strtoul(tok, &end, 10);
            if (end == tok || *end != '\0')
                return -RT_EINVAL;
        }

        if (v >= (unsigned long)cJSON_GetArraySize(parent))
            return -RT_EINVAL;

        old = cJSON_GetArrayItem(parent, (int)v);
        neu = param_make_value(old, value_str);
        if (neu == RT_NULL)
            return -RT_EINVAL;
        cJSON_ReplaceItemInArray(parent, (int)v, neu);
        return RT_EOK;
    }

    if (!cJSON_IsObject(parent))
        return -RT_EINVAL;

    if (param_parse_name_id(tok, name, sizeof(name), &has_id, &id) != RT_EOK || has_id)
        return -RT_EINVAL;

    old = cJSON_GetObjectItemCaseSensitive(parent, name);
    neu = param_make_value(old, value_str);
    if (neu == RT_NULL)
        return -RT_EINVAL;

    json_set_item(parent, name, neu);
    return RT_EOK;
}

static void param_print_item(cJSON *item)
{
    char *printed;

    if (item == RT_NULL)
        return;

    printed = cJSON_Print(item);
    if (printed == RT_NULL)
    {
        rt_kprintf("print json failed\n");
        return;
    }

    rt_kprintf("%s\n", printed);
    cJSON_free(printed);
}

static int cmd_get_param(int argc, char **argv)
{
    cJSON *root;
    cJSON *section;
    int next_arg;

    if (argc < 2)
    {
        rt_kprintf("Usage: get_param <hosts[id]|platforms[id]|eth>\n");
        rt_kprintf("Example: get_param hosts[0]\n");
        rt_kprintf("         get_param platforms 1\n");
        rt_kprintf("         get_param eth\n");
        return -RT_ERROR;
    }

    root = param_load_root(PARAM_FILE_PATH);
    if (root == RT_NULL)
    {
        rt_kprintf("load %s failed\n", PARAM_FILE_PATH);
        return -RT_ERROR;
    }

    if (param_resolve_section(root, argc, argv, &next_arg, &section) != RT_EOK)
    {
        cJSON_Delete(root);
        return -RT_ERROR;
    }

    if (next_arg != argc)
    {
        rt_kprintf("Usage: get_param <hosts[id]|platforms[id]|eth>\n");
        cJSON_Delete(root);
        return -RT_ERROR;
    }

    param_print_item(section);
    cJSON_Delete(root);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_get_param, get_param, Get param: get_param hosts[id]|platforms[id]|eth);

static int cmd_set_param(int argc, char **argv)
{
    cJSON *root;
    cJSON *section;
    cJSON *parent;
    int next_arg;
    int i;

    if (argc < 4)
    {
        rt_kprintf("Usage: set_param <hosts[id]|platforms[id]|eth> [keys...] <key> <value>\n");
        rt_kprintf("Example: set_param hosts[0] crt_interface uart_name uart0\n");
        rt_kprintf("         set_param eth dhcp false\n");
        return -RT_ERROR;
    }

    root = param_load_root(PARAM_FILE_PATH);
    if (root == RT_NULL)
    {
        rt_kprintf("load %s failed\n", PARAM_FILE_PATH);
        return -RT_ERROR;
    }

    if (param_resolve_section(root, argc, argv, &next_arg, &section) != RT_EOK)
    {
        cJSON_Delete(root);
        return -RT_ERROR;
    }

    if (argc - next_arg < 2)
    {
        rt_kprintf("Usage: set_param <hosts[id]|platforms[id]|eth> [keys...] <key> <value>\n");
        cJSON_Delete(root);
        return -RT_ERROR;
    }

    parent = section;
    for (i = next_arg; i < argc - 2; i++)
    {
        parent = param_child(parent, argv[i]);
        if (parent == RT_NULL)
        {
            rt_kprintf("path not found: %s\n", argv[i]);
            cJSON_Delete(root);
            return -RT_ERROR;
        }
    }

    if (param_set_leaf(parent, argv[argc - 2], argv[argc - 1]) != RT_EOK)
    {
        rt_kprintf("set %s = %s failed\n", argv[argc - 2], argv[argc - 1]);
        cJSON_Delete(root);
        return -RT_ERROR;
    }

    if (rt_strncmp(argv[1], "hosts", 5) == 0)
    {
        if (host_std_set_param_from_json(section) != RT_EOK)
        {
            rt_kprintf("hosts set_param failed\n");
            cJSON_Delete(root);
            return -RT_ERROR;
        }
    }

    if (param_save_root(root) != RT_EOK)
    {
        rt_kprintf("save %s failed\n", PARAM_FILE_PATH);
        cJSON_Delete(root);
        return -RT_ERROR;
    }

    rt_kprintf("set ok: %s = %s\n", argv[argc - 2], argv[argc - 1]);
    cJSON_Delete(root);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_set_param, set_param, Set param: set_param hosts[0] crt_interface uart_name uart0);
#endif

