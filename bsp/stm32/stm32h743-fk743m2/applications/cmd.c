#include "cmd.h"
#include "protocol_net_cmd.h"
#include "protocol_net_status.h"
#include "host.h"
#include "param.h"
#include "console.h"
#include "event.h"
#include "json_util.h"

#include <string.h>

#define UPLOAD_UI_LINK_HOST     50
#define UPLOAD_UI_LINK_STATUS   98
#define UPLOAD_UI_LINK_CMD      99
#define CMD_LINK_ALL            255

#define DBG_TAG "cmd"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static rt_size_t cmd_reply_result(rt_uint8_t *reply, rt_size_t reply_size, rt_uint8_t cmd, rt_bool_t ok)
{
    if (reply_size < 2)
        return 0;

    reply[0] = cmd | 0x80;
    reply[1] = ok ? 1 : 0;
    return 2;
}

static rt_bool_t cmd_pair_bound(const char *pre_name, const char *next_name)
{
    custom_obj_t pre = (custom_obj_t)rt_object_find(pre_name, RT_Object_Class_Custom);
    custom_obj_t next = (custom_obj_t)rt_object_find(next_name, RT_Object_Class_Custom);

    return bind_get_status(pre, next, RT_TRUE);
}

static rt_bool_t cmd_net_host_bound(rt_uint8_t host_id)
{
    host_t host;

    if (host_id >= HOST_MAX)
        return RT_FALSE;

    host = g_host[host_id];
    if (host == RT_NULL || host->ops == RT_NULL || host->ops->net_rx_bind_status == RT_NULL)
        return RT_FALSE;
    return host->ops->net_rx_bind_status(host);
}

/* UI 主机链路尚未接入，接口先留在这里。 */
static rt_bool_t cmd_ui_host_bound(rt_uint8_t host_id)
{
    (void)host_id;
    return RT_FALSE;
}

/* UI 状态链路、控制链路对象尚未创建。 */
static rt_bool_t cmd_ui_status_bound(void)
{
    return RT_FALSE;
}

static rt_bool_t cmd_ui_cmd_bound(void)
{
    return RT_FALSE;
}

static rt_bool_t cmd_link_bound(enum cmd_obj_type obj_type, rt_uint8_t link_id)
{
    if (link_id == UPLOAD_NET_LINK_STATUS)
        return cmd_pair_bound("protocol_net_status", "upload_net_status");
    if (link_id == UPLOAD_NET_LINK_CMD)
        return cmd_pair_bound(PROTOCOL_NET_CMD_NAME, "upload_net_cmd");
    if (link_id == UPLOAD_UI_LINK_STATUS)
        return cmd_ui_status_bound();
    if (link_id == UPLOAD_UI_LINK_CMD)
        return cmd_ui_cmd_bound();

    /* 0~3 只属于 net 主机链路，ui 收到时按未绑定回复。 */
    if (obj_type == CMD_OBJ_UI)
    {
        if (link_id < HOST_MAX)
            return RT_FALSE;
        if (link_id >= UPLOAD_UI_LINK_HOST && link_id < UPLOAD_UI_LINK_HOST + HOST_MAX)
            return cmd_ui_host_bound(link_id - UPLOAD_UI_LINK_HOST);
        return RT_FALSE;
    }

    if (link_id < HOST_MAX)
        return cmd_net_host_bound(link_id);
    return RT_FALSE;
}

static void cmd_bind_add(cJSON *arr, enum cmd_obj_type obj_type, rt_uint8_t link_id)
{
    cJSON *item;

    if (arr == RT_NULL)
        return;

    item = cJSON_CreateObject();
    if (item == RT_NULL)
        return;

    cJSON_AddNumberToObject(item, "link_id", link_id);
    cJSON_AddItemToObject(item, "status", cJSON_CreateBool(cmd_link_bound(obj_type, link_id) ? 1 : 0));
    cJSON_AddItemToArray(arr, item);
}

static rt_size_t cmd_reply_bind(rt_uint8_t *reply, rt_size_t reply_size, rt_uint8_t cmd,
                                enum cmd_obj_type obj_type, rt_uint8_t link_id)
{
    cJSON *arr;
    rt_size_t n;
    rt_uint8_t id;

    if (reply_size < 2)
        return 0;

    arr = cJSON_CreateArray();
    if (arr == RT_NULL)
        return 0;

