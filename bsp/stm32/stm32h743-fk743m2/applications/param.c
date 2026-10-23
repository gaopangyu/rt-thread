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
"            \"interface_crt\": {\r\n"
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
"            \"protocol_crt\": {\r\n"
"                \"lib_name\": \"xxx_crt.so\"\r\n"
"            },\r\n"
"            \"interface_bus\": {\r\n"
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
"            \"protocol_bus\": {\r\n"
"                \"lib_name\": \"xxx_bus.so\"\r\n"
"            },\r\n"
"            \"funnel_default\": {\r\n"
"                \"func_id\": 0\r\n"
"            },\r\n"
"            \"upload_store\": {\r\n"
"                \"path\": \"xxx\"\r\n"
"            },\r\n"
"            \"upload_net\": {\r\n"
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
"            \"interface_crt\": {\r\n"
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
"            \"protocol_crt\": {\r\n"
"                \"lib_name\": \"xxx_crt.so\"\r\n"
"            },\r\n"
"            \"interface_bus\": {\r\n"
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
"            \"protocol_bus\": {\r\n"
"                \"lib_name\": \"xxx_bus.so\"\r\n"
"            },\r\n"
"            \"funnel_default\": {\r\n"
"                \"func_id\": 0\r\n"
"            },\r\n"
"            \"upload_store\": {\r\n"
"                \"path\": \"xxx\"\r\n"
"            },\r\n"
"            \"upload_net\": {\r\n"
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
"            \"interface_crt\": {\r\n"
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
"            \"protocol_crt\": {\r\n"
"                \"lib_name\": \"xxx_crt.so\"\r\n"
"            },\r\n"
"            \"interface_bus\": {\r\n"
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
"            \"protocol_bus\": {\r\n"
"                \"lib_name\": \"xxx_bus.so\"\r\n"
"            },\r\n"
"            \"funnel_default\": {\r\n"
"                \"func_id\": 0\r\n"
"            },\r\n"
"            \"upload_store\": {\r\n"
"                \"path\": \"xxx\"\r\n"
"            },\r\n"
"            \"upload_net\": {\r\n"
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
"            \"interface_crt\": {\r\n"
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
"            \"protocol_crt\": {\r\n"
"                \"lib_name\": \"xxx_crt.so\"\r\n"
"            },\r\n"
"            \"interface_bus\": {\r\n"
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
"            \"protocol_bus\": {\r\n"
"                \"lib_name\": \"xxx_bus.so\"\r\n"
"            },\r\n"
"            \"funnel_default\": {\r\n"
"                \"func_id\": 0\r\n"
"            },\r\n"
"            \"upload_store\": {\r\n"
"                \"path\": \"xxx\"\r\n"
"            },\r\n"
"            \"upload_net\": {\r\n"
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

/* 读出整个 JSON 文件 */
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

/* 把文本写入文件，必要时先建 /init */
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

/* 检查数组元素是对象，且 id 在范围内 */
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

/* 检查 hosts、platforms 和 data_source */
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

/* 校验后把根对象写回参数文件 */
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

/* 读参数文件，失败则写入默认参数再解析 */
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

/* 解析下发 JSON，并加载参数文件 */
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

/* 保存参数文件 */
static rt_err_t param_commit_edit(cJSON *file)
{
    rt_err_t err = param_save_root(file);

    if (err == RT_EOK)
        LOG_I("saved %s", PARAM_FILE_PATH);
    return err;
}

/* ---------- 查找、data_source、JSON 片段 ---------- */

/* 从 type 字段得到主机类型 */
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

/* 按 id 找主机 */
static cJSON *param_host_by_id(cJSON *root, rt_uint32_t id)
{
    return json_array_find_by_id(cJSON_GetObjectItemCaseSensitive(root, "hosts"), id);
}

/* 按 id 找平台 */
static cJSON *param_plat_by_id(cJSON *root, rt_uint32_t id)
{
    return json_array_find_by_id(cJSON_GetObjectItemCaseSensitive(root, "platforms"), id);
}

/* 按 id 找主机，找不到打日志 */
static cJSON *param_need_host(cJSON *root, rt_uint32_t id)
{
    cJSON *host = param_host_by_id(root, id);

    if (!cJSON_IsObject(host))
        LOG_E("hosts id %u not found", id);
    return cJSON_IsObject(host) ? host : RT_NULL;
}

/* 按 id 找平台，找不到打日志 */
static cJSON *param_need_plat(cJSON *root, rt_uint32_t id)
{
    cJSON *plat = param_plat_by_id(root, id);

    if (!cJSON_IsObject(plat))
        LOG_E("platforms id %u not found", id);
    return cJSON_IsObject(plat) ? plat : RT_NULL;
}

/* 该平台是否接了这台主机 */
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

/* 设置平台 data_source 里这台主机的开关 */
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

