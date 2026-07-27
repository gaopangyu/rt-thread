#include "protocol_cmd.h"
#include "bind.h"

#define DBG_TAG "protocol_cmd"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static void protocol_cmd_recv_sub_event(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    protocol_cmd_obj_t obj = (protocol_cmd_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;

    if(custom_obj->rxb.sub_mb == RT_NULL)
        return;

    while (1)
    {
        while(custom_obj->rxb.sub_event == RT_NULL || custom_obj->rxb.pre_event_mask == 0)
            rt_thread_mdelay(100);

        rt_uint32_t recv_event_flag = 0;
        if(rt_event_recv(custom_obj->rxb.sub_event, custom_obj->rxb.pre_event_mask, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR , RT_WAITING_FOREVER, &recv_event_flag) != RT_EOK)
        {
            if (custom_obj->rxb.sub_event == RT_NULL || custom_obj->rxb.pre_event_mask == 0 || recv_event_flag == 0)
                continue;
            LOG_E("%s recv tx_sub_event error: %d/%d\r\n", rt_obj->name, recv_event_flag, custom_obj->rxb.pre_event_mask);
            break;
        }

        if (rt_mb_send(custom_obj->rxb.sub_mb, recv_event_flag) != RT_EOK)
        {
            LOG_E("%s send tx_sub_mb error: %d\r\n", rt_obj->name, recv_event_flag);
            break;
        }
    }
}

static void protocol_cmd_send_sub_event(void *parameter)
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

static double_ret protocol_cmd_set_param(protocol_obj_t self, void *config){
    double_ret result = {0, 0};

    return result;
}

static rt_err_t protocol_cmd_handle_bind_next( rt_object_t rt_obj,
                                                 custom_obj_t custom_obj,
                                                //  EventEntry* eventi,
                                                 rt_uint8_t *buffer,
                                                 rt_size_t len)
{
    if (custom_obj->rxb.pub_event == RT_NULL || custom_obj->event_flag == 0 
    || custom_obj->rxb.pub_succ_event == RT_NULL || custom_obj->rxb.next_event_mask == 0 )
        return -RT_EINVAL;

    custom_obj->rxb.buffer = buffer;
    custom_obj->rxb.len = len;

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

    return RT_EOK;
}

static Event * protocol_cmd_handle_bind_pre(protocol_cmd_obj_t obj, rt_object_t rt_obj, rt_uint8_t *buffer, rt_size_t len)
{
    if (obj->lib_process_data == RT_NULL || obj->lib_destroy_event == RT_NULL || buffer == RT_NULL || len == 0)
        return RT_NULL;

	const uint8_t timestamp[] = {0, 0, 0, 1, 1, 20};
	Event * event = obj->lib_process_data(buffer, len, timestamp, sizeof(timestamp)/sizeof(uint8_t));
	if (NULL == event)
    {
        LOG_D("%s process data return 0, use lib: %s\r\n", rt_obj->name, obj->config.lib_name);
        return RT_NULL;
    }

    return event;
}

