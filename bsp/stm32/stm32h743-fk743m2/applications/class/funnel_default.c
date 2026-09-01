#include <stdlib.h>
#include "funnel_default.h"
#include "bind.h"

#define DBG_TAG "funnel_default"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static void funnel_default_recv_sub_event(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    funnel_default_obj_t obj = (funnel_default_obj_t)parameter;
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

static void funnel_default_send_sub_event(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    funnel_default_obj_t obj = (funnel_default_obj_t)parameter;
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

static double_ret funnel_default_set_param(funnel_obj_t self, void *config){
    double_ret result = {0, 0};

    if (self == RT_NULL || config == RT_NULL) 
    {
        result.fail++;
        return result;
    }

    funnel_default_config_t cfg = (funnel_default_config_t)config;
    funnel_default_obj_t obj = (funnel_default_obj_t)self;
    funnel_default_config_t obj_cfg = &(obj->config);

    obj->config.func_id = cfg->func_id;
    result.succ++;

    obj->event_last_num = cfg->event_last_num;

    return result;
}

static rt_uint32_t get_event_last_num(funnel_default_obj_t obj, enum event_file_type type)
{
    switch (type & 0x0F)
    {
    case FILE_TYPE_RESET:       return obj->event_last_num.reset;
    case FILE_TYPE_FIRE:        return obj->event_last_num.fire;
    case FILE_TYPE_MONITOR:     return obj->event_last_num.monitor;
    case FILE_TYPE_FAULT:       return obj->event_last_num.fault;
    case FILE_TYPE_ACTION:      return obj->event_last_num.action;
    case FILE_TYPE_LINKAGE:     return obj->event_last_num.linkage;
    case FILE_TYPE_MASK:        return obj->event_last_num.mask;
    case FILE_TYPE_OTHER:       return obj->event_last_num.other;
    case FILE_TYPE_CTRL:        return obj->event_last_num.ctrl;
    default:                    return obj->event_last_num.other;
    }
}

static void set_event_last_num(funnel_default_obj_t obj, enum event_file_type type, rt_uint32_t number)
{
    switch (type & 0x0F)
    {
    case FILE_TYPE_RESET:       obj->event_last_num.reset = number;     break;
    case FILE_TYPE_FIRE:        obj->event_last_num.fire = number;      break;
    case FILE_TYPE_MONITOR:     obj->event_last_num.monitor = number;   break;
    case FILE_TYPE_FAULT:       obj->event_last_num.fault = number;     break;
    case FILE_TYPE_ACTION:      obj->event_last_num.action = number;    break;
    case FILE_TYPE_LINKAGE:     obj->event_last_num.linkage = number;   break;
    case FILE_TYPE_MASK:        obj->event_last_num.mask = number;      break;
    case FILE_TYPE_OTHER:       obj->event_last_num.other = number;     break;
    case FILE_TYPE_CTRL:        obj->event_last_num.ctrl = number;      break;
    default:                    obj->event_last_num.other = number;     return;
    }
}

static rt_err_t funnel_default_handle_rx_bind_pre(funnel_default_obj_t obj,
                                                  rt_object_t rt_obj,
                                                  custom_obj_t custom_obj,
                                                  rt_uint8_t *buffer,
                                                  rt_size_t len)
{
    if (custom_obj->rxb.pub_event == RT_NULL || custom_obj->event_flag == 0 
    || custom_obj->rxb.pub_succ_event == RT_NULL || custom_obj->rxb.next_event_mask == 0 
    || buffer == RT_NULL || len == 0)
        return -RT_EINVAL;

    // rt_size_t new_len  = len + 4;
    // rt_uint8_t * new_buffer = rt_malloc(new_len);
    // if (new_buffer == RT_NULL)
    // {
    //     LOG_E("%s malloc %d bytes error", rt_obj->name, new_len);
    //     return -RT_ENOMEM;
    // }

    // enum event_file_type file_type = (enum event_file_type)(((event_pack_t)buffer)->event_type);
    // rt_uint32_t last_num = get_event_last_num(obj, file_type);
    // rt_memcpy(new_buffer, &last_num, 4);
    // rt_memcpy(new_buffer + 4, buffer, len);

    // custom_obj->rxb.buffer = new_buffer;
    // custom_obj->rxb.len = new_len;

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

    // set_event_last_num(obj, file_type, last_num + 1);

    custom_obj->rxb.buffer = RT_NULL;
    custom_obj->rxb.len = 0;

    return RT_EOK;
}

static void funnel_default_recv(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    funnel_default_obj_t obj = (funnel_default_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    funnel_obj_t funnel_obj = (funnel_obj_t)obj;

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

            funnel_default_handle_rx_bind_pre(obj, rt_obj, custom_obj, rx_custom_obj->rxb.buffer, rx_custom_obj->rxb.len);

            LOG_I("%s ---> %s send rxb.pub_succ_event: 0x%X\r\n", rt_obj->name, rx_custom_obj->parent.name, custom_obj->event_flag);
            if( rt_event_send(rx_custom_obj->rxb.pub_succ_event, custom_obj->event_flag) != RT_EOK )
                LOG_E("%s send rx_pub_succ_event error: %d/%d\r\n", rt_obj->name, custom_obj->event_flag, rx_custom_obj->rxb.next_event_mask);
        }

        rt_mutex_release(custom_obj->rxb.bind_mutex);
    }
}