/* 写到缓冲区后释放对象 */
static rt_size_t param_send_json(cJSON *obj, char *out, rt_size_t out_sz)
{
    rt_size_t n;

    if (obj == RT_NULL)
        return 0;

    n = json_print(obj, out, out_sz);
    cJSON_Delete(obj);
    return n;
}

/* ---------- 选择器 hosts[id] / platforms[id] / eth ---------- */

/* 解析 hosts[0] 这种名字和 id */
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

/* 选择器对应的回包字段名：host、platform、eth */
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

/* 按选择器取出参数文件里的一块 */
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

/* 加载参数文件，并取出选择器对应的块 */
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

    argv[0] = "param_get";
    argv[1] = (char *)selector;
    if (param_resolve_section(*root, 2, argv, &next_arg, section) != RT_EOK)
    {
        cJSON_Delete(*root);
        *root = RT_NULL;
        return -RT_ERROR;
    }

    return RT_EOK;
}

/* 按 type 字段把主机下发到运行时，未知类型跳过 */
static rt_err_t param_apply_host(cJSON *host, rt_bool_t *skip)
{
    *skip = RT_FALSE;
    switch (param_parse_host_type(host))
    {
    case HOST_TYPE_STD:
        return host_std_set_param_from_json(host);
    default:
        *skip = RT_TRUE;
        return RT_EOK;
    }
}

/* 把参数文件里的主机应用到运行时 */
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

        err = param_apply_host(host_json, &skip);
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

/* 用 host.platform[] 改各平台的 data_source */
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

/* 把界面主机写进参数文件并生效 */
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

    json_copy_keys(dst, ui_host, copy_keys, sizeof(copy_keys) / sizeof(copy_keys[0]));

    json_merge_nested(dst, ui_host, "interface_crt");
    json_merge_nested(dst, ui_host, "interface_bus");

    item = cJSON_GetObjectItemCaseSensitive(ui_host, "host_type");
    if (cJSON_IsString(item) && item->valuestring != RT_NULL)
    {
        char lib_name[64];
        cJSON *nested;

        json_set_string(dst, "host_type", item->valuestring);

        nested = json_ensure_object(dst, "protocol_crt");
        rt_snprintf(lib_name, sizeof(lib_name), "%s_crt.so", item->valuestring);
        json_set_string(nested, "lib_name", lib_name);

        nested = json_ensure_object(dst, "protocol_bus");
        rt_snprintf(lib_name, sizeof(lib_name), "%s_bus.so", item->valuestring);
        json_set_string(nested, "lib_name", lib_name);

        nested = json_ensure_object(dst, "upload_store");
        json_set_string(nested, "path", item->valuestring);
    }

    ui_platform = cJSON_GetObjectItemCaseSensitive(ui_host, "platform");
    err = param_ui_apply_host_platforms(root, host_id, ui_platform);
    if (err != RT_EOK)
        return err;

    LOG_I("apply ui host id %u", host_id);
    return RT_EOK;
}

static const char * const param_ui_plat_keys[] =
{
    "switch", "id", "name", "ip", "port", "protocol"
};

/* 把界面平台写进参数文件 */
static rt_err_t param_ui_apply_platform(cJSON *root, cJSON *ui_platform)
{
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

    json_copy_keys(dst, ui_platform, param_ui_plat_keys,
                    sizeof(param_ui_plat_keys) / sizeof(param_ui_plat_keys[0]));

    LOG_I("apply ui platform id %u", platform_id);
    return RT_EOK;
}

/* 把以太网参数合并进参数文件 */
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

/* 把参数文件里的主机转成界面格式 */
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

    ui = json_pick_object(host, host_keys, sizeof(host_keys) / sizeof(host_keys[0]));
    if (ui == RT_NULL)
        return RT_NULL;

    crt = json_pick_object(cJSON_GetObjectItemCaseSensitive(host, "interface_crt"),
                            crt_keys, sizeof(crt_keys) / sizeof(crt_keys[0]));
    bus = json_pick_object(cJSON_GetObjectItemCaseSensitive(host, "interface_bus"),
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

    json_set_item(ui, "interface_crt", crt);
    json_set_item(ui, "interface_bus", bus);
    json_set_item(ui, "platform", platform);
    return ui;
}

/* 按条数上限逐项写入数组 */
static rt_err_t param_apply_array(cJSON *root, cJSON *arr, int max,
                                  const char *what, const char *max_name,
                                  rt_err_t (*apply_one)(cJSON *root, cJSON *item))
{
    int i;
    int n;

    if (!cJSON_IsArray(arr))
        return -RT_EINVAL;

    n = cJSON_GetArraySize(arr);
    if (n > max)
    {
        LOG_E("%s count %d must <= %s(%d)", what, n, max_name, max);
        return -RT_EINVAL;
    }

    for (i = 0; i < n; i++)
    {
        rt_err_t err = apply_one(root, cJSON_GetArrayItem(arr, i));

        if (err != RT_EOK)
            return err;
    }

    return RT_EOK;
}

/* 把界面主机数组写进参数文件 */
static rt_err_t param_ui_apply_hosts(cJSON *root, cJSON *ui_hosts)
{
    return param_apply_array(root, ui_hosts, HOST_MAX, "ui hosts", "HOST_MAX", param_ui_apply_host);
}

/* 把界面平台数组写进参数文件 */
static rt_err_t param_ui_apply_platforms(cJSON *root, cJSON *ui_platforms)
{
    return param_apply_array(root, ui_platforms, PLATFORM_MAX, "ui platforms", "PLATFORM_MAX",
                             param_ui_apply_platform);
}

/* 把参数文件转成整份界面 JSON */
static cJSON *param_file_to_ui_all(cJSON *root)
{
    cJSON *out;
    cJSON *hosts;
    cJSON *platforms;
    cJSON *eth;
    int i;

    out = cJSON_CreateObject();
    hosts = cJSON_CreateArray();
    platforms = cJSON_CreateArray();
    if (out == RT_NULL || hosts == RT_NULL || platforms == RT_NULL)
    {
        cJSON_Delete(out);
        cJSON_Delete(hosts);
        cJSON_Delete(platforms);
        return RT_NULL;
    }

    for (i = 0; i < HOST_MAX; i++)
    {
        cJSON *host = param_host_by_id(root, (rt_uint32_t)i);
        cJSON *ui;

        if (!cJSON_IsObject(host))
            continue;
        ui = param_file_to_ui_host(root, host);
        if (ui == RT_NULL)
            goto fail;
        cJSON_AddItemToArray(hosts, ui);
    }

    for (i = 0; i < PLATFORM_MAX; i++)
    {
        cJSON *plat = param_plat_by_id(root, (rt_uint32_t)i);
        cJSON *ui;

        if (!cJSON_IsObject(plat))
            continue;
        ui = json_pick_object(plat, param_ui_plat_keys,
                               sizeof(param_ui_plat_keys) / sizeof(param_ui_plat_keys[0]));
        if (ui == RT_NULL)
            goto fail;
        cJSON_AddItemToArray(platforms, ui);
    }

    eth = cJSON_GetObjectItemCaseSensitive(root, "eth");
    if (cJSON_IsObject(eth))
        eth = cJSON_Duplicate(eth, 1);
    else
        eth = cJSON_CreateObject();
    if (eth == RT_NULL)
        goto fail;

    cJSON_AddItemToObject(out, "hosts", hosts);
    cJSON_AddItemToObject(out, "platforms", platforms);
    cJSON_AddItemToObject(out, "eth", eth);
    return out;

fail:
    cJSON_Delete(out);
    cJSON_Delete(hosts);
    cJSON_Delete(platforms);
    return RT_NULL;
}

/* ---------- net：file 与合宙参数互转 ---------- */
/* tx_enable/rx_enable 不进 file，生成 net 时固定为 true。
 * auto_switch_adapter/register_en/adapter 存在 platforms。 */

/* 协议号转名字：1 是 ev1，2 是 gb26875 */
static const char *param_net_protocol_name(int protocol)
{
    if (protocol == 1)
        return "ev1";
    if (protocol == 2)
        return "gb26875";
    return RT_NULL;
}

/* 协议名转协议号 */
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

/* 解析数字 id，超出 max 则失败 */
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

/* 解析 interface_muart_N 里的主机号 */
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

/* 解析 protocol_ev1_N 或 protocol_gb26875_N */
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

/* 解析 socket_cloud_HP，前一位主机、后一位平台 */
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

/* socket 从平台拷贝的字段 */
static const char * const param_socket_keys[] =
{
    "ip", "port", "switch",
    "auto_switch_adapter", "register_en", "adapter"
};

/* 生成 interface_muart 名字 */
static void param_net_name_muart(char *buf, rt_size_t n, rt_uint32_t host_id)
{
    rt_snprintf(buf, n, "interface_muart_%u", host_id);
}

/* 生成 protocol 名字 */
static void param_net_name_protocol(char *buf, rt_size_t n, const char *proto, rt_uint32_t host_id)
{
    rt_snprintf(buf, n, "protocol_%s_%u", proto, host_id);
}

/* 生成 socket_cloud 名字 */
static void param_net_name_socket(char *buf, rt_size_t n, rt_uint32_t host_id, rt_uint32_t plat_id)
{
    rt_snprintf(buf, n, "socket_cloud_%u%u", host_id, plat_id);
}

/* 主机协议名，不是已知协议时用 ev1 */
static const char *param_host_proto(cJSON *host)
{
    cJSON *ver = cJSON_GetObjectItemCaseSensitive(host, "protocol_ver");

    if (cJSON_IsString(ver) && ver->valuestring != RT_NULL &&
        param_net_protocol_id(ver->valuestring) > 0)
        return ver->valuestring;
    return "ev1";
}

/* 平台 switch 打开 */
static rt_bool_t param_plat_on(cJSON *plat)
{
    return json_is_enabled(plat, "switch", RT_FALSE);
}

/* 平台接了这台主机时返回协议名 */
static const char *param_plat_proto(cJSON *plat, rt_uint32_t host_id)
{
    rt_uint32_t protocol = 0;

    if (!param_data_source_on(plat, host_id))
        return RT_NULL;
    if (!json_get_u32(plat, "protocol", &protocol))
        return RT_NULL;
    return param_net_protocol_name((int)protocol);
}

/* 生成串口对象，收发固定为 true */
static cJSON *param_make_net(cJSON *host, rt_uint32_t host_id)
{
    cJSON *iface = cJSON_CreateObject();
    cJSON *net;
    rt_uint32_t number = host_id;

    if (iface == RT_NULL)
        return RT_NULL;

    net = cJSON_GetObjectItemCaseSensitive(host, "upload_net");
    if (cJSON_IsObject(net))
        json_get_u32(net, "number", &number);
    cJSON_AddNumberToObject(iface, "number", (double)number);
    cJSON_AddItemToObject(iface, "tx_enable", cJSON_CreateBool(1));
    cJSON_AddItemToObject(iface, "rx_enable", cJSON_CreateBool(1));
    return iface;
}

/* 从平台取出 socket 字段，缺的补默认值 */
static cJSON *param_make_socket(cJSON *plat)
{
    cJSON *sock = json_pick_object(plat, param_socket_keys,
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

/* 名字解析结果转错误码，不认识则跳过 */
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

/* 记录 bind 里的主机号，不一致则失败 */
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

/* 建好 protocols、interfaces、binds 空壳 */
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

/* 追加一条 bind */
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

/* 一台主机转成合宙参数 */
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

    if (!json_is_enabled(host, "switch", RT_FALSE))
        return out;

    param_net_name_muart(muart_name, sizeof(muart_name), host_id);
    iface = param_make_net(host, host_id);
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
        if (!used[p])
            continue;
        param_net_name_protocol(proto_name, sizeof(proto_name), param_net_protocol_name(p), host_id);
        cJSON_AddItemToObject(protocols, proto_name, cJSON_CreateObject());
    }

    for (p = 0; p < PLATFORM_MAX; p++)
    {
        cJSON *plat = param_plat_by_id(root, (rt_uint32_t)p);
        const char *pname;

        if (!param_plat_on(plat))
            continue;
        pname = param_plat_proto(plat, host_id);
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

/* 一个平台转成合宙 sockets */
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
    if (!param_plat_on(plat))
        return out;

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
    }

    return out;
}

/* 以太网转成合宙参数 */
static cJSON *param_net_from_eth(cJSON *root)
{
    cJSON *eth = cJSON_GetObjectItemCaseSensitive(root, "eth");
    cJSON *dup;

    if (cJSON_IsObject(eth))
        dup = cJSON_Duplicate(eth, 1);
    else
        dup = cJSON_CreateObject();
    return json_wrap("eth", dup);
}

/* 合成一份合宙参数，hosts 与参数文件里的 hosts 相同 */
static cJSON *param_net_from_all(cJSON *root)
{
    cJSON *out;
    cJSON *protocols;
    cJSON *interfaces;
    cJSON *sockets;
    cJSON *binds;
    int i;

    out = cJSON_CreateObject();
    protocols = cJSON_CreateObject();
    interfaces = cJSON_CreateObject();
    sockets = cJSON_CreateObject();
    binds = cJSON_CreateArray();
    if (out == RT_NULL || protocols == RT_NULL || interfaces == RT_NULL ||
        sockets == RT_NULL || binds == RT_NULL)
    {
        cJSON_Delete(out);
        cJSON_Delete(protocols);
        cJSON_Delete(interfaces);
        cJSON_Delete(sockets);
        cJSON_Delete(binds);
        return RT_NULL;
    }

    cJSON_AddItemToObject(out, "protocols", protocols);
    cJSON_AddItemToObject(out, "sockets", sockets);
    cJSON_AddItemToObject(out, "binds", binds);
    cJSON_AddItemToObject(out, "interfaces", interfaces);

    for (i = 0; i < HOST_MAX; i++)
    {
        cJSON *piece = param_net_from_host(root, (rt_uint32_t)i);
        cJSON *rows;

        if (piece == RT_NULL)
        {
            cJSON_Delete(out);
            return RT_NULL;
        }

        json_merge_object(protocols, cJSON_GetObjectItemCaseSensitive(piece, "protocols"));
        json_merge_object(interfaces, cJSON_GetObjectItemCaseSensitive(piece, "interfaces"));
        rows = cJSON_GetObjectItemCaseSensitive(piece, "binds");
        if (cJSON_IsArray(rows))
        {
            cJSON *row;

            for (row = rows->child; row != RT_NULL; row = row->next)
            {
                cJSON *dup = cJSON_Duplicate(row, 1);

                if (dup == RT_NULL)
                {
                    cJSON_Delete(piece);
                    cJSON_Delete(out);
                    return RT_NULL;
                }
                cJSON_AddItemToArray(binds, dup);
            }
        }
        cJSON_Delete(piece);
    }

    for (i = 0; i < PLATFORM_MAX; i++)
    {
        cJSON *piece = param_net_from_platform(root, (rt_uint32_t)i);

        if (piece == RT_NULL)
        {
            cJSON_Delete(out);
            return RT_NULL;
        }

        json_merge_object(sockets, cJSON_GetObjectItemCaseSensitive(piece, "sockets"));
        cJSON_Delete(piece);
    }

    {
        cJSON *eth = param_net_from_eth(root);

        if (eth == RT_NULL)
        {
            cJSON_Delete(out);
            return RT_NULL;
        }
        json_merge_object(out, eth);
        cJSON_Delete(eth);
    }

    {
        cJSON *hosts = cJSON_GetObjectItemCaseSensitive(root, "hosts");
        cJSON *dup;

        if (cJSON_IsArray(hosts))
            dup = cJSON_Duplicate(hosts, 1);
        else
            dup = cJSON_CreateArray();
        if (dup == RT_NULL)
        {
            cJSON_Delete(out);
            return RT_NULL;
        }
        cJSON_AddItemToObject(out, "hosts", dup);
    }

    json_drop_empty(out, "protocols");
    json_drop_empty(out, "sockets");
    json_drop_empty(out, "binds");
    json_drop_empty(out, "interfaces");
    cJSON_AddItemToObject(out, "clear", cJSON_CreateBool(1));
    return out;
}

