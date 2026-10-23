#include "cmd.h"
#include "host.h"
#include "param.h"
#include "console.h"
#include "event.h"

#include <string.h>

#define DBG_TAG "cmd"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

rt_size_t cmd_parse(enum cmd_obj_type obj_type, rt_uint8_t *buffer, rt_size_t len,
                    rt_uint8_t *reply, rt_size_t reply_size)
{
    rt_uint8_t net_cmd;
    rt_size_t reply_len = 0;
    host_t host;

    if (obj_type >= CMD_OBJ_MAX || buffer == RT_NULL || len == 0 || reply == RT_NULL || reply_size == 0)
        return 0;

    /* bit7：0 请求，1 回复。这里只处理请求。 */
    if (buffer[0] & 0x80)
        return 0;

    net_cmd = buffer[0] & 0x7F;

    switch (net_cmd)
    {
        case 0x01: /* 获取参数。请求：无选择器；回复：按对象类型导出的整份 JSON */
        {
            LOG_I("cmd parse: 0x01 get param obj=%d\r\n", obj_type);
            if (obj_type == CMD_OBJ_UI)
                reply_len = param_get_all_to_ui(PARAM_FILE_PATH, (char *)reply + 1, reply_size - 1);
            else
                reply_len = param_get_all_to_net(PARAM_FILE_PATH, (char *)reply + 1, reply_size - 1);
            if (reply_len == 0)
                LOG_W("cmd parse: 0x01 get param failed obj=%d\r\n", obj_type);
            else
            {
                reply[0] = net_cmd | 0x80;
                reply_len += 1;
            }
            break;
        }
        case 0x02: /* 设置参数。请求：按对象类型的整份 JSON；回复：成功条数 + 失败条数 */
        {
            char *json;
            rt_err_t err;

            LOG_I("cmd parse: 0x02 set param obj=%d\r\n", obj_type);
            if (len < 2)
            {
                LOG_W("protocol net_cmd parse: 0x02 frame len: %d\r\n", (int)len);
                break;
            }

            json = rt_malloc(len);
            if (json == RT_NULL)
                break;

            rt_memcpy(json, buffer + 1, len - 1);
            json[len - 1] = '\0';
            if (obj_type == CMD_OBJ_UI)
                err = param_set_all_from_ui(json);
            else
                err = param_set_all_from_net(json);
            rt_free(json);

            reply[0] = net_cmd | 0x80;
            reply[1] = (err == RT_EOK) ? 1 : 0;
            reply[2] = (err == RT_EOK) ? 0 : 1;
            reply_len = 3;
            LOG_I("protocol net_cmd return: 0x02 set param succ=%d fail=%d\r\n",
                  reply[1], reply[2]);
            break;
        }
        case 0x03: /* 获取历史报文 */
        {
            LOG_I("protocol net_cmd parse: 0x03 get history messages\r\n");

            /* 布局: [0]=0x03 [1]=host_id [2]=event_type
             *       [3..6]=start_time(u32 LE) [7..10]=end_time(u32 LE) [11]=count */
            rt_bool_t is_end = RT_FALSE;

            if (len < sizeof(struct event_history) + 1)
            {
                LOG_W("protocol net_cmd parse: 0x03 frame len: %d < sizeof(struct event_history)\r\n", (int)len);
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
                    LOG_I("protocol net_cmd parse: 0x03 invalid args host=%d count=%d \r\n",
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
                reply[0] = net_cmd | 0x80;
                reply_len += 1;
            }

            break;
        }
        case 0x04: /* 删除绑定。请求：host_id；回复空 */
        case 0x05: /* 创建绑定。请求：host_id；回复空 */
        {
            rt_uint8_t host_id;

            LOG_I("protocol net_cmd parse: 0x%02X %s bind\r\n", net_cmd, net_cmd == 0x04 ? "delete" : "create");
            if (len < 2)
            {
                LOG_W("protocol net_cmd parse: 0x%02X frame len: %d < 2\r\n", net_cmd, (int)len);
                break;
            }

            host_id = buffer[1];
            if (host_id >= HOST_MAX
                || g_host[host_id] == RT_NULL || g_host[host_id]->ops == RT_NULL)
            {
                LOG_I("protocol net_cmd parse: 0x%02X invalid host=%d\r\n", net_cmd, host_id);
                break;
            }

            host = g_host[host_id];
            if (net_cmd == 0x04)
            {
                if (host->ops->net_rx_bind_delete == RT_NULL)
                    break;
                host->ops->net_rx_bind_delete(host);
            }
            else
            {
                if (host->ops->net_rx_bind_create == RT_NULL)
                    break;
                host->ops->net_rx_bind_create(host);
            }
            reply[0] = net_cmd | 0x80;
            reply_len = 1;
            break;
        }
        case 0x06: /* 获取绑定状态。请求：host_id；回复：0 未绑定 / 1 已绑定 */
        {
            LOG_I("protocol net_cmd parse: 0x06 get bind\r\n");

            if (len < 2)
            {
                LOG_W("protocol net_cmd parse: 0x06 frame len: %d < 2\r\n", (int)len);
                break;
            }

            reply[0] = net_cmd | 0x80;

            rt_uint8_t host_id = buffer[1];
            if (host_id >= HOST_MAX
                || g_host[host_id] == RT_NULL || g_host[host_id]->ops == RT_NULL
                || g_host[host_id]->ops->net_rx_bind_status == RT_NULL)
            {
                LOG_I("protocol net_cmd parse: 0x06 invalid args host=%d\r\n",
                        host_id);
                reply[1] = 0;
            }
            else
                reply[1] = g_host[host_id]->ops->net_rx_bind_status(g_host[host_id]) ? 1 : 0;

            reply_len = 2;
            LOG_I("protocol net_cmd return: 0x06 get bind: %d\r\n", reply[1]);

            break;
        }
        case 0x07: /* 切换控制台。请求：usb|uart|net；回复空 */
        {
            char name[16];

            LOG_I("protocol net_cmd parse: 0x07 switch console\r\n");
            if (len < 2 || len - 1 >= sizeof(name))
            {
                LOG_W("protocol net_cmd parse: 0x07 frame len: %d\r\n", (int)len);
                break;
            }

            rt_memcpy(name, buffer + 1, len - 1);
            name[len - 1] = '\0';
            if (rt_strcmp(name, "uart") == 0)
                console_device_set(CONSOLE_DEVICE_UART);
            else if (rt_strcmp(name, "usb") == 0)
                console_device_set(CONSOLE_DEVICE_USB_CDC);
            else if (rt_strcmp(name, "net") == 0)
                console_device_set(CONSOLE_DEVICE_NET);
            else
            {
                LOG_W("protocol net_cmd parse: 0x07 invalid console: %s\r\n", name);
                break;
            }

            reply[0] = net_cmd | 0x80;
            reply_len = 1;
            break;
        }
        default:
        {
            LOG_E("protocol net_cmd parse: unknown command: 0x%02X\r\n", buffer[0]);
            break;
        }
    }

    return reply_len;
}