static void funnel_default_send(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    funnel_default_obj_t obj = (funnel_default_obj_t)parameter;
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
 
static rt_err_t funnel_default_delete(funnel_obj_t self)
{
    if (self == RT_NULL) {
        return -RT_EINVAL;
    }

    if (rt_strncmp(self->parent.parent.name, FUNNEL_DEFAULT_PARENT_NAME, RT_NAME_MAX) == 0) {
        return -RT_EINVAL;
    }

    funnel_default_obj_t obj = (funnel_default_obj_t)self;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;

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

    rt_free(obj);
    obj = RT_NULL;

    return RT_EOK;
}

static funnel_obj_t  funnel_default_create(funnel_obj_t self, char *name, void *config)
{
    if (self == RT_NULL || name == RT_NULL) {
        LOG_E("create error: param");
        return RT_NULL;
    }

    if (rt_strncmp(self->parent.parent.name, FUNNEL_DEFAULT_PARENT_NAME, RT_NAME_MAX) != 0) {
        LOG_E("create error: invalid parent");
        return RT_NULL;
    }

    if (rt_object_find(name, RT_Object_Class_Custom) != RT_NULL)
    {
        LOG_E("create error: %s exist", name);
        return RT_NULL;
    }

    funnel_default_obj_t obj = rt_malloc(sizeof(struct funnel_default_obj));
    if (obj == RT_NULL) {
        LOG_E("create error: memory allocation");
        return RT_NULL;
    }

    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    funnel_obj_t funnel_obj = (funnel_obj_t)obj;

    rt_memcpy(obj, self, sizeof(struct funnel_default_obj));
    rt_strncpy(rt_obj->name, name, RT_NAME_MAX - 1);
    rt_obj->name[RT_NAME_MAX - 1] = 0;

    if(config == RT_NULL)
        config = &(obj->config);

    double_ret result = funnel_default_set_param(funnel_obj, config);
    if (result.fail != 0)
    {
        LOG_E("create error: set param");
        goto exit;
    }

    char name_buf[RT_NAME_MAX] = {0};

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx");
    obj->rx_tid = rt_thread_create(name_buf, funnel_default_recv, obj, 2048, CUSTOM_THREAD_PRIORITY, 20);
    if (obj->rx_tid == RT_NULL) {
        LOG_E("create error: rx thread create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_tx");
    obj->tx_tid = rt_thread_create(name_buf, funnel_default_send, obj, 1024, CUSTOM_THREAD_PRIORITY, 20);
    if (obj->tx_tid == RT_NULL) {
        LOG_E("create error: tx thread create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_sub_event");
    if((obj->rx_sub_event_tid = rt_thread_create(name_buf, funnel_default_recv_sub_event, obj, 1024, CUSTOM_THREAD_PRIORITY - 1, 20)) == RT_NULL) {
        LOG_E("create error: rx sub event thread create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_tx_sub_event");
    if((obj->tx_sub_event_tid = rt_thread_create(name_buf, funnel_default_send_sub_event, obj, 1024, CUSTOM_THREAD_PRIORITY - 1, 20)) == RT_NULL) {
        LOG_E("create error: tx sub event thread create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_bind_mutex");
    if(((custom_obj->rxb.bind_mutex = rt_mutex_create(name_buf, RT_IPC_FLAG_FIFO)) == RT_NULL)) {
        LOG_E("create error: rx mutex create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_tx_bind_mutex");
    if(((custom_obj->txb.bind_mutex = rt_mutex_create(name_buf, RT_IPC_FLAG_FIFO)) == RT_NULL)) {
        LOG_E("create error: tx mutex create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_sub_mb");
    if ((custom_obj->rxb.sub_mb = rt_mb_create(name_buf, CUSTOM_PER_LAYER_OBJ_MAX, RT_IPC_FLAG_FIFO)) == RT_NULL) {
        LOG_E("create error: rx mailbox create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_tx_sub_mb");
    if ((custom_obj->txb.sub_mb = rt_mb_create(name_buf, CUSTOM_PER_LAYER_OBJ_MAX, RT_IPC_FLAG_FIFO)) == RT_NULL) {
        LOG_E("create error: tx mailbox create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_pub_succ");
    if((custom_obj->rxb.pub_succ_event = rt_event_create(name_buf, RT_IPC_FLAG_PRIO)) == RT_NULL) {
        LOG_E("create error: rx pub succ event create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_tx_pub_succ");
    if((custom_obj->txb.pub_succ_event = rt_event_create(name_buf, RT_IPC_FLAG_PRIO)) == RT_NULL) {
        LOG_E("create error: tx pub succ event create");
        goto exit;
    }

    rt_thread_startup(obj->rx_tid);
    rt_thread_startup(obj->tx_tid);
    rt_thread_startup(obj->rx_sub_event_tid);
    rt_thread_startup(obj->tx_sub_event_tid);

    rt_object_init(rt_obj, RT_Object_Class_Custom, name);
    LOG_I("create success: %s", name);
    return funnel_obj;

exit:
    funnel_default_delete(funnel_obj);
    return RT_NULL;
}

static const struct funnel_obj_ops funnel_default_ops =
{
    .create = funnel_default_create,
    .delete = funnel_default_delete,
    .set_param = funnel_default_set_param,
};

static rt_err_t  funnel_default_gc(custom_obj_t self)
{
    if (self == RT_NULL)
        return -RT_EINVAL;

    funnel_obj_t funnel_obj = (funnel_obj_t)self;
    return funnel_default_delete(funnel_obj);
}

static const struct custom_object_ops custom_obj_ops =
{
    .gc = funnel_default_gc,
};

static int funnel_default_init(void)
{
    static struct funnel_default_obj object = {0};
    funnel_default_obj_t obj = &object;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    funnel_obj_t funnel_obj = (funnel_obj_t)obj;

    if (rt_object_find(FUNNEL_DEFAULT_PARENT_NAME, RT_Object_Class_Custom) != RT_NULL)
        return -RT_ERROR;

    custom_obj->type = Custom_Object_Class_Filter;

    funnel_obj_init(custom_obj);

    custom_obj->ops = &custom_obj_ops;
    funnel_obj->ops = &funnel_default_ops;

    obj->config.func_id = 0;

    rt_object_init(rt_obj, RT_Object_Class_Custom, FUNNEL_DEFAULT_PARENT_NAME);

    return RT_EOK;
}

INIT_ENV_EXPORT(funnel_default_init);

static int cmd_create_funnel_default(int argc, char *argv[])
{
    if (argc < 2)
    {
        rt_kprintf("Usage: create_funnel_default <name> [-l lib_name]\n");
        rt_kprintf("Example: create_funnel_default funnel_default_1 -l 208\n");
        return -RT_ERROR;
    }

    const char *name = argv[1];
    rt_err_t err = custom_obj_msh_check_create_name(name, "funnel_default");

    if (err == -RT_EFULL)
    {
        rt_kprintf("Object already exists: %s\n", name);
        return -RT_ERROR;
    }
    if (err != RT_EOK)
    {
        rt_kprintf("Invalid name: %s\n", name);
        return -RT_ERROR;
    }

    funnel_obj_t parent = (funnel_obj_t)rt_object_find(FUNNEL_DEFAULT_PARENT_NAME, RT_Object_Class_Custom);
    if (parent == RT_NULL || parent->ops == RT_NULL || parent->ops->create == RT_NULL)
    {
        rt_kprintf("Parent not ready: %s\n", FUNNEL_DEFAULT_PARENT_NAME);
        return -RT_ERROR;
    }

    struct funnel_default_config *config_ptr = RT_NULL;
    struct funnel_default_config config = {0};
    const char *func_id_str = custom_obj_msh_get_opt(argc, argv, "f");

    if (func_id_str != RT_NULL)
    {
        config.func_id = (rt_uint32_t)strtoul(func_id_str, RT_NULL, 10);
        config_ptr = &config;
    }

    if (parent->ops->create(parent, (char *)name, config_ptr) == RT_NULL)
    {
        rt_kprintf("Failed to create funnel_default: %s\n", name);
        return -RT_ERROR;
    }

    rt_kprintf("Created funnel_default: %s\n", name);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_create_funnel_default, create_funnel_default, Create funnel_default object);

static int cmd_delete_funnel_default(int argc, char *argv[])
{
    const char *name = custom_obj_msh_get_opt(argc, argv, "n");

    if (name == RT_NULL)
    {
        rt_kprintf("Usage: delete_funnel_default -n <name>\n");
        rt_kprintf("Example: delete_funnel_default -n funnel_default_1\n");
        return -RT_ERROR;
    }

    if (custom_obj_msh_check_delete_name(name) != RT_EOK)
    {
        rt_kprintf("Invalid name: %s\n", name);
        return -RT_ERROR;
    }

    funnel_obj_t obj = (funnel_obj_t)rt_object_find(name, RT_Object_Class_Custom);
    if (obj == RT_NULL)
    {
        rt_kprintf("Object not found: %s\n", name);
        return -RT_ERROR;
    }

    if (((custom_obj_t)obj)->type != Custom_Object_Class_Filter)
    {
        rt_kprintf("Not a funnel_default object: %s\n", name);
        return -RT_ERROR;
    }

    if (obj->ops == RT_NULL || obj->ops->delete == RT_NULL)
    {
        rt_kprintf("Delete not supported: %s\n", name);
        return -RT_ERROR;
    }

    if (obj->ops->delete(obj) != RT_EOK)
    {
        rt_kprintf("Failed to delete funnel_default: %s\n", name);
        return -RT_ERROR;
    }

    rt_kprintf("Deleted funnel_default: %s\n", name);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_delete_funnel_default, delete_funnel_default, Delete funnel_default object);