/* 改主机开关，并标记需要生效 */
static rt_err_t param_net_set_switch(cJSON *root, rt_uint32_t host_id, rt_bool_t on, rt_bool_t *touched)
{
    cJSON *host = param_need_host(root, host_id);

    if (host == RT_NULL)
        return -RT_EINVAL;

    json_set_item(host, "switch", cJSON_CreateBool(on ? 1 : 0));
    touched[host_id] = RT_TRUE;
    return RT_EOK;
}

/* 从名字里记下主机号 */
static void param_net_mark_host_name(const char *name, rt_bool_t *seen)
{
    char proto[32];
    rt_uint32_t host_id = 0;
    rt_uint32_t plat_id = 0;

    if (name == RT_NULL)
        return;
    if (param_net_parse_muart(name, &host_id) != 1 &&
        param_net_parse_protocol(name, proto, sizeof(proto), &host_id) != 1 &&
        param_net_parse_socket(name, &host_id, &plat_id) != 1)
        return;
    if (host_id < HOST_MAX)
        seen[host_id] = RT_TRUE;
}

/* 从对象的字段名记下主机号 */
static void param_net_mark_keys(cJSON *obj, rt_bool_t *seen)
{
    cJSON *child;

    if (!cJSON_IsObject(obj))
        return;

    for (child = obj->child; child != RT_NULL; child = child->next)
        param_net_mark_host_name(child->string, seen);
}

/* JSON 里没有的主机号最后关掉 */
static rt_err_t param_net_drop_absent(cJSON *root, cJSON *net, rt_bool_t *touched)
{
    rt_bool_t seen[HOST_MAX];
    cJSON *child;
    cJSON *obj;
    int h;

    rt_memset(seen, 0, sizeof(seen));

    param_net_mark_keys(cJSON_GetObjectItemCaseSensitive(net, "interfaces"), seen);
    param_net_mark_keys(cJSON_GetObjectItemCaseSensitive(net, "protocols"), seen);
    param_net_mark_keys(cJSON_GetObjectItemCaseSensitive(net, "sockets"), seen);

    obj = cJSON_GetObjectItemCaseSensitive(net, "hosts");
    if (cJSON_IsArray(obj))
    {
        for (child = obj->child; child != RT_NULL; child = child->next)
        {
            rt_uint32_t id;

            if (json_get_u32(child, "id", &id) && id < HOST_MAX)
                seen[id] = RT_TRUE;
        }
    }

    obj = cJSON_GetObjectItemCaseSensitive(net, "binds");
    if (cJSON_IsArray(obj))
    {
        for (child = obj->child; child != RT_NULL; child = child->next)
        {
            cJSON *item;

            if (!cJSON_IsArray(child))
                continue;
            for (item = child->child; item != RT_NULL; item = item->next)
            {
                if (cJSON_IsString(item))
                    param_net_mark_host_name(item->valuestring, seen);
            }
        }
    }

    for (h = 0; h < HOST_MAX; h++)
    {
        if (seen[h])
            continue;
        if (!cJSON_IsObject(param_host_by_id(root, (rt_uint32_t)h)))
            continue;
        if (param_net_set_switch(root, (rt_uint32_t)h, RT_FALSE, touched) != RT_EOK)
            return -RT_EINVAL;
    }

    return RT_EOK;
}