    if (link_id == CMD_LINK_ALL)
    {
        if (obj_type == CMD_OBJ_UI)
        {
            for (id = 0; id < HOST_MAX; id++)
                cmd_bind_add(arr, obj_type, UPLOAD_UI_LINK_HOST + id);
            cmd_bind_add(arr, obj_type, UPLOAD_UI_LINK_STATUS);
            cmd_bind_add(arr, obj_type, UPLOAD_UI_LINK_CMD);
        }
        else
        {
            for (id = 0; id < HOST_MAX; id++)
                cmd_bind_add(arr, obj_type, id);
            cmd_bind_add(arr, obj_type, UPLOAD_NET_LINK_STATUS);
            cmd_bind_add(arr, obj_type, UPLOAD_NET_LINK_CMD);
        }
    }
    else
        cmd_bind_add(arr, obj_type, link_id);

    n = json_print(arr, (char *)reply + 1, reply_size - 1);
    cJSON_Delete(arr);
    if (n == 0)
        return 0;

    reply[0] = cmd | 0x80;
    return n + 1;
}

rt_size_t cmd_parse(enum cmd_obj_type obj_type, rt_uint8_t *buffer, rt_size_t len,
                    rt_uint8_t *reply, rt_size_t reply_size)
{
    if (obj_type >= CMD_OBJ_MAX || buffer == RT_NULL || len == 0 || reply == RT_NULL || reply_size == 0)
        return 0;

    /* bit7：0 请求，1 回复。这里只处理请求。 */
    if (buffer[0] & 0x80)
        return 0;

    rt_size_t reply_len = 0;
    host_t host;
    rt_uint8_t cmd = buffer[0] & 0x7F;

    switch (cmd)
    {
        case CMD_GET_PARAM:
        {
            char selector[32];

            LOG_I("cmd parse: get param obj=%d\r\n", obj_type);
            if (len < 2 || len - 1 >= sizeof(selector))
            {
                LOG_W("cmd parse: get param frame len: %d\r\n", (int)len);
                break;
            }

            rt_memcpy(selector, buffer + 1, len - 1);
            selector[len - 1] = '\0';

            /* net 只接受 all；ui 还可按 hosts[id]、platforms[id]、eth 取一块 */
            if (obj_type == CMD_OBJ_NET)
            {
                if (rt_strcmp(selector, "all") != 0)
                {
                    LOG_W("cmd parse: get param net selector: %s\r\n", selector);
                    break;
                }
                reply_len = param_get_all_to_net(PARAM_FILE_PATH, (char *)reply + 1, reply_size - 1);
            }
            else if (rt_strcmp(selector, "all") == 0)
                reply_len = param_get_all_to_ui(PARAM_FILE_PATH, (char *)reply + 1, reply_size - 1);
            else
                reply_len = param_get_to_ui(PARAM_FILE_PATH, selector, (char *)reply + 1, reply_size - 1);

            if (reply_len == 0)
                LOG_W("cmd parse: get param failed obj=%d selector=%s\r\n", obj_type, selector);
            else
            {
                reply[0] = cmd | 0x80;
                reply_len += 1;
            }
            break;
        }
        case CMD_SET_PARAM:
        {
            char *json = RT_NULL;
            rt_err_t err;

            LOG_I("cmd parse: set param obj=%d\r\n", obj_type);
            err = -RT_ERROR;
            if (len < 2 || reply_size < 3)
                LOG_W("cmd parse: set param frame len: %d\r\n", (int)len);
            else if ((json = rt_malloc(len)) == RT_NULL)
                LOG_W("cmd parse: set param alloc failed\r\n");
            else
            {
                rt_memcpy(json, buffer + 1, len - 1);
                json[len - 1] = '\0';
                if (obj_type == CMD_OBJ_NET)
                    err = param_set_all_from_net(json);
                else
                {
                    cJSON *doc = cJSON_Parse(json);

                    /* ui 整份用 hosts/platforms；单块用 host/platform/eth */
                    if (doc != RT_NULL &&
                        (cJSON_GetObjectItemCaseSensitive(doc, "hosts") != RT_NULL ||
                         cJSON_GetObjectItemCaseSensitive(doc, "platforms") != RT_NULL))
                        err = param_set_all_from_ui(json);
                    else
                        err = param_set_from_ui(json);
                    cJSON_Delete(doc);
                }
                rt_free(json);
            }

            if (reply_size >= 3)
            {
                reply[0] = cmd | 0x80;
                reply[1] = (err == RT_EOK) ? 1 : 0;
                reply[2] = (err == RT_EOK) ? 0 : 1;
                reply_len = 3;
                LOG_I("cmd return: set param succ=%d fail=%d\r\n",
                      reply[1], reply[2]);
            }
            break;
        }
        case CMD_GET_HISTORY:
        {
            LOG_I("cmd parse: get history messages\r\n");

            /* 请求体为 struct event_history，回复为 32 字节事件数组 */
            rt_bool_t is_end = RT_FALSE;

            if (len < sizeof(struct event_history) + 1)
            {
                LOG_W("cmd parse: get history frame len: %d < sizeof(struct event_history)\r\n", (int)len);
                reply_len = event_put_end(reply + 1, reply_size - 1, &is_end);
            }
            else
            {
                struct event_history event_history;
                rt_memcpy(&event_history, buffer + 1, sizeof(struct event_history));

                if (event_history.host_id >= HOST_MAX || event_history.count == 0
                    || g_host[event_history.host_id] == RT_NULL || g_host[event_history.host_id]->ops == RT_NULL
                    || g_host[event_history.host_id]->ops->get_history == RT_NULL)
                {
                    LOG_I("cmd parse: get history invalid args host=%d count=%d \r\n",
                                                   event_history.host_id, event_history.count);
                    reply_len = event_put_end(reply + 1, reply_size - 1, &is_end);
                }
                else
                {
                    host = g_host[event_history.host_id];
                    reply_len = host->ops->get_history(host,
                                                        reply + 1,
                                                        reply_size - 1,
                                                        &event_history,
                                                        &is_end);
                    if (reply_len == 0)
                        reply_len = event_put_end(reply + 1, reply_size - 1, &is_end);
                    if (is_end && host->ops->net_rx_bind_create != RT_NULL)
                        host->ops->net_rx_bind_create(host);
                }
            }

            if (reply_len > 0)
            {
                reply[0] = cmd | 0x80;
                reply_len += 1;
            }

            break;
        }
        case CMD_DELETE_BIND:
        case CMD_CREATE_BIND:
        {
            rt_uint8_t host_id;
            rt_err_t err = -RT_ERROR;

            LOG_I("cmd parse: %s bind\r\n", cmd == CMD_DELETE_BIND ? "delete" : "create");
            if (len < 2)
            {
                LOG_W("cmd parse: bind frame len: %d < 2\r\n", (int)len);
                reply_len = cmd_reply_result(reply, reply_size, cmd, RT_FALSE);
                break;
            }

            host_id = buffer[1];
            if (host_id >= HOST_MAX
                || g_host[host_id] == RT_NULL || g_host[host_id]->ops == RT_NULL)
            {
                LOG_I("cmd parse: bind invalid host=%d\r\n", host_id);
                reply_len = cmd_reply_result(reply, reply_size, cmd, RT_FALSE);
                break;
            }

            host = g_host[host_id];
            if (cmd == CMD_DELETE_BIND)
            {
                if (host->ops->net_rx_bind_delete != RT_NULL)
                    err = host->ops->net_rx_bind_delete(host);
            }
            else if (host->ops->net_rx_bind_create != RT_NULL)
                err = host->ops->net_rx_bind_create(host);

            reply_len = cmd_reply_result(reply, reply_size, cmd, err == RT_EOK);
            if (reply_len >= 2)
                LOG_I("cmd return: %s bind: %d\r\n",
                      cmd == CMD_DELETE_BIND ? "delete" : "create", reply[1]);
            break;
        }
        case CMD_GET_BIND:
        {
            rt_uint8_t link_id;

            LOG_I("cmd parse: get bind\r\n");
            if (len < 2)
            {
                LOG_W("cmd parse: get bind frame len: %d < 2\r\n", (int)len);
                break;
            }

            /* net：0~3/48/49；ui：50~53/98/99，0~3 按未绑定；255 为当前侧全部 */
            link_id = buffer[1];
            reply_len = cmd_reply_bind(reply, reply_size, cmd, obj_type, link_id);
            LOG_I("cmd return: get bind link=%d len=%d\r\n", link_id, (int)reply_len);
            break;
        }
        case CMD_SWITCH_CONSOLE:
        {
            char name[16];
            rt_bool_t ok = RT_FALSE;

            LOG_I("cmd parse: switch console\r\n");
            if (len < 2 || len - 1 >= sizeof(name))
                LOG_W("cmd parse: switch console frame len: %d\r\n", (int)len);
            else
            {
                rt_memcpy(name, buffer + 1, len - 1);
                name[len - 1] = '\0';
                if (rt_strcmp(name, "uart") == 0)
                {
                    console_device_set(CONSOLE_DEVICE_UART);
                    ok = RT_TRUE;
                }
                else if (rt_strcmp(name, "usb") == 0)
                {
                    console_device_set(CONSOLE_DEVICE_USB_CDC);
                    ok = RT_TRUE;
                }
                else if (rt_strcmp(name, "net") == 0)
                {
                    console_device_set(CONSOLE_DEVICE_NET);
                    ok = RT_TRUE;
                }
                else
                    LOG_W("cmd parse: switch console invalid: %s\r\n", name);
            }

            reply_len = cmd_reply_result(reply, reply_size, cmd, ok);
            break;
        }
        default:
        {
            LOG_E("protocol cmd parse: unknown command: 0x%02X\r\n", buffer[0]);
            break;
        }
    }

    return reply_len;
}
