#include "protocol_cmd.h"
#include "bind.h"
#include "upload_muart.h"
#include "host.h"
#include "param.h"
#include "console.h"

#include <time.h>
#include <string.h>

#define DBG_TAG "protocol_cmd"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define CMD_BUFFER_SIZE 8192
static rt_uint8_t cmd_recv_buffer[CMD_BUFFER_SIZE];
static rt_size_t cmd_recv_len = 0;
static rt_uint8_t cmd_reply_buffer[CMD_BUFFER_SIZE];
static rt_size_t cmd_reply_len = 0;

static double_ret protocol_cmd_set_param(protocol_obj_t self, void *config)
{
    double_ret result = {0, 0};
    return result;
}

static rt_err_t protocol_cmd_delete(protocol_obj_t self)
{
    return -RT_ENOSYS;
}

static protocol_obj_t  protocol_cmd_create(protocol_obj_t self, char *name, void *config)
{
    return RT_NULL;
}

static const struct protocol_obj_ops protocol_cmd_ops =
{
    .create = protocol_cmd_create,
    .delete = protocol_cmd_delete,
    .set_param = protocol_cmd_set_param,
};

static rt_err_t  protocol_cmd_gc(custom_obj_t self)
{
    return -RT_ENOSYS;
}

static const struct custom_object_ops custom_obj_ops =
{
    .gc = protocol_cmd_gc,
};

