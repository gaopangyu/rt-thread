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
"            \"auto_switch_adapter\": true,\r\n"
"            \"register_en\": false,\r\n"
"            \"adapter\": 1,\r\n"
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
"            \"auto_switch_adapter\": true,\r\n"
"            \"register_en\": false,\r\n"
"            \"adapter\": 1,\r\n"
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
"            \"auto_switch_adapter\": true,\r\n"
"            \"register_en\": false,\r\n"
"            \"adapter\": 1,\r\n"
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
"            \"auto_switch_adapter\": true,\r\n"
"            \"register_en\": false,\r\n"
"            \"adapter\": 1,\r\n"
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

/* ---------- 文件读写 ---------- */

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
        if (param_write_text(path, param_mcu_json_default) == RT_EOK)
            LOG_I("created default %s", path);
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

static rt_err_t param_open_edit(const char *json_text, const char *tag, cJSON **incoming, cJSON **file)
{
    if (json_text == RT_NULL)
        return -RT_EINVAL;

    *incoming = cJSON_Parse(json_text);
    if (*incoming == RT_NULL)
    {
        LOG_E("parse %s param failed", tag);
        return -RT_ERROR;
    }

    *file = param_load_root(PARAM_FILE_PATH);
    if (*file == RT_NULL)
    {
        LOG_E("parse param_mcu.json failed");
        cJSON_Delete(*incoming);
        *incoming = RT_NULL;
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t param_commit_edit(cJSON *file)
{
    rt_err_t err = param_save_root(file);

    if (err == RT_EOK)
        LOG_I("saved %s", PARAM_FILE_PATH);
    return err;
}

/* ---------- 查找、data_source、JSON 片段 ---------- */

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

static cJSON *param_host_by_id(cJSON *root, rt_uint32_t id)
{
    return json_array_find_by_id(cJSON_GetObjectItemCaseSensitive(root, "hosts"), id);
}

static cJSON *param_plat_by_id(cJSON *root, rt_uint32_t id)
{
    return json_array_find_by_id(cJSON_GetObjectItemCaseSensitive(root, "platforms"), id);
}

static cJSON *param_need_host(cJSON *root, rt_uint32_t id)
{
    cJSON *host = param_host_by_id(root, id);

    if (!cJSON_IsObject(host))
        LOG_E("hosts id %u not found", id);
    return cJSON_IsObject(host) ? host : RT_NULL;
}

static cJSON *param_need_plat(cJSON *root, rt_uint32_t id)
{
    cJSON *plat = param_plat_by_id(root, id);

    if (!cJSON_IsObject(plat))
        LOG_E("platforms id %u not found", id);
    return cJSON_IsObject(plat) ? plat : RT_NULL;
}

static rt_bool_t param_data_source_on(cJSON *plat, rt_uint32_t host_id)
{
    cJSON *ds;
    cJSON *item;

    if (!cJSON_IsObject(plat) || host_id >= HOST_MAX)
        return RT_FALSE;

    ds = cJSON_GetObjectItemCaseSensitive(plat, "data_source");
    if (!cJSON_IsArray(ds))
        return RT_FALSE;

    item = cJSON_GetArrayItem(ds, (int)host_id);
    if (cJSON_IsTrue(item))
        return RT_TRUE;
    if (cJSON_IsNumber(item))
        return item->valuedouble != 0;
    return RT_FALSE;
}

static rt_err_t param_set_data_source(cJSON *plat, rt_uint32_t host_id, rt_bool_t enabled)
{
    cJSON *ds;
    int n;

    if (!cJSON_IsObject(plat) || host_id >= HOST_MAX)
        return -RT_EINVAL;

    ds = cJSON_GetObjectItemCaseSensitive(plat, "data_source");
    if (!cJSON_IsArray(ds))
    {
        ds = cJSON_CreateArray();
        json_set_item(plat, "data_source", ds);
    }

    n = cJSON_GetArraySize(ds);
    if (n > HOST_MAX)
    {
        LOG_E("data_source count %d must <= HOST_MAX(%d)", n, HOST_MAX);
        return -RT_EINVAL;
    }

    while (cJSON_GetArraySize(ds) < HOST_MAX)
        cJSON_AddItemToArray(ds, cJSON_CreateBool(0));

    cJSON_ReplaceItemInArray(ds, (int)host_id, cJSON_CreateBool(enabled ? 1 : 0));
    return RT_EOK;
}

static void param_copy_keys(cJSON *dst, cJSON *src, const char * const *keys, rt_size_t n)
{
    rt_size_t i;

    for (i = 0; i < n; i++)
        json_copy_key(dst, src, keys[i]);
}

static cJSON *param_pick_object(cJSON *src, const char * const *keys, rt_size_t n)
{
    cJSON *dst = cJSON_CreateObject();

    if (dst == RT_NULL)
        return RT_NULL;

    param_copy_keys(dst, src, keys, n);
    return dst;
}

static rt_size_t param_print_json(cJSON *obj, char *out, rt_size_t out_sz)
{
    char *printed;
    rt_size_t n;

    if (obj == RT_NULL || out == RT_NULL || out_sz == 0)
        return 0;

    printed = cJSON_PrintUnformatted(obj);
    if (printed == RT_NULL)
        return 0;

    n = rt_strlen(printed);
    if (n == 0 || n >= out_sz)
    {
        LOG_E("param json size %u", (unsigned)n);
        cJSON_free(printed);
        return 0;
    }

    rt_memcpy(out, printed, n);
    cJSON_free(printed);
    return n;
}

static cJSON *param_wrap(const char *key, cJSON *child)
{
    cJSON *wrap;

    if (child == RT_NULL)
        return RT_NULL;

    wrap = cJSON_CreateObject();
    if (wrap == RT_NULL)
    {
        cJSON_Delete(child);
        return RT_NULL;
    }

    cJSON_AddItemToObject(wrap, key, child);
    return wrap;
}

static rt_size_t param_send_json(cJSON *obj, char *out, rt_size_t out_sz)
{
    rt_size_t n;

    if (obj == RT_NULL)
        return 0;

    n = param_print_json(obj, out, out_sz);
    cJSON_Delete(obj);
    return n;
}

/* ---------- 选择器 hosts[id] / platforms[id] / eth ---------- */

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

    if (rt_strcmp(name, "platform") == 0)
        rt_strncpy(name, "platforms", sizeof(name));

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

static rt_err_t param_open_section(const char *path, const char *selector,
                                   cJSON **root, cJSON **section, const char **key)
{
    char *argv[2];
    int next_arg;

    *root = RT_NULL;
    *section = RT_NULL;
    *key = param_reply_key(selector);
    if (*key == RT_NULL)
        return -RT_EINVAL;

    *root = param_load_root(path);
    if (*root == RT_NULL)
        return -RT_ERROR;

    argv[0] = "get_param";
    argv[1] = (char *)selector;
    if (param_resolve_section(*root, 2, argv, &next_arg, section) != RT_EOK)
    {
        cJSON_Delete(*root);
        *root = RT_NULL;
        return -RT_ERROR;
    }

    return RT_EOK;
}

static const char *param_selector_kind(const char *name, rt_bool_t has_id, rt_uint32_t id)
{
    if (name == RT_NULL)
        return RT_NULL;
    if (rt_strcmp(name, "eth") == 0)
        return has_id ? RT_NULL : "eth";
    if (rt_strcmp(name, "hosts") == 0 || rt_strcmp(name, "host") == 0)
        return (has_id && id < HOST_MAX) ? "host" : RT_NULL;
    if (rt_strcmp(name, "platforms") == 0 || rt_strcmp(name, "platform") == 0)
        return (has_id && id < PLATFORM_MAX) ? "platform" : RT_NULL;
    return RT_NULL;
}

static rt_err_t param_apply_std_host(cJSON *host, rt_bool_t *skip)
{
    *skip = RT_FALSE;
    if (param_parse_host_type(host) != HOST_TYPE_STD)
    {
        *skip = RT_TRUE;
        return RT_EOK;
    }

    return host_std_set_param_from_json(host);
}

static rt_err_t param_apply_hosts(cJSON *root)
{
    cJSON *hosts;
    cJSON *host_json;
    int i, count;
    rt_err_t ret = RT_EOK;

    hosts = cJSON_GetObjectItemCaseSensitive(root, "hosts");
    if (!cJSON_IsArray(hosts))
    {
        LOG_E("param file missing hosts array");
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
        LOG_W("param file hosts is empty");
        return RT_EOK;
    }

    for (i = 0; i < count; i++)
    {
        rt_bool_t skip;
        rt_err_t err;

        host_json = cJSON_GetArrayItem(hosts, i);
        if (!cJSON_IsObject(host_json))
        {
            LOG_W("hosts[%d] is not object, skip", i);
            continue;
        }

        err = param_apply_std_host(host_json, &skip);
        if (skip)
        {
            LOG_W("hosts[%d] unsupported type, skip", i);
            continue;
        }
        if (err != RT_EOK)
        {
            LOG_E("hosts[%d] apply failed: %d", i, err);
            ret = err;
        }
    }

    return ret;
}

/* ---------- UI：file 与界面参数互转 ---------- */

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
        rt_bool_t enabled;

        if (cJSON_IsTrue(en))
            enabled = RT_TRUE;
        else if (cJSON_IsFalse(en))
            enabled = RT_FALSE;
        else if (cJSON_IsNumber(en))
            enabled = en->valuedouble != 0;
        else
            continue;

        plat = param_plat_by_id(root, (rt_uint32_t)i);
        if (!cJSON_IsObject(plat))
        {
            LOG_W("platforms id %d not found", i);
            continue;
        }

        if (param_set_data_source(plat, host_id, enabled) != RT_EOK)
            return -RT_EINVAL;
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
    cJSON *dst;
    cJSON *ui_platform;
    cJSON *item;
    rt_uint32_t host_id;
    rt_err_t err;

    if (!cJSON_IsObject(ui_host))
        return -RT_EINVAL;

    if (!json_get_u32(ui_host, "id", &host_id) || host_id >= HOST_MAX)
    {
        LOG_E("ui host id must < HOST_MAX(%d)", HOST_MAX);
        return -RT_EINVAL;
    }

    dst = param_need_host(root, host_id);
    if (dst == RT_NULL)
        return -RT_EINVAL;

    param_copy_keys(dst, ui_host, copy_keys, sizeof(copy_keys) / sizeof(copy_keys[0]));

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
    cJSON *dst;
    rt_uint32_t platform_id;

    if (!cJSON_IsObject(ui_platform))
        return -RT_EINVAL;

    if (!json_get_u32(ui_platform, "id", &platform_id) || platform_id >= PLATFORM_MAX)
    {
        LOG_E("ui platform id must < PLATFORM_MAX(%d)", PLATFORM_MAX);
        return -RT_EINVAL;
    }

    dst = param_need_plat(root, platform_id);
    if (dst == RT_NULL)
        return -RT_EINVAL;

    param_copy_keys(dst, ui_platform, copy_keys, sizeof(copy_keys) / sizeof(copy_keys[0]));

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

static cJSON *param_file_to_ui_host(cJSON *root, cJSON *host)
{
    static const char * const host_keys[] =
    {
        "switch", "id", "type", "protocol_ver", "host_type",
        "central_num", "regional_num", "local_num",
        "keyboard_addr", "key_addr"
    };
    static const char * const crt_keys[] = { "id", "baud_rate", "term_switch" };
    static const char * const bus_keys[] = { "id", "baud_rate", "parity", "term_switch" };
    cJSON *ui;
    cJSON *crt;
    cJSON *bus;
    cJSON *platform;
    rt_uint32_t host_id = 0;
    int i;

    ui = param_pick_object(host, host_keys, sizeof(host_keys) / sizeof(host_keys[0]));
    if (ui == RT_NULL)
        return RT_NULL;

    crt = param_pick_object(cJSON_GetObjectItemCaseSensitive(host, "crt_interface"),
                            crt_keys, sizeof(crt_keys) / sizeof(crt_keys[0]));
    bus = param_pick_object(cJSON_GetObjectItemCaseSensitive(host, "bus_interface"),
                            bus_keys, sizeof(bus_keys) / sizeof(bus_keys[0]));
    json_get_u32(host, "id", &host_id);
    platform = cJSON_CreateArray();
    if (crt == RT_NULL || bus == RT_NULL || platform == RT_NULL)
    {
        cJSON_Delete(ui);
        cJSON_Delete(crt);
        cJSON_Delete(bus);
        cJSON_Delete(platform);
        return RT_NULL;
    }

    for (i = 0; i < PLATFORM_MAX; i++)
    {
        cJSON *plat = param_plat_by_id(root, (rt_uint32_t)i);

        cJSON_AddItemToArray(platform, cJSON_CreateBool(param_data_source_on(plat, host_id)));
    }

    json_set_item(ui, "crt_interface", crt);
    json_set_item(ui, "bus_interface", bus);
    json_set_item(ui, "platform", platform);
    return ui;
}

/* ---------- net：file 与合宙参数互转 ---------- */
/* tx_enable/rx_enable 不进 file，生成 net 时固定为 true。
 * auto_switch_adapter/register_en/adapter 存在 platforms。 */

static const char *param_net_protocol_name(int protocol)
{
    if (protocol == 1)
        return "ev1";
    if (protocol == 2)
        return "gb26875";
    return RT_NULL;
}

static int param_net_protocol_id(const char *name)
{
    if (name == RT_NULL)
        return -1;
    if (rt_strcmp(name, "ev1") == 0)
        return 1;
    if (rt_strcmp(name, "gb26875") == 0)
        return 2;
    return -1;
}

static int param_net_match_id(const char *s, unsigned long *id, unsigned long max)
{
    char *end;
    unsigned long v;

    if (s == RT_NULL || *s == '\0' || id == RT_NULL)
        return -1;

    v = strtoul(s, &end, 10);
    if (end == s || *end != '\0' || v >= max)
        return -1;

    *id = v;
    return 1;
}

static int param_net_parse_muart(const char *name, rt_uint32_t *host_id)
{
    unsigned long v;

    if (name == RT_NULL || rt_strncmp(name, "interface_muart_", 16) != 0)
        return 0;
    if (param_net_match_id(name + 16, &v, HOST_MAX) != 1)
        return -1;

    *host_id = (rt_uint32_t)v;
    return 1;
}

static int param_net_parse_protocol(const char *name, char *proto, rt_size_t proto_sz, rt_uint32_t *host_id)
{
    const char *body;
    const char *us;
    unsigned long v;
    rt_size_t n;

    if (name == RT_NULL || rt_strncmp(name, "protocol_", 9) != 0)
        return 0;

    body = name + 9;
    us = strrchr(body, '_');
    if (us == RT_NULL || us == body)
        return -1;

    n = (rt_size_t)(us - body);
    if (n == 0 || n >= proto_sz)
        return -1;

    rt_memcpy(proto, body, n);
    proto[n] = '\0';
    if (param_net_protocol_id(proto) < 0)
        return -1;
    if (param_net_match_id(us + 1, &v, HOST_MAX) != 1)
        return -1;

    *host_id = (rt_uint32_t)v;
    return 1;
}

static int param_net_parse_socket(const char *name, rt_uint32_t *host_id, rt_uint32_t *plat_id)
{
    const char *p;

    if (name == RT_NULL || rt_strncmp(name, "socket_cloud_", 13) != 0)
        return 0;

    p = name + 13;
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9' || p[2] != '\0')
        return -1;
    if ((rt_uint32_t)(p[0] - '0') >= HOST_MAX || (rt_uint32_t)(p[1] - '0') >= PLATFORM_MAX)
        return -1;

    *host_id = (rt_uint32_t)(p[0] - '0');
    *plat_id = (rt_uint32_t)(p[1] - '0');
    return 1;
}

static const char * const param_socket_keys[] =
{
    "ip", "port", "switch",
    "auto_switch_adapter", "register_en", "adapter"
};

static void param_net_name_muart(char *buf, rt_size_t n, rt_uint32_t host_id)
{
    rt_snprintf(buf, n, "interface_muart_%u", host_id);
}

static void param_net_name_protocol(char *buf, rt_size_t n, const char *proto, rt_uint32_t host_id)
{
    rt_snprintf(buf, n, "protocol_%s_%u", proto, host_id);
}

static void param_net_name_socket(char *buf, rt_size_t n, rt_uint32_t host_id, rt_uint32_t plat_id)
{
    rt_snprintf(buf, n, "socket_cloud_%u%u", host_id, plat_id);
}

static const char *param_host_proto(cJSON *host)
{
    cJSON *ver = cJSON_GetObjectItemCaseSensitive(host, "protocol_ver");

    if (cJSON_IsString(ver) && ver->valuestring != RT_NULL &&
        param_net_protocol_id(ver->valuestring) > 0)
        return ver->valuestring;
    return "ev1";
}

static const char *param_plat_proto(cJSON *plat, rt_uint32_t host_id)
{
    rt_uint32_t protocol = 0;

    if (!param_data_source_on(plat, host_id))
        return RT_NULL;
    if (!json_get_u32(plat, "protocol", &protocol))
        return RT_NULL;
    return param_net_protocol_name((int)protocol);
}

static void param_put_null(cJSON *obj, const char *name)
{
    cJSON_AddItemToObject(obj, name, cJSON_CreateNull());
}

static cJSON *param_make_muart(cJSON *host, rt_uint32_t host_id)
{
    cJSON *iface = cJSON_CreateObject();
    cJSON *muart;
    rt_uint32_t number = host_id;

    if (iface == RT_NULL)
        return RT_NULL;

    muart = cJSON_GetObjectItemCaseSensitive(host, "muart");
    if (cJSON_IsObject(muart))
        json_get_u32(muart, "number", &number);
    cJSON_AddNumberToObject(iface, "number", (double)number);
    cJSON_AddItemToObject(iface, "tx_enable", cJSON_CreateBool(1));
    cJSON_AddItemToObject(iface, "rx_enable", cJSON_CreateBool(1));
    return iface;
}

static cJSON *param_make_socket(cJSON *plat)
{
    cJSON *sock = param_pick_object(plat, param_socket_keys,
                                    sizeof(param_socket_keys) / sizeof(param_socket_keys[0]));

    if (sock == RT_NULL)
        return RT_NULL;
    if (cJSON_GetObjectItemCaseSensitive(sock, "auto_switch_adapter") == RT_NULL)
        cJSON_AddItemToObject(sock, "auto_switch_adapter", cJSON_CreateBool(1));
    if (cJSON_GetObjectItemCaseSensitive(sock, "register_en") == RT_NULL)
        cJSON_AddItemToObject(sock, "register_en", cJSON_CreateBool(0));
    if (cJSON_GetObjectItemCaseSensitive(sock, "adapter") == RT_NULL)
        cJSON_AddItemToObject(sock, "adapter", cJSON_CreateNumber(1));
    return sock;
}

static rt_bool_t param_json_off(cJSON *item)
{
    return cJSON_IsNull(item) || cJSON_IsFalse(item);
}

static rt_err_t param_net_name_rc(int rc, const char *kind, const char *name)
{
    if (rc > 0)
        return RT_EOK;
    if (rc == 0)
    {
        LOG_W("skip net %s %s", kind, name);
        return 1;
    }

    LOG_E("bad net %s %s", kind, name);
    return -RT_EINVAL;
}

static rt_err_t param_note_host(rt_bool_t *got, rt_uint32_t *host_id, rt_uint32_t id, const char *name)
{
    if (*got && *host_id != id)
    {
        LOG_E("bind host mismatch %s", name);
        return -RT_EINVAL;
    }

    *host_id = id;
    *got = RT_TRUE;
    return RT_EOK;
}

static cJSON *param_net_host_shell(cJSON **protocols, cJSON **interfaces, cJSON **binds)
{
    cJSON *out = cJSON_CreateObject();

    if (out == RT_NULL)
        return RT_NULL;

    *protocols = cJSON_CreateObject();
    *interfaces = cJSON_CreateObject();
    *binds = cJSON_CreateArray();
    if (*protocols == RT_NULL || *interfaces == RT_NULL || *binds == RT_NULL)
    {
        cJSON_Delete(out);
        cJSON_Delete(*protocols);
        cJSON_Delete(*interfaces);
        cJSON_Delete(*binds);
        return RT_NULL;
    }

    cJSON_AddItemToObject(out, "protocols", *protocols);
    cJSON_AddItemToObject(out, "interfaces", *interfaces);
    cJSON_AddItemToObject(out, "binds", *binds);
    return out;
}

static rt_err_t param_net_add_bind(cJSON *binds, const char *muart, const char *proto, const char *sock)
{
    cJSON *row = cJSON_CreateArray();

    if (row == RT_NULL)
        return -RT_ENOMEM;

    cJSON_AddItemToArray(row, cJSON_CreateString(muart));
    cJSON_AddItemToArray(row, cJSON_CreateString(proto));
    if (sock != RT_NULL)
        cJSON_AddItemToArray(row, cJSON_CreateString(sock));
    cJSON_AddItemToArray(binds, row);
    return RT_EOK;
}

static cJSON *param_net_from_host(cJSON *root, rt_uint32_t host_id)
{
    cJSON *out;
    cJSON *protocols;
    cJSON *interfaces;
    cJSON *binds;
    cJSON *host;
    cJSON *iface;
    char muart_name[32];
    char proto_name[40];
    char sock_name[32];
    const char *fallback;
    int used[3] = {0, 0, 0};
    int p;
    int bind_count = 0;

    host = param_host_by_id(root, host_id);
    if (!cJSON_IsObject(host))
        return RT_NULL;

    out = param_net_host_shell(&protocols, &interfaces, &binds);
    if (out == RT_NULL)
        return RT_NULL;

    param_net_name_muart(muart_name, sizeof(muart_name), host_id);
    if (!json_is_enabled(host, "switch", RT_FALSE))
    {
        param_put_null(interfaces, muart_name);
        param_net_name_protocol(proto_name, sizeof(proto_name), "ev1", host_id);
        param_put_null(protocols, proto_name);
        param_net_name_protocol(proto_name, sizeof(proto_name), "gb26875", host_id);
        param_put_null(protocols, proto_name);
        return out;
    }

    iface = param_make_muart(host, host_id);
    if (iface == RT_NULL)
    {
        cJSON_Delete(out);
        return RT_NULL;
    }
    cJSON_AddItemToObject(interfaces, muart_name, iface);

    for (p = 0; p < PLATFORM_MAX; p++)
    {
        const char *pname = param_plat_proto(param_plat_by_id(root, (rt_uint32_t)p), host_id);
        int id;

        if (pname == RT_NULL)
            continue;
        id = param_net_protocol_id(pname);
        if (id > 0)
            used[id] = 1;
    }

    fallback = param_host_proto(host);
    if (used[1] == 0 && used[2] == 0)
    {
        int id = param_net_protocol_id(fallback);

        if (id > 0)
            used[id] = 1;
    }

    for (p = 1; p <= 2; p++)
    {
        param_net_name_protocol(proto_name, sizeof(proto_name), param_net_protocol_name(p), host_id);
        if (used[p])
            cJSON_AddItemToObject(protocols, proto_name, cJSON_CreateObject());
        else
            param_put_null(protocols, proto_name);
    }

    for (p = 0; p < PLATFORM_MAX; p++)
    {
        const char *pname = param_plat_proto(param_plat_by_id(root, (rt_uint32_t)p), host_id);

        if (pname == RT_NULL)
            continue;

        param_net_name_protocol(proto_name, sizeof(proto_name), pname, host_id);
        param_net_name_socket(sock_name, sizeof(sock_name), host_id, (rt_uint32_t)p);
        if (param_net_add_bind(binds, muart_name, proto_name, sock_name) != RT_EOK)
        {
            cJSON_Delete(out);
            return RT_NULL;
        }
        bind_count++;
    }

    if (bind_count == 0)
    {
        param_net_name_protocol(proto_name, sizeof(proto_name), fallback, host_id);
        if (param_net_add_bind(binds, muart_name, proto_name, RT_NULL) != RT_EOK)
        {
            cJSON_Delete(out);
            return RT_NULL;
        }
    }

    return out;
}

static cJSON *param_net_from_platform(cJSON *root, rt_uint32_t plat_id)
{
    cJSON *out;
    cJSON *sockets;
    cJSON *plat;
    int h;

    plat = param_plat_by_id(root, plat_id);
    if (!cJSON_IsObject(plat))
        return RT_NULL;

    out = cJSON_CreateObject();
    sockets = cJSON_CreateObject();
    if (out == RT_NULL || sockets == RT_NULL)
    {
        cJSON_Delete(out);
        cJSON_Delete(sockets);
        return RT_NULL;
    }
    cJSON_AddItemToObject(out, "sockets", sockets);

    for (h = 0; h < HOST_MAX; h++)
    {
        char name[32];

        param_net_name_socket(name, sizeof(name), (rt_uint32_t)h, plat_id);
        if (param_data_source_on(plat, (rt_uint32_t)h))
        {
            cJSON *sock = param_make_socket(plat);

            if (sock == RT_NULL)
            {
                cJSON_Delete(out);
                return RT_NULL;
            }
            cJSON_AddItemToObject(sockets, name, sock);
        }
        else
            param_put_null(sockets, name);
    }

    return out;
}

static cJSON *param_net_from_eth(cJSON *root)
{
    cJSON *eth = cJSON_GetObjectItemCaseSensitive(root, "eth");
    cJSON *dup;

    if (cJSON_IsObject(eth))
        dup = cJSON_Duplicate(eth, 1);
    else
        dup = cJSON_CreateObject();
    return param_wrap("eth", dup);
}

static rt_err_t param_net_set_switch(cJSON *root, rt_uint32_t host_id, rt_bool_t on, rt_bool_t *touched)
{
    cJSON *host = param_need_host(root, host_id);

    if (host == RT_NULL)
        return -RT_EINVAL;

    json_set_item(host, "switch", cJSON_CreateBool(on ? 1 : 0));
    touched[host_id] = RT_TRUE;
    return RT_EOK;
}

static rt_err_t param_net_apply_interfaces(cJSON *root, cJSON *interfaces, rt_bool_t *touched)
{
    cJSON *child;

    if (!cJSON_IsObject(interfaces))
        return -RT_EINVAL;

    for (child = interfaces->child; child != RT_NULL; child = child->next)
    {
        rt_uint32_t host_id;
        rt_uint32_t number;
        cJSON *host;
        rt_err_t rc;

        if (child->string == RT_NULL)
            continue;

        rc = param_net_name_rc(param_net_parse_muart(child->string, &host_id), "interface", child->string);
        if (rc == 1)
            continue;
        if (rc != RT_EOK)
            return rc;

        host = param_need_host(root, host_id);
        if (host == RT_NULL)
            return -RT_EINVAL;

        if (param_json_off(child))
        {
            if (param_net_set_switch(root, host_id, RT_FALSE, touched) != RT_EOK)
                return -RT_EINVAL;
            continue;
        }
        if (!cJSON_IsObject(child))
        {
            LOG_W("skip net interface %s", child->string);
            continue;
        }

        if (param_net_set_switch(root, host_id, RT_TRUE, touched) != RT_EOK)
            return -RT_EINVAL;
        if (json_get_u32(child, "number", &number))
        {
            cJSON *muart = json_ensure_object(host, "muart");

            json_set_item(muart, "number", cJSON_CreateNumber((double)number));
        }
    }

    return RT_EOK;
}

static rt_err_t param_net_apply_protocols(cJSON *protocols)
{
    cJSON *child;

    if (!cJSON_IsObject(protocols))
        return -RT_EINVAL;

    for (child = protocols->child; child != RT_NULL; child = child->next)
    {
        char proto[32];
        rt_uint32_t host_id;
        rt_err_t rc;

        if (child->string == RT_NULL)
            continue;

        rc = param_net_name_rc(param_net_parse_protocol(child->string, proto, sizeof(proto), &host_id),
                               "protocol", child->string);
        if (rc == 1)
            continue;
        if (rc != RT_EOK)
            return rc;
    }

    return RT_EOK;
}

static rt_err_t param_net_apply_sockets(cJSON *root, cJSON *sockets)
{
    cJSON *child;

    if (!cJSON_IsObject(sockets))
        return -RT_EINVAL;

    for (child = sockets->child; child != RT_NULL; child = child->next)
    {
        rt_uint32_t host_id;
        rt_uint32_t plat_id;
        cJSON *plat;
        rt_err_t rc;

        if (child->string == RT_NULL)
            continue;

        rc = param_net_name_rc(param_net_parse_socket(child->string, &host_id, &plat_id), "socket", child->string);
        if (rc == 1)
            continue;
        if (rc != RT_EOK)
            return rc;

        plat = param_need_plat(root, plat_id);
        if (plat == RT_NULL)
            return -RT_EINVAL;

        if (param_json_off(child))
        {
            if (param_set_data_source(plat, host_id, RT_FALSE) != RT_EOK)
                return -RT_EINVAL;
            continue;
        }
        if (!cJSON_IsObject(child))
        {
            LOG_W("skip net socket %s", child->string);
            continue;
        }

        if (param_set_data_source(plat, host_id, RT_TRUE) != RT_EOK)
            return -RT_EINVAL;
        param_copy_keys(plat, child, param_socket_keys,
                        sizeof(param_socket_keys) / sizeof(param_socket_keys[0]));
    }

    return RT_EOK;
}

static rt_err_t param_net_apply_binds(cJSON *root, cJSON *binds, rt_bool_t *touched)
{
    cJSON *row;

    if (!cJSON_IsArray(binds))
        return -RT_EINVAL;

    for (row = binds->child; row != RT_NULL; row = row->next)
    {
        cJSON *item;
        cJSON *plat;
        rt_uint32_t host_id = 0;
        rt_uint32_t plat_id = PLATFORM_MAX;
        int proto = -1;
        rt_bool_t got_host = RT_FALSE;

        if (!cJSON_IsArray(row))
            continue;

        for (item = row->child; item != RT_NULL; item = item->next)
        {
            char pname[32];
            rt_uint32_t id = 0;
            rt_uint32_t pid = 0;
            int rc;

            if (!cJSON_IsString(item) || item->valuestring == RT_NULL)
                continue;

            rc = param_net_parse_muart(item->valuestring, &id);
            if (rc < 0)
                return param_net_name_rc(rc, "bind", item->valuestring);
            if (rc == 1)
            {
                if (param_note_host(&got_host, &host_id, id, item->valuestring) != RT_EOK)
                    return -RT_EINVAL;
                continue;
            }

            rc = param_net_parse_protocol(item->valuestring, pname, sizeof(pname), &id);
            if (rc < 0)
                return param_net_name_rc(rc, "bind", item->valuestring);
            if (rc == 1)
            {
                if (param_note_host(&got_host, &host_id, id, item->valuestring) != RT_EOK)
                    return -RT_EINVAL;
                proto = param_net_protocol_id(pname);
                continue;
            }

            rc = param_net_parse_socket(item->valuestring, &id, &pid);
            if (rc < 0)
                return param_net_name_rc(rc, "bind", item->valuestring);
            if (rc == 1)
            {
                if (param_note_host(&got_host, &host_id, id, item->valuestring) != RT_EOK)
                    return -RT_EINVAL;
                plat_id = pid;
            }
        }

        if (!got_host)
            continue;

        if (param_net_set_switch(root, host_id, RT_TRUE, touched) != RT_EOK)
            return -RT_EINVAL;
        if (plat_id >= PLATFORM_MAX)
            continue;

        plat = param_need_plat(root, plat_id);
        if (plat == RT_NULL)
            return -RT_EINVAL;
        if (param_set_data_source(plat, host_id, RT_TRUE) != RT_EOK)
            return -RT_EINVAL;
        if (proto > 0)
            json_set_item(plat, "protocol", cJSON_CreateNumber((double)proto));
    }

    return RT_EOK;
}

static rt_err_t param_net_apply_runtime(cJSON *root, const rt_bool_t *touched)
{
    int i;

    for (i = 0; i < HOST_MAX; i++)
    {
        cJSON *host;
        rt_bool_t skip;
        rt_err_t err;

        if (!touched[i])
            continue;

        host = param_host_by_id(root, (rt_uint32_t)i);
        err = param_apply_std_host(host, &skip);
        if (skip)
        {
            LOG_W("hosts id %d unsupported type, skip", i);
            continue;
        }
        if (err != RT_EOK)
        {
            LOG_E("hosts id %d net apply failed: %d", i, err);
            return err;
        }
        LOG_I("apply net host id %d", i);
    }

    return RT_EOK;
}
/* ---------- MSH ---------- */

#ifdef RT_USING_FINSH
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

/* ---------- 对外接口 ---------- */

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

    err = param_apply_hosts(root);
    cJSON_Delete(root);
    return err;
}

rt_size_t param_get_from_file(char *path, const char *selector, char *out, rt_size_t out_sz)
{
    cJSON *root;
    cJSON *section;
    cJSON *wrap;
    const char *key;

    if (path == RT_NULL || selector == RT_NULL || out == RT_NULL || out_sz == 0)
        return 0;
    if (param_open_section(path, selector, &root, &section, &key) != RT_EOK)
        return 0;

    wrap = param_wrap(key, cJSON_Duplicate(section, 1));
    cJSON_Delete(root);
    return param_send_json(wrap, out, out_sz);
}

rt_err_t param_set_from_ui(const char *json_text)
{
    cJSON *ui_root;
    cJSON *mcu_root;
    cJSON *item;
    rt_err_t err = -RT_EINVAL;
    rt_bool_t applied = RT_FALSE;

    err = param_open_edit(json_text, "ui", &ui_root, &mcu_root);
    if (err != RT_EOK)
        return err;

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

    err = param_commit_edit(mcu_root);

exit:
    cJSON_Delete(ui_root);
    cJSON_Delete(mcu_root);
    return err;
}

rt_size_t param_get_to_ui(char *path, const char *selector, char *out, rt_size_t out_sz)
{
    static const char * const plat_keys[] =
    {
        "switch", "id", "name", "ip", "port", "protocol"
    };
    cJSON *root;
    cJSON *section;
    cJSON *ui_obj = RT_NULL;
    const char *key;

    if (path == RT_NULL || selector == RT_NULL || out == RT_NULL || out_sz == 0)
        return 0;
    if (param_open_section(path, selector, &root, &section, &key) != RT_EOK)
        return 0;

    if (rt_strcmp(key, "host") == 0)
        ui_obj = param_file_to_ui_host(root, section);
    else if (rt_strcmp(key, "platform") == 0)
        ui_obj = param_pick_object(section, plat_keys, sizeof(plat_keys) / sizeof(plat_keys[0]));
    else
        ui_obj = cJSON_Duplicate(section, 1);

    cJSON_Delete(root);
    return param_send_json(param_wrap(key, ui_obj), out, out_sz);
}

rt_err_t param_set_from_net(const char *json_text)
{
    cJSON *net_root;
    cJSON *mcu_root;
    cJSON *item;
    rt_bool_t touched[HOST_MAX];
    rt_err_t err;
    rt_bool_t applied = RT_FALSE;

    err = param_open_edit(json_text, "net", &net_root, &mcu_root);
    if (err != RT_EOK)
        return err;

    rt_memset(touched, 0, sizeof(touched));

    item = cJSON_GetObjectItemCaseSensitive(net_root, "interfaces");
    if (item != RT_NULL)
    {
        err = param_net_apply_interfaces(mcu_root, item, touched);
        if (err != RT_EOK)
            goto exit;
        applied = RT_TRUE;
    }

    item = cJSON_GetObjectItemCaseSensitive(net_root, "protocols");
    if (item != RT_NULL)
    {
        err = param_net_apply_protocols(item);
        if (err != RT_EOK)
            goto exit;
        applied = RT_TRUE;
    }

    item = cJSON_GetObjectItemCaseSensitive(net_root, "sockets");
    if (item != RT_NULL)
    {
        err = param_net_apply_sockets(mcu_root, item);
        if (err != RT_EOK)
            goto exit;
        applied = RT_TRUE;
    }

    item = cJSON_GetObjectItemCaseSensitive(net_root, "binds");
    if (item != RT_NULL)
    {
        err = param_net_apply_binds(mcu_root, item, touched);
        if (err != RT_EOK)
            goto exit;
        applied = RT_TRUE;
    }

    item = cJSON_GetObjectItemCaseSensitive(net_root, "eth");
    if (item != RT_NULL)
    {
        err = param_ui_apply_eth(mcu_root, item);
        if (err != RT_EOK)
            goto exit;
        applied = RT_TRUE;
    }

    if (!applied)
    {
        LOG_E("net param has no interfaces/protocols/sockets/binds/eth");
        err = -RT_EINVAL;
        goto exit;
    }

    err = param_net_apply_runtime(mcu_root, touched);
    if (err != RT_EOK)
        goto exit;

    err = param_commit_edit(mcu_root);

exit:
    cJSON_Delete(net_root);
    cJSON_Delete(mcu_root);
    return err;
}

rt_size_t param_get_to_net(char *path, const char *selector, char *out, rt_size_t out_sz)
{
    cJSON *root;
    cJSON *net;
    char name[16];
    rt_bool_t has_id;
    rt_uint32_t id = 0;
    const char *kind;

    if (path == RT_NULL || selector == RT_NULL || out == RT_NULL || out_sz == 0)
        return 0;
    if (param_parse_name_id(selector, name, sizeof(name), &has_id, &id) != RT_EOK)
        return 0;

    kind = param_selector_kind(name, has_id, id);
    if (kind == RT_NULL)
        return 0;

    root = param_load_root(path);
    if (root == RT_NULL)
        return 0;

    if (rt_strcmp(kind, "eth") == 0)
        net = param_net_from_eth(root);
    else if (rt_strcmp(kind, "host") == 0)
        net = param_net_from_host(root, id);
    else
        net = param_net_from_platform(root, id);

    cJSON_Delete(root);
    return param_send_json(net, out, out_sz);
}