static void protocol_cmd_recv(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    protocol_cmd_obj_t obj = (protocol_cmd_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    protocol_obj_t protocol_obj = (protocol_obj_t)obj;

    if(custom_obj->rxb.sub_mb == RT_NULL)
        return;

    while (1)
    {
        rt_uint32_t recv_event_flag = 0;
        if(rt_mb_recv(custom_obj->rxb.sub_mb, (rt_ubase_t *)&recv_event_flag, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s recv rxb.sub_mb error", rt_obj->name);
            break;
        }

        rt_thread_yield();

        if (rt_mutex_take(custom_obj->rxb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s take rx_bind_mutex error", rt_obj->name);
            break;
        }

        if(custom_obj->event_flag == 0)
        {
            LOG_E("%s event_flag is 0, skip", rt_obj->name);
            rt_mutex_release(custom_obj->rxb.bind_mutex);
            continue;
        }

        while(recv_event_flag != 0)
        {
            custom_obj_t rx_custom_obj = get_obj_from_event_flag(custom_obj, custom_obj->rxb.pre_obj_list, &recv_event_flag, RT_TRUE);
            if(rx_custom_obj == RT_NULL)
                break;

            LOG_D("%s recv from %s: %d\r\n", rt_obj->name, rx_custom_obj->parent.name, rx_custom_obj->rxb.len);
            custom_obj_bind_link_xfer(rx_custom_obj, custom_obj, rx_custom_obj->rxb.len, RT_TRUE);

            if(rx_custom_obj->rxb.pub_succ_event == RT_NULL)
                continue;

            // Event *event = protocol_cmd_handle_bind_pre(obj, rt_obj, rx_custom_obj->rxb.buffer, rx_custom_obj->rxb.len);

            // if (event != RT_NULL && obj->lib_destroy_event != RT_NULL)
            // {
            //     for (int i=0; i< event->event_size; i++) 
            //         protocol_cmd_handle_bind_next(rt_obj, custom_obj, &(event->events[i]));
            //     obj->lib_destroy_event(event);
            // }

            protocol_cmd_handle_bind_next(rt_obj, custom_obj, rx_custom_obj->rxb.buffer, rx_custom_obj->rxb.len);

            LOG_I("%s ---> %s send rxb.pub_succ_event: 0x%X\r\n", rt_obj->name, rx_custom_obj->parent.name, custom_obj->event_flag);
            if( rt_event_send(rx_custom_obj->rxb.pub_succ_event, custom_obj->event_flag) != RT_EOK )
                LOG_E("%s send rx_pub_succ_event error: %d/%d\r\n", rt_obj->name, custom_obj->event_flag, rx_custom_obj->rxb.next_event_mask);
        }

        rt_mutex_release(custom_obj->rxb.bind_mutex);
    }
}

static void protocol_cmd_send(void *parameter)
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

            if (custom_obj->txb.pub_event != RT_NULL
             && custom_obj->txb.pub_succ_event != RT_NULL && custom_obj->txb.pre_event_mask != 0)
            {
                custom_obj->txb.buffer = tx_custom_obj->txb.buffer;
                custom_obj->txb.len = tx_custom_obj->txb.len;
                custom_obj_bind_link_xfer(tx_custom_obj, custom_obj, tx_custom_obj->txb.len, RT_FALSE);

                LOG_D("%s send to %s: %d\r\n", tx_custom_obj->parent.name, rt_obj->name, tx_custom_obj->txb.len);

                rt_thread_yield();
                LOG_I("%s send txb.pub_event: 0x%X\r\n", rt_obj->name, custom_obj->event_flag);
                if (rt_event_send(custom_obj->txb.pub_event, custom_obj->event_flag) == RT_EOK)
                {
                    rt_uint32_t succ_event_flag = 0;
                    LOG_I("%s recv txb.pub_succ_event: 0x%X\r\n", rt_obj->name, custom_obj->txb.pre_event_mask);
                    if (rt_event_recv(custom_obj->txb.pub_succ_event, custom_obj->txb.pre_event_mask, RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &succ_event_flag) != RT_EOK)
                        LOG_E("%s recv tx_pub_succ_event error: %d/%d\r\n", rt_obj->name, succ_event_flag, custom_obj->txb.pre_event_mask);
                }
                else
                    LOG_E("%s send tx_pub_event error: %d/%d\r\n", rt_obj->name, custom_obj->txb.pub_event, custom_obj->txb.pre_event_mask);

                custom_obj->txb.buffer = RT_NULL;
                custom_obj->txb.len = 0;
            }

            
            LOG_I("%s ---> %s send txb.pub_succ_event: 0x%X\r\n", rt_obj->name, tx_custom_obj->parent.name, custom_obj->event_flag);
            if( rt_event_send(tx_custom_obj->txb.pub_succ_event, custom_obj->event_flag) != RT_EOK )
                    LOG_E("%s send tx_pub_succ_event error: %d/%d\r\n", rt_obj->name, custom_obj->event_flag, tx_custom_obj->txb.pre_event_mask);
        }

        rt_mutex_release(custom_obj->txb.bind_mutex); 
    }
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

    obj->rx_tid = rt_thread_create("protocol_cmd_rx", protocol_cmd_recv, obj, 1024, CUSTOM_THREAD_PRIORITY, 20);
    if (obj->rx_tid == RT_NULL) {
        LOG_E("create error: rx thread create");
        goto exit;
    }

    obj->tx_tid = rt_thread_create("protocol_cmd_tx", protocol_cmd_send, obj, 1024, CUSTOM_THREAD_PRIORITY, 20);
    if (obj->tx_tid == RT_NULL) {
        LOG_E("create error: tx thread create");
        goto exit;
    }

    if((obj->rx_sub_event_tid = rt_thread_create("protocol_cmd_rx_sub_event", protocol_cmd_recv_sub_event, obj, 1024, CUSTOM_THREAD_PRIORITY - 1, 20)) == RT_NULL) {
        LOG_E("create error: rx sub event thread create");
        goto exit;
    }

    if((obj->tx_sub_event_tid = rt_thread_create("protocol_cmd_tx_sub_event", protocol_cmd_send_sub_event, obj, 1024, CUSTOM_THREAD_PRIORITY - 1, 20)) == RT_NULL) {
        LOG_E("create error: tx sub event thread create");
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

    rt_thread_startup(obj->rx_tid);
    rt_thread_startup(obj->tx_tid);
    rt_thread_startup(obj->rx_sub_event_tid);
    rt_thread_startup(obj->tx_sub_event_tid);

    rt_object_init(rt_obj, RT_Object_Class_Custom, PROTOCOL_CMD_NAME);
    return RT_EOK;

exit:
    if(rt_object_find(rt_obj->name, RT_Object_Class_Custom) != RT_NULL)
        rt_object_detach(rt_obj);

    bind_delete_all_for_obj(custom_obj);
    
    if (obj->rx_sub_event_tid != RT_NULL)
    {
        rt_thread_delete(obj->rx_sub_event_tid);
        obj->rx_sub_event_tid = RT_NULL;
    }

    if (obj->tx_sub_event_tid != RT_NULL)
    {
        rt_thread_delete(obj->tx_sub_event_tid);
        obj->tx_sub_event_tid = RT_NULL;
    }

    if (obj->rx_tid != RT_NULL)
    {
        rt_thread_delete(obj->rx_tid);
        obj->rx_tid = RT_NULL;
    }

    if (obj->tx_tid != RT_NULL)
    {
        rt_thread_delete(obj->tx_tid);
        obj->tx_tid = RT_NULL;
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

    return -RT_ERROR;
}

INIT_APP_EXPORT(protocol_cmd_init);