/* 用合宙 JSON 里的 hosts 覆盖文件里的同 id 主机 */
static rt_err_t param_net_apply_hosts(cJSON *root, cJSON *net_hosts, rt_bool_t *touched)
{
    cJSON *file_hosts;
    int i;
    int n;

    if (!cJSON_IsArray(net_hosts))
        return -RT_EINVAL;

    n = cJSON_GetArraySize(net_hosts);
    if (n > HOST_MAX)
    {
        LOG_E("net hosts count %d must <= HOST_MAX(%d)", n, HOST_MAX);
        return -RT_EINVAL;
    }

    file_hosts = cJSON_GetObjectItemCaseSensitive(root, "hosts");
    if (!cJSON_IsArray(file_hosts))
    {
        LOG_E("param file missing hosts array");
        return -RT_EINVAL;
    }

    for (i = 0; i < n; i++)
    {
        cJSON *src = cJSON_GetArrayItem(net_hosts, i);
        cJSON *dup;
        rt_uint32_t id;
        int j;
        int found = 0;
        int fn;

        if (!cJSON_IsObject(src) || !json_get_u32(src, "id", &id) || id >= HOST_MAX)
        {
            LOG_E("net hosts[%d] id must < HOST_MAX(%d)", i, HOST_MAX);
            return -RT_EINVAL;
        }

        dup = cJSON_Duplicate(src, 1);
        if (dup == RT_NULL)
            return -RT_ENOMEM;

        fn = cJSON_GetArraySize(file_hosts);
        for (j = 0; j < fn; j++)
        {
            cJSON *dst = cJSON_GetArrayItem(file_hosts, j);
            rt_uint32_t dst_id;

            if (cJSON_IsObject(dst) && json_get_u32(dst, "id", &dst_id) && dst_id == id)
            {
                cJSON_ReplaceItemInArray(file_hosts, j, dup);
                dup = RT_NULL;
                found = 1;
                break;
            }
        }
        if (!found)
        {
            cJSON_Delete(dup);
            LOG_E("hosts id %u not found", id);
            return -RT_EINVAL;
        }
        touched[id] = RT_TRUE;
    }

    return RT_EOK;
}

/* 用 interface_muart 更新主机开关和串口号 */
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

        if (json_is_off(child))
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
            cJSON *net = json_ensure_object(host, "upload_net");

            json_set_item(net, "number", cJSON_CreateNumber((double)number));
        }
    }

    return RT_EOK;
}

/* 校验 protocol 名字 */
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

/* 用 socket 更新平台连接和 socket 字段 */
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

        if (json_is_off(child))
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
        json_copy_keys(plat, child, param_socket_keys,
                        sizeof(param_socket_keys) / sizeof(param_socket_keys[0]));
    }

    return RT_EOK;
}

/* 用 bind 打开主机、数据源和协议号 */
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

/* ---------- MSH ---------- */

#ifdef RT_USING_FINSH
/* 沿路径取下一层，支持名字和数组下标 */
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

/* 改对象字段或数组元素 */
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

    if (rt_strcmp(value_str, "null") == 0)
    {
        if (!cJSON_IsObject(parent))
            return -RT_EINVAL;
        if (param_parse_name_id(tok, name, sizeof(name), &has_id, &id) != RT_EOK || has_id)
            return -RT_EINVAL;
        if (cJSON_GetObjectItemCaseSensitive(parent, name) == RT_NULL)
            return -RT_EINVAL;

        cJSON_DeleteItemFromObject(parent, name);
        return RT_EOK;
    }

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
        neu = json_from_text(old, value_str);
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
    neu = json_from_text(old, value_str);
    if (neu == RT_NULL)
        return -RT_EINVAL;

    json_set_item(parent, name, neu);
    return RT_EOK;
}

/* 格式化打印到控制台 */
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

/* MSH：读出一块或整份参数 */
static int cmd_param_get(int argc, char **argv)
{
    cJSON *root;
    cJSON *section;
    int next_arg;

    if (argc < 2)
    {
        rt_kprintf("Usage: param_get <all|hosts[id]|platforms[id]|eth>\n");
        rt_kprintf("Example: param_get all\n");
        rt_kprintf("         param_get hosts[0]\n");
        rt_kprintf("         param_get platforms 1\n");
        rt_kprintf("         param_get eth\n");
        return -RT_ERROR;
    }

    root = param_load_root(PARAM_FILE_PATH);
    if (root == RT_NULL)
    {
        rt_kprintf("load %s failed\n", PARAM_FILE_PATH);
        return -RT_ERROR;
    }

    if (argc == 2 && rt_strcmp(argv[1], "all") == 0)
    {
        param_print_item(root);
        cJSON_Delete(root);
        return RT_EOK;
    }

    if (param_resolve_section(root, argc, argv, &next_arg, &section) != RT_EOK)
    {
        cJSON_Delete(root);
        return -RT_ERROR;
    }

    if (next_arg != argc)
    {
        rt_kprintf("Usage: param_get <all|hosts[id]|platforms[id]|eth>\n");
        cJSON_Delete(root);
        return -RT_ERROR;
    }

    param_print_item(section);
    cJSON_Delete(root);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_param_get, param_get, Get param: param_get all|hosts[id]|platforms[id]|eth);