static void protocol_cmd_recv_sub_event(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    protocol_cmd_obj_t obj = (protocol_cmd_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;

    if(custom_obj->txb.sub_mb == RT_NULL)
        return;

    while (1)
    {
        while(custom_obj->txb.sub_event == RT_NULL || custom_obj->txb.next_event_mask == 0)
            rt_thread_mdelay(100);

        rt_uint32_t recv_event_flag = 0;
        if(rt_event_recv(custom_obj->txb.sub_event, custom_obj->txb.next_event_mask, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR , RT_WAITING_FOREVER, &recv_event_flag) != RT_EOK)
        {
            if (custom_obj->txb.sub_event == RT_NULL || custom_obj->txb.next_event_mask == 0 || recv_event_flag == 0)
                continue;
            LOG_E("%s recv tx_sub_event error: %d/%d\r\n", rt_obj->name, recv_event_flag, custom_obj->txb.next_event_mask);
            break;
        }

        if (rt_mb_send(custom_obj->txb.sub_mb, recv_event_flag) != RT_EOK)
        {
            LOG_E("%s send tx_sub_mb error: %d\r\n", rt_obj->name, recv_event_flag);
            break;
        }
    }
}

static void protocol_cmd_recv(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    protocol_cmd_obj_t obj = (protocol_cmd_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;

    if (custom_obj->txb.sub_mb == RT_NULL)
        return;

    while (1)
    {
        rt_uint32_t recv_event_flag = 0;
        if(rt_mb_recv(custom_obj->txb.sub_mb, (rt_ubase_t *)&recv_event_flag, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s recv tx_sub_mb error", rt_obj->name);
            break;
        }

        rt_thread_yield();

        if (rt_mutex_take(custom_obj->txb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s take tx_bind_mutex error", rt_obj->name);
            break;
        }

        if (custom_obj->event_flag == 0)
        {
            LOG_E("%s event_flag is 0, skip", rt_obj->name);
            rt_mutex_release(custom_obj->txb.bind_mutex);
            continue;
        }

        while(recv_event_flag != 0)
        {
            custom_obj_t tx_custom_obj = get_obj_from_event_flag(custom_obj, custom_obj->txb.next_obj_list, &recv_event_flag, RT_TRUE);
            if(tx_custom_obj == RT_NULL)
                break;

            if(tx_custom_obj->txb.pub_succ_event == RT_NULL)
                continue;

            LOG_D("protocol cmd recv len: %d\r\n", tx_custom_obj->txb.len);

            if( tx_custom_obj->txb.len > CMD_BUFFER_SIZE )
            {
                LOG_E("protocol cmd recv len: %d > CMD_BUFFER_SIZE: %d\r\n", tx_custom_obj->txb.len, CMD_BUFFER_SIZE);
                cmd_recv_len = CMD_BUFFER_SIZE;
            }
            else
                cmd_recv_len = tx_custom_obj->txb.len;

            rt_memcpy(cmd_recv_buffer, tx_custom_obj->txb.buffer, cmd_recv_len);
            LOG_HEX("rx", 16, cmd_recv_buffer, cmd_recv_len);

            rt_sem_release(obj->recv_sem);

            custom_obj_bind_link_xfer(tx_custom_obj, custom_obj, tx_custom_obj->txb.len, RT_FALSE);

            LOG_I("%s ---> %s send txb.pub_succ_event: 0x%X\r\n", rt_obj->name, tx_custom_obj->parent.name, custom_obj->event_flag);
            if( rt_event_send(tx_custom_obj->txb.pub_succ_event, custom_obj->event_flag) != RT_EOK )
                    LOG_E("%s send tx_pub_succ_event error: %d/%d\r\n", rt_obj->name, custom_obj->event_flag, tx_custom_obj->txb.pre_event_mask);
        }

        rt_mutex_release(custom_obj->txb.bind_mutex);
    }
}

static rt_size_t parse(rt_uint8_t *buffer, rt_size_t len)
{
    rt_uint8_t cmd;
    rt_size_t reply_len = 0;
    host_t host;

    if (buffer == RT_NULL || len == 0)
        return 0;

    /* bit7：0 请求，1 回复。这里只处理请求。 */
    if (buffer[0] & 0x80)
        return 0;

    cmd = buffer[0] & 0x7F;

    switch (cmd)
    {
        case 0x01: /* 获取参数。请求：无选择器；回复：整份合宙 JSON */
        {
            LOG_I("protocol cmd parse: 0x01 get param\r\n");
            reply_len = param_get_all_to_net(PARAM_FILE_PATH, (char *)cmd_reply_buffer + 1, sizeof(cmd_reply_buffer) - 1);
            if (reply_len == 0)
                LOG_W("protocol cmd parse: 0x01 get net param failed\r\n");
            else
            {
                cmd_reply_buffer[0] = cmd | 0x80;
                reply_len += 1;
            }
            break;
        }
        case 0x02: /* 设置参数。请求：整份合宙 JSON；回复：成功条数 + 失败条数 */
        {
            char *json;
            rt_err_t err;

            LOG_I("protocol cmd parse: 0x02 set param\r\n");
            if (len < 2)
            {
                LOG_W("protocol cmd parse: 0x02 frame len: %d\r\n", (int)len);
                break;
            }

            json = rt_malloc(len);
            if (json == RT_NULL)
                break;

            rt_memcpy(json, buffer + 1, len - 1);
            json[len - 1] = '\0';
            err = param_set_all_from_net(json);
            rt_free(json);

            cmd_reply_buffer[0] = cmd | 0x80;
            cmd_reply_buffer[1] = (err == RT_EOK) ? 1 : 0;
            cmd_reply_buffer[2] = (err == RT_EOK) ? 0 : 1;
            reply_len = 3;
            LOG_I("protocol cmd return: 0x02 set param succ=%d fail=%d\r\n",
                  cmd_reply_buffer[1], cmd_reply_buffer[2]);
            break;
        }
        case 0x03: /* 获取历史报文 */
        {
            LOG_I("protocol cmd parse: 0x03 get history messages\r\n");

            /* 布局: [0]=0x03 [1]=host_id [2]=event_type
             *       [3..6]=start_time(u32 LE) [7..10]=end_time(u32 LE) [11]=count */
            if (len < sizeof(struct event_history) + 1)
            {
                LOG_W("protocol cmd parse: 0x03 frame len: %d < sizeof(struct event_history)\r\n", (int)len);
                break;
            }

            struct event_history event_history;
            rt_memcpy(&event_history, buffer + 1, sizeof(struct event_history));

            if (event_history.host_id >= HOST_MAX || event_history.count == 0
                || g_host[event_history.host_id] == RT_NULL || g_host[event_history.host_id]->ops == RT_NULL
                || g_host[event_history.host_id]->ops->get_history == RT_NULL)
            {
                LOG_I("protocol cmd parse: 0x03 invalid args host=%d count=%d \r\n",
                                               event_history.host_id, event_history.count);
                break;
            }

            rt_bool_t is_end = RT_FALSE;
            host = g_host[event_history.host_id];

            reply_len = host->ops->get_history(host,
                                                cmd_reply_buffer + 1,
                                                sizeof(cmd_reply_buffer) - 1,
                                                &event_history,
                                                &is_end);
            if (reply_len > 0)
            {
                cmd_reply_buffer[0] = cmd | 0x80;
                reply_len += 1;
            }
            if (is_end && host->ops->muart_rx_bind_create != RT_NULL)
                host->ops->muart_rx_bind_create(host);

            break;
        }
        case 0x04: /* 删除绑定。请求：host_id；回复空 */
        case 0x05: /* 创建绑定。请求：host_id；回复空 */
        {
            rt_uint8_t host_id;

            LOG_I("protocol cmd parse: 0x%02X %s bind\r\n", cmd, cmd == 0x04 ? "delete" : "create");
            if (len < 2)
            {
                LOG_W("protocol cmd parse: 0x%02X frame len: %d < 2\r\n", cmd, (int)len);
                break;
            }

            host_id = buffer[1];
            if (host_id >= HOST_MAX
                || g_host[host_id] == RT_NULL || g_host[host_id]->ops == RT_NULL)
            {
                LOG_I("protocol cmd parse: 0x%02X invalid host=%d\r\n", cmd, host_id);
                break;
            }

            host = g_host[host_id];
            if (cmd == 0x04)
            {
                if (host->ops->muart_rx_bind_delete == RT_NULL)
                    break;
                host->ops->muart_rx_bind_delete(host);
            }
            else
            {
                if (host->ops->muart_rx_bind_create == RT_NULL)
                    break;
                host->ops->muart_rx_bind_create(host);
            }
            cmd_reply_buffer[0] = cmd | 0x80;
            reply_len = 1;
            break;
        }
        case 0x06: /* 获取绑定状态。请求：host_id；回复：0 未绑定 / 1 已绑定 */
        {
            LOG_I("protocol cmd parse: 0x06 get bind\r\n");

            if (len < 2)
            {
                LOG_W("protocol cmd parse: 0x06 frame len: %d < 2\r\n", (int)len);
                break;
            }

            cmd_reply_buffer[0] = cmd | 0x80;

            rt_uint8_t host_id = buffer[1];
            if (host_id >= HOST_MAX
                || g_host[host_id] == RT_NULL || g_host[host_id]->ops == RT_NULL
                || g_host[host_id]->ops->muart_rx_bind_status == RT_NULL)
            {
                LOG_I("protocol cmd parse: 0x06 invalid args host=%d\r\n",
                        host_id);
                cmd_reply_buffer[1] = 0;
            }
            else
                cmd_reply_buffer[1] = g_host[host_id]->ops->muart_rx_bind_status(g_host[host_id]) ? 1 : 0;

            reply_len = 2;
            LOG_I("protocol cmd return: 0x06 get bind: %d\r\n", cmd_reply_buffer[1]);

            break;
        }
        case 0x07: /* 切换控制台。请求：usb|uart|net；回复空 */
        {
            char name[16];

            LOG_I("protocol cmd parse: 0x07 switch console\r\n");
            if (len < 2 || len - 1 >= sizeof(name))
            {
                LOG_W("protocol cmd parse: 0x07 frame len: %d\r\n", (int)len);
                break;
            }

            rt_memcpy(name, buffer + 1, len - 1);
            name[len - 1] = '\0';
            if (rt_strcmp(name, "uart") == 0)
                set_console_device(CONSOLE_DEVICE_UART);
            else if (rt_strcmp(name, "usb") == 0)
                set_console_device(CONSOLE_DEVICE_USB_CDC);
            else if (rt_strcmp(name, "net") == 0)
                set_console_device(CONSOLE_DEVICE_NET);
            else
            {
                LOG_W("protocol cmd parse: 0x07 invalid console: %s\r\n", name);
                break;
            }

            cmd_reply_buffer[0] = cmd | 0x80;
            reply_len = 1;
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

static void protocol_cmd_parse(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    protocol_cmd_obj_t obj = (protocol_cmd_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    protocol_obj_t protocol_obj = (protocol_obj_t)obj;

    if(obj->recv_sem == RT_NULL)
        return;

    while (1)
    {
        if(rt_sem_take(obj->recv_sem, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s wait recv_sem error", rt_obj->name);
            break;
        }

        cmd_reply_len = parse(cmd_recv_buffer, cmd_recv_len);
        if (cmd_reply_len == 0)
        {
            rt_kprintf("I/protocol_cmd tx none\n");
            continue;
        }
        LOG_HEX("tx", 16, cmd_reply_buffer, cmd_reply_len);

        //回复
        if (rt_mutex_take(custom_obj->rxb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s take rx_bind_mutex error", rt_obj->name);
            break;
        }

        if (custom_obj->rxb.pub_event != RT_NULL && custom_obj->event_flag != 0 
         && custom_obj->rxb.pub_succ_event != RT_NULL && custom_obj->rxb.next_event_mask != 0 )
        {
            custom_obj->rxb.buffer = cmd_reply_buffer;
            custom_obj->rxb.len = cmd_reply_len;

            rt_thread_yield();
            LOG_I("%s send rxb.pub_event: 0x%X\r\n", rt_obj->name, custom_obj->event_flag);
            if(rt_event_send(custom_obj->rxb.pub_event, custom_obj->event_flag) == RT_EOK)
            {
                rt_uint32_t recv_event_flag = 0;
                LOG_I("%s recv rxb.pub_succ_event: 0x%X\r\n", rt_obj->name, custom_obj->rxb.next_event_mask);
                if(rt_event_recv(custom_obj->rxb.pub_succ_event, custom_obj->rxb.next_event_mask, RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR , RT_WAITING_FOREVER, &recv_event_flag) != RT_EOK)
                    LOG_E("%s recv pub_succ_event error: %d/%d\r\n", rt_obj->name, recv_event_flag, custom_obj->rxb.next_event_mask);
            }
            else
            {
                LOG_E("%s send rx_pub_event error: %d/%d\r\n", rt_obj->name, custom_obj->rxb.pub_event, custom_obj->rxb.next_event_mask);
            }
            
            custom_obj->rxb.buffer = RT_NULL;
            custom_obj->rxb.len = 0;
        }

        rt_mutex_release(custom_obj->rxb.bind_mutex);
    }
}

static int protocol_cmd_init(void)
{
    static struct protocol_cmd_obj object = {0};
    protocol_cmd_obj_t obj = &object;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    protocol_obj_t protocol_obj = (protocol_obj_t)obj;

    if (rt_object_find(PROTOCOL_CMD_NAME, RT_Object_Class_Custom) != RT_NULL)
        return -RT_ERROR;

    custom_obj->type = Custom_Object_Class_Protocol;

    protocol_obj_init(custom_obj);

    custom_obj->ops = &custom_obj_ops;
    protocol_obj->ops = &protocol_cmd_ops;

    if((obj->recv_sem = rt_sem_create("protocol_cmd_recv_sem", 0, RT_IPC_FLAG_FIFO)) == RT_NULL) {
        LOG_E("create error: recv semaphore create");
        goto exit;
    }

    obj->parse_tid = rt_thread_create("protocol_cmd_parse", protocol_cmd_parse, obj, 6144, CUSTOM_THREAD_PRIORITY, 20);
    if (obj->parse_tid == RT_NULL) {
        LOG_E("create error: parse thread create");
        goto exit;
    }

    obj->recv_tid = rt_thread_create("protocol_cmd_recv", protocol_cmd_recv, obj, 1024, CUSTOM_THREAD_PRIORITY, 20);
    if (obj->recv_tid == RT_NULL) {
        LOG_E("create error: recv thread create");
        goto exit;
    }

    if((obj->recv_sub_event_tid = rt_thread_create("protocol_cmd_recv_sub_event", protocol_cmd_recv_sub_event, obj, 1024, CUSTOM_THREAD_PRIORITY - 1, 20)) == RT_NULL) {
        LOG_E("create error: recv sub event thread create");
        goto exit;
    }

    if(((custom_obj->rxb.bind_mutex = rt_mutex_create("protocol_cmd_rx_bind_mutex", RT_IPC_FLAG_FIFO)) == RT_NULL)) {
        LOG_E("create error: rx mutex create");
        goto exit;
    }

    if(((custom_obj->txb.bind_mutex = rt_mutex_create("protocol_cmd_tx_bind_mutex", RT_IPC_FLAG_FIFO)) == RT_NULL)) {
        LOG_E("create error: tx mutex create");
        goto exit;
    }

    if ((custom_obj->rxb.sub_mb = rt_mb_create("protocol_cmd_rx_sub_mb", CUSTOM_PER_LAYER_OBJ_MAX, RT_IPC_FLAG_FIFO)) == RT_NULL) {
        LOG_E("create error: rx mailbox create");
        goto exit;
    }

    if ((custom_obj->txb.sub_mb = rt_mb_create("protocol_cmd_tx_sub_mb", CUSTOM_PER_LAYER_OBJ_MAX, RT_IPC_FLAG_FIFO)) == RT_NULL) {
        LOG_E("create error: tx mailbox create");
        goto exit;
    }

    if((custom_obj->rxb.pub_succ_event = rt_event_create("protocol_cmd_rx_pub_succ", RT_IPC_FLAG_PRIO)) == RT_NULL) {
        LOG_E("create error: rx pub succ event create");
        goto exit;
    }

    if((custom_obj->txb.pub_succ_event = rt_event_create("protocol_cmd_tx_pub_succ", RT_IPC_FLAG_PRIO)) == RT_NULL) {
        LOG_E("create error: tx pub succ event create");
        goto exit;
    }

    rt_thread_startup(obj->parse_tid);
    rt_thread_startup(obj->recv_tid);
    rt_thread_startup(obj->recv_sub_event_tid);

    rt_object_init(rt_obj, RT_Object_Class_Custom, PROTOCOL_CMD_NAME);

    upload_obj_t muart_parent = (upload_obj_t)rt_object_find(UPLOAD_MUART_PARENT_NAME, RT_Object_Class_Custom);
    if (muart_parent == RT_NULL || muart_parent->ops == RT_NULL || muart_parent->ops->create == RT_NULL)
    {
        LOG_E("create error: parent upload muart not found");
        goto exit;
    }

    struct upload_muart_config config = { .number = 255 };
    upload_obj_t upload_muart_255 = muart_parent->ops->create(muart_parent, "upload_muart_255", &config);
    if (upload_muart_255 == RT_NULL)
    {
        LOG_E("create error: upload muart 255 create failed");
        goto exit;
    }

    if (bind_create(custom_obj, (custom_obj_t)upload_muart_255, RT_TRUE) != RT_EOK)
    {
        LOG_E("create error: rx_bind create failed");
        goto exit;
    }

    if (bind_create(custom_obj, (custom_obj_t)upload_muart_255, RT_FALSE) != RT_EOK)
    {
        LOG_E("create error: tx_bind create failed");
        goto exit;
    }

    return RT_EOK;

exit:
    if(rt_object_find(rt_obj->name, RT_Object_Class_Custom) != RT_NULL)
        rt_object_detach(rt_obj);

    bind_delete_all_for_obj(custom_obj);

    if (obj->recv_sub_event_tid != RT_NULL)
    {
        rt_thread_delete(obj->recv_sub_event_tid);
        obj->recv_sub_event_tid = RT_NULL;
    }

    if (obj->recv_tid != RT_NULL)
    {
        rt_thread_delete(obj->recv_tid);
        obj->recv_tid = RT_NULL;
    }

    if (obj->parse_tid != RT_NULL)
    {
        rt_thread_delete(obj->parse_tid);
        obj->parse_tid = RT_NULL;
    }

    if(custom_obj->rxb.sub_mb != RT_NULL)
    {
        rt_mb_delete(custom_obj->rxb.sub_mb);
        custom_obj->rxb.sub_mb = RT_NULL;
    }

    if(custom_obj->txb.sub_mb != RT_NULL)
    {
        rt_mb_delete(custom_obj->txb.sub_mb);
        custom_obj->txb.sub_mb = RT_NULL;
    }

    if(custom_obj->rxb.bind_mutex != RT_NULL)
    {
        rt_mutex_delete(custom_obj->rxb.bind_mutex);
        custom_obj->rxb.bind_mutex = RT_NULL;
    }

    if(custom_obj->txb.bind_mutex != RT_NULL)
    {
        rt_mutex_delete(custom_obj->txb.bind_mutex);
        custom_obj->txb.bind_mutex = RT_NULL;
    }

    if(custom_obj->rxb.pub_succ_event  != RT_NULL)
    {
        rt_event_delete(custom_obj->rxb.pub_succ_event);
        custom_obj->rxb.pub_succ_event = RT_NULL;
    }

    if(custom_obj->txb.pub_succ_event != RT_NULL)
    {
        rt_event_delete(custom_obj->txb.pub_succ_event);
        custom_obj->txb.pub_succ_event = RT_NULL;
    }

    if(obj->recv_sem != RT_NULL)
    {
        rt_sem_delete(obj->recv_sem);
        obj->recv_sem = RT_NULL;
    }

    return -RT_ERROR;
}

INIT_APP_EXPORT(protocol_cmd_init);