/* MSH：改一个字段并保存 */
static int cmd_param_set(int argc, char **argv)
{
    cJSON *root;
    cJSON *section;
    cJSON *parent;
    int next_arg;
    int i;

    if (argc < 4)
    {
        rt_kprintf("Usage: param_set <hosts[id]|platforms[id]|eth> [keys...] <key> <value>\n");
        rt_kprintf("Example: param_set hosts[0] interface_crt uart_name uart0\n");
        rt_kprintf("         param_set eth dhcp false\n");
        rt_kprintf("         param_set hosts[0] interface_crt uart_name null\n");
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
        rt_kprintf("Usage: param_set <hosts[id]|platforms[id]|eth> [keys...] <key> <value>\n");
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
        rt_bool_t skip;

        if (param_apply_host(section, &skip) != RT_EOK)
        {
            rt_kprintf("hosts param_set failed\n");
            cJSON_Delete(root);
            return -RT_ERROR;
        }
        if (skip)
            rt_kprintf("hosts unsupported type, skip\n");
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
MSH_CMD_EXPORT_ALIAS(cmd_param_set, param_set, Set param: param_set hosts[0] interface_crt uart_name uart0);
#endif

/* 有该字段才写入，并记为已处理 */
static rt_err_t param_apply_key(cJSON *doc, cJSON *file, const char *key,
                                rt_err_t (*fn)(cJSON *file, cJSON *item),
                                rt_bool_t *applied)
{
    cJSON *item;
    rt_err_t err;

    item = cJSON_GetObjectItemCaseSensitive(doc, key);
    if (item == RT_NULL)
        return RT_EOK;

    err = fn(file, item);
    if (err != RT_EOK)
        return err;

    *applied = RT_TRUE;
    return RT_EOK;
}

/* 解析下发 JSON，写入成功后保存 */
static rt_err_t param_edit(const char *json_text, const char *tag,
                           rt_err_t (*apply)(cJSON *incoming, cJSON *file))
{
    cJSON *incoming;
    cJSON *file;
    rt_err_t err;

    err = param_open_edit(json_text, tag, &incoming, &file);
    if (err != RT_EOK)
        return err;

    err = apply(incoming, file);
    if (err == RT_EOK)
        err = param_commit_edit(file);

    cJSON_Delete(incoming);
    cJSON_Delete(file);
    return err;
}

/* 加载参数文件，可选转换后写到缓冲区 */
static rt_size_t param_load_send(char *path, char *out, rt_size_t out_sz,
                                 cJSON *(*build)(cJSON *root))
{
    cJSON *root;
    cJSON *obj;

    if (path == RT_NULL || out == RT_NULL || out_sz == 0)
        return 0;

    root = param_load_root(path);
    if (root == RT_NULL)
        return 0;

    if (build == RT_NULL)
        return param_send_json(root, out, out_sz);

    obj = build(root);
    cJSON_Delete(root);
    return param_send_json(obj, out, out_sz);
}

/* 按选择器取出一块，转换后写到缓冲区 */
static rt_size_t param_export_section(char *path, const char *selector, char *out, rt_size_t out_sz,
                                      cJSON *(*build)(cJSON *root, cJSON *section, const char *key))
{
    cJSON *root;
    cJSON *section;
    cJSON *obj;
    const char *key;

    if (path == RT_NULL || selector == RT_NULL || out == RT_NULL || out_sz == 0)
        return 0;
    if (param_open_section(path, selector, &root, &section, &key) != RT_EOK)
        return 0;

    obj = build(root, section, key);
    cJSON_Delete(root);
    return param_send_json(obj, out, out_sz);
}

/* 写入单条界面参数 */
static rt_err_t param_ui_apply_doc(cJSON *ui, cJSON *file)
{
    rt_bool_t applied = RT_FALSE;
    rt_err_t err;

    err = param_apply_key(ui, file, "host", param_ui_apply_host, &applied);
    if (err != RT_EOK)
        return err;
    err = param_apply_key(ui, file, "platform", param_ui_apply_platform, &applied);
    if (err != RT_EOK)
        return err;
    err = param_apply_key(ui, file, "eth", param_ui_apply_eth, &applied);
    if (err != RT_EOK)
        return err;
    if (!applied)
    {
        LOG_E("ui param has no host/platform/eth");
        return -RT_EINVAL;
    }
    return param_apply_hosts(file);
}

/* 写入整份界面参数 */
static rt_err_t param_ui_apply_all_doc(cJSON *ui, cJSON *file)
{
    rt_bool_t applied = RT_FALSE;
    rt_err_t err;

    err = param_apply_key(ui, file, "hosts", param_ui_apply_hosts, &applied);
    if (err != RT_EOK)
        return err;
    err = param_apply_key(ui, file, "platforms", param_ui_apply_platforms, &applied);
    if (err != RT_EOK)
        return err;
    err = param_apply_key(ui, file, "eth", param_ui_apply_eth, &applied);
    if (err != RT_EOK)
        return err;
    if (!applied)
    {
        LOG_E("ui param has no hosts/platforms/eth");
        return -RT_EINVAL;
    }
    return param_apply_hosts(file);
}

/* 按选择器把一块转成界面 JSON */
static cJSON *param_section_to_ui(cJSON *root, cJSON *section, const char *key)
{
    cJSON *ui;

    if (rt_strcmp(key, "host") == 0)
        ui = param_file_to_ui_host(root, section);
    else if (rt_strcmp(key, "platform") == 0)
        ui = json_pick_object(section, param_ui_plat_keys,
                               sizeof(param_ui_plat_keys) / sizeof(param_ui_plat_keys[0]));
    else
        ui = cJSON_Duplicate(section, 1);
    return json_wrap(key, ui);
}

/* 按选择器原样包一层 */
static cJSON *param_section_dup(cJSON *root, cJSON *section, const char *key)
{
    (void)root;
    return json_wrap(key, cJSON_Duplicate(section, 1));
}

/* 写入整份合宙参数 */
static rt_err_t param_net_apply_doc(cJSON *net, cJSON *file)
{
    cJSON *item;
    rt_bool_t touched[HOST_MAX];
    rt_bool_t applied = RT_FALSE;
    rt_err_t err;

    rt_memset(touched, 0, sizeof(touched));

    item = cJSON_GetObjectItemCaseSensitive(net, "interfaces");
    if (item != RT_NULL)
    {
        err = param_net_apply_interfaces(file, item, touched);
        if (err != RT_EOK)
            return err;
        applied = RT_TRUE;
    }

    item = cJSON_GetObjectItemCaseSensitive(net, "protocols");
    if (item != RT_NULL)
    {
        err = param_net_apply_protocols(item);
        if (err != RT_EOK)
            return err;
        applied = RT_TRUE;
    }

    item = cJSON_GetObjectItemCaseSensitive(net, "sockets");
    if (item != RT_NULL)
    {
        err = param_net_apply_sockets(file, item);
        if (err != RT_EOK)
            return err;
        applied = RT_TRUE;
    }

    item = cJSON_GetObjectItemCaseSensitive(net, "binds");
    if (item != RT_NULL)
    {
        err = param_net_apply_binds(file, item, touched);
        if (err != RT_EOK)
            return err;
        applied = RT_TRUE;
    }

    err = param_apply_key(net, file, "eth", param_ui_apply_eth, &applied);
    if (err != RT_EOK)
        return err;

    item = cJSON_GetObjectItemCaseSensitive(net, "hosts");
    if (item != RT_NULL)
    {
        err = param_net_apply_hosts(file, item, touched);
        if (err != RT_EOK)
            return err;
        applied = RT_TRUE;
    }

    if (!applied)
    {
        LOG_E("net param has no interfaces/protocols/sockets/binds/eth/hosts");
        return -RT_EINVAL;
    }

    err = param_net_drop_absent(file, net, touched);
    if (err != RT_EOK)
        return err;
    return param_apply_hosts(file);
}

/* ---------- 对外接口 ---------- */

/* 加载参数文件，并让主机生效 */
rt_err_t param_set_all_from_file(char *path)
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

/* 按选择器读出参数文件里的一块 */
rt_size_t param_get_from_file(char *path, const char *selector, char *out, rt_size_t out_sz)
{
    return param_export_section(path, selector, out, out_sz, param_section_dup);
}

/* 读出整份参数文件：hosts、platforms、eth */
rt_size_t param_get_all_from_file(char *path, char *out, rt_size_t out_sz)
{
    return param_load_send(path, out, out_sz, RT_NULL);
}

/* 把界面 JSON 合并进参数文件 */
rt_err_t param_set_from_ui(const char *json_text)
{
    return param_edit(json_text, "ui", param_ui_apply_doc);
}

/* 按选择器转成界面 JSON */
rt_size_t param_get_to_ui(char *path, const char *selector, char *out, rt_size_t out_sz)
{
    return param_export_section(path, selector, out, out_sz, param_section_to_ui);
}

/* 把整份界面 JSON 写进参数文件并生效 */
rt_err_t param_set_all_from_ui(const char *json_text)
{
    return param_edit(json_text, "ui", param_ui_apply_all_doc);
}

/* 把整份参数转成界面 JSON */
rt_size_t param_get_all_to_ui(char *path, char *out, rt_size_t out_sz)
{
    return param_load_send(path, out, out_sz, param_file_to_ui_all);
}

/* 把整份合宙 JSON 写进参数文件并生效 */
rt_err_t param_set_all_from_net(const char *json_text)
{
    return param_edit(json_text, "net", param_net_apply_doc);
}

/* 把整份参数转成合宙 JSON */
rt_size_t param_get_all_to_net(char *path, char *out, rt_size_t out_sz)
{
    return param_load_send(path, out, out_sz, param_net_from_all);
}
