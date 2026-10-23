#include "upload_net.h"
#include "custom_obj.h"
#include "msh_util.h"

#define DBG_TAG "upload_net"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static rt_device_t net_dev = RT_NULL;
static rt_list_t net_link_list;
static struct rt_semaphore net_parent_rx_sem;
static struct ppp_adapter net_ppp_adapter;
volatile rt_bool_t net_parent_idle_flag = RT_TRUE;

static rt_err_t upload_net_parent_rx_ind(rt_device_t dev, rt_size_t size)
{
    UNUSED(dev);
    UNUSED(size);

    rt_sem_release(&net_parent_rx_sem);

    return RT_EOK;
}

static void upload_net_recv_sub_event(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    upload_net_obj_t obj = (upload_net_obj_t)parameter;
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

static double_ret upload_net_set_param(upload_obj_t self, void *config)
{
    double_ret result = {0, 0};

    if (self == RT_NULL || config == RT_NULL) 
    {
        result.fail++;
        return result;
    }

    upload_net_config_t cfg = (upload_net_config_t)config;
    upload_net_obj_t obj = (upload_net_obj_t)self;
    upload_net_config_t obj_cfg = &(obj->config);

    obj_cfg->number = cfg->number;
    obj->link.id = cfg->number;
    result.succ++;

    return result;
}

static void upload_net_parent_recv(void *parameter)
{
    static uint8_t buf[PPP_PKT_WIRE_MAX * 2];
    static rt_size_t len = 0;
    rt_object_t rt_obj;
    rt_ssize_t n;

    if (parameter == RT_NULL)
        return;

    rt_obj = (rt_object_t)parameter;

    while (1)
    {
        net_parent_idle_flag = RT_TRUE;
        if (rt_sem_take(&net_parent_rx_sem, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s take net_parent_rx_sem error", rt_obj->name);
            break;
        }
        net_parent_idle_flag = RT_FALSE;

        while (len < sizeof(buf))
        {
            n = rt_device_read(net_dev, 0, buf + len, sizeof(buf) - len);
            if (n <= 0)
                break;
            len += n;
        }

        while (ppp_adapter_recv(&net_ppp_adapter, buf, &len))
            ;

        /* 抽完帧后缓冲仍满：半帧/垃圾占满，丢弃才能继续收 */
        if (len >= sizeof(buf))
        {
            LOG_W("%s rx buffer full, no complete frame, drop", rt_obj->name);
            len = 0;
        }
    }
}

static rt_ssize_t upload_net_send(upload_obj_t self, const void *buffer, rt_size_t size)
{
    if (self == RT_NULL || buffer == RT_NULL || size == 0)
        return 0;

    upload_net_obj_t obj = (upload_net_obj_t)self;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;

    if (rt_mutex_take(custom_obj->txb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
    {
        LOG_E("%s take tx_bind_mutex error", rt_obj->name);
        return 0;
    }

    if (custom_obj->txb.pub_event == RT_NULL || custom_obj->event_flag == 0 
     || custom_obj->txb.pub_succ_event == RT_NULL || custom_obj->txb.pre_event_mask == 0)
    {
        rt_mutex_release(custom_obj->txb.bind_mutex);
        return 0;
    }

    custom_obj->txb.buffer = (rt_uint8_t *)buffer;
    custom_obj->txb.len = size;

    LOG_D("%s send to all pre tx_bind: %d\r\n", rt_obj->name, size);

    rt_thread_yield();
    LOG_I("%s send txb.pub_event: 0x%X\r\n", rt_obj->name, custom_obj->event_flag);
    if (rt_event_send(custom_obj->txb.pub_event, custom_obj->event_flag) == RT_EOK)
    {
        rt_uint32_t recv_event_flag = 0;
        LOG_I("%s recv txb.pub_succ_event: 0x%X\r\n", rt_obj->name, custom_obj->txb.pre_event_mask);
        if (rt_event_recv(custom_obj->txb.pub_succ_event, custom_obj->txb.pre_event_mask, RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &recv_event_flag) != RT_EOK)
            LOG_E("%s recv tx_pub_succ_event error: %d/%d\r\n", rt_obj->name, recv_event_flag, custom_obj->txb.pre_event_mask);
    }
    else
        LOG_E("%s send tx_pub_event error: %d/%d\r\n", rt_obj->name, custom_obj->txb.pub_event, custom_obj->txb.pre_event_mask);

    custom_obj->txb.buffer = RT_NULL;
    custom_obj->txb.len = 0;

    rt_mutex_release(custom_obj->txb.bind_mutex);

    return size;
}

static rt_err_t upload_net_handle_rx_bind_pre(upload_obj_t self, const void *buffer, rt_size_t size)
{
    upload_net_obj_t obj;
    custom_obj_t custom_obj;
    rt_size_t sent;

    if (self == RT_NULL || buffer == RT_NULL || size == 0)
        return -RT_EINVAL;

    obj = (upload_net_obj_t)self;
    custom_obj = (custom_obj_t)obj;
    sent = ppp_adapter_send(&net_ppp_adapter, &obj->link, buffer, size);
    custom_obj_bind_add_ext_send(custom_obj, sent, RT_TRUE);

    return (sent == size) ? RT_EOK : -RT_ERROR;
}

/* 绑定侧收包线程：从绑定对象取数据，调用 upload_net_handle_rx_bind_pre 发出 */
static void upload_net_recv(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    upload_net_obj_t obj = (upload_net_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    upload_obj_t upload_obj = (upload_obj_t)obj;

    if(custom_obj->rxb.sub_mb == RT_NULL)
        return;

    while (1)
    {
        obj->link_idle_flag = RT_TRUE;
        rt_uint32_t recv_event_flag = 0;
        if(rt_mb_recv(custom_obj->rxb.sub_mb, (rt_ubase_t *)&recv_event_flag, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s recv rxb.sub_mb error", rt_obj->name);
            break;
        }
        obj->link_idle_flag = RT_FALSE;
        
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

        rt_err_t err = RT_EOK;
        custom_obj_t fail_pre = RT_NULL;
        while(recv_event_flag != 0)
        {
            custom_obj_t rx_custom_obj = custom_obj_get_by_event_flag(custom_obj->rxb.pre_obj_list, &recv_event_flag, RT_TRUE);
            if(rx_custom_obj == RT_NULL)
                break;

            LOG_D("%s recv from %s: %d\r\n", rt_obj->name, rx_custom_obj->parent.name, rx_custom_obj->rxb.len);
            custom_obj_bind_xfer_link(rx_custom_obj, custom_obj, rx_custom_obj->rxb.len, RT_TRUE);

            if(rx_custom_obj->rxb.pub_succ_event == RT_NULL)
                continue;

            err = upload_net_handle_rx_bind_pre(upload_obj, rx_custom_obj->rxb.buffer, rx_custom_obj->rxb.len);

            LOG_I("%s ---> %s send rxb.pub_succ_event: 0x%X\r\n", rt_obj->name, rx_custom_obj->parent.name, custom_obj->event_flag);
            if( rt_event_send(rx_custom_obj->rxb.pub_succ_event, custom_obj->event_flag) != RT_EOK )
                LOG_E("%s send rx_pub_succ_event error: %d/%d\r\n", rt_obj->name, custom_obj->event_flag, rx_custom_obj->rxb.next_event_mask);

            if (err != RT_EOK)
            {
                fail_pre = rx_custom_obj;
                break;
            }
        }

        rt_mutex_release(custom_obj->rxb.bind_mutex);

        if (fail_pre != RT_NULL && rt_strncmp(fail_pre->parent.name, "upload_net_255", RT_NAME_MAX) != 0)
        {
            LOG_E("%s send to net failed, unbind %s", rt_obj->name, fail_pre->parent.name);
            bind_delete(fail_pre, custom_obj, RT_TRUE);
        }
    }
}

/* 按 uart_id 找到对应通道的 ppp_link */
static struct ppp_link *upload_net_ppp_find_link(rt_uint8_t id)
{
    struct upload_net_obj *node;
    struct ppp_link *link = RT_NULL;

    rt_enter_critical();
    rt_list_for_each_entry(node, &net_link_list, list)
    {
        if (node->link.id == id)
        {
            link = &node->link;
            break;
        }
    }
    rt_exit_critical();

    return link;
}

/* PPP 组包完成：把完整消息交给绑定下游 */
static void upload_net_ppp_on_data(struct ppp_link *link,
                                     const uint8_t *payload, rt_size_t len)
{
    upload_net_obj_t obj;

    if (link == RT_NULL)
        return;

    obj = rt_container_of(link, struct upload_net_obj, link);
    upload_net_send((upload_obj_t)obj, payload, len);
}

static const struct ppp_adapter_ops net_ppp_ops =
{
    .find_link = upload_net_ppp_find_link,
    .on_data = upload_net_ppp_on_data,
};

static rt_err_t upload_net_delete(upload_obj_t self)
{
    if (self == RT_NULL) {
        return -RT_EINVAL;
    }

    if (rt_strncmp(self->parent.parent.name, UPLOAD_NET_PARENT_NAME, RT_NAME_MAX) == 0) {
        return -RT_EINVAL;
    }

    upload_net_obj_t obj = (upload_net_obj_t)self;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;

    if(rt_object_find(rt_obj->name, RT_Object_Class_Custom) != RT_NULL)
        rt_object_detach(rt_obj);

    bind_delete_all_for_obj(custom_obj);

    rt_enter_critical();
    struct upload_net_obj *net_obj, *next_net_obj;
    rt_list_for_each_entry_safe(net_obj, next_net_obj, &net_link_list, list)
    {
        if(net_obj == obj)
            rt_list_remove(&net_obj->list);
    }
    rt_exit_critical();

    if (obj->rx_sub_event_tid != RT_NULL)
    {
        rt_thread_delete(obj->rx_sub_event_tid);
        obj->rx_sub_event_tid = RT_NULL;
    }

    while(obj->rx_tid != RT_NULL && obj->link_idle_flag == RT_FALSE)
    {
        rt_thread_mdelay(20);
    }

    while(net_parent_idle_flag == RT_FALSE)
    {
        rt_thread_mdelay(20);
    }

    if (obj->rx_tid != RT_NULL)
    {
        rt_thread_delete(obj->rx_tid);
        obj->rx_tid = RT_NULL;
    }

    ppp_link_deinit(&obj->link);

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

static upload_obj_t upload_net_create(upload_obj_t self, char *name, void *config)
{
    if (self == RT_NULL || name == RT_NULL) {
        LOG_E("create error: param");
        return RT_NULL;
    }

    if (rt_strncmp(self->parent.parent.name, UPLOAD_NET_PARENT_NAME, RT_NAME_MAX) != 0) {
        LOG_E("create error: invalid parent");
        return RT_NULL;
    }

    if (rt_object_find(name, RT_Object_Class_Custom) != RT_NULL)
    {
        LOG_E("create error: name exist");
        return RT_NULL;
    }

    upload_net_obj_t obj = rt_malloc(sizeof(struct upload_net_obj));
    if (obj == RT_NULL) {
        LOG_E("create error: memory allocation");
        return RT_NULL;
    }
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    upload_obj_t upload_obj = (upload_obj_t)obj;

    rt_memcpy(obj, self, sizeof(struct upload_net_obj));
    rt_strncpy(rt_obj->name, name, RT_NAME_MAX - 1);
    rt_obj->name[RT_NAME_MAX - 1] = 0;

    if(config == RT_NULL)
        config = &(obj->config);

    double_ret result = upload_net_set_param(upload_obj, config);
    if (result.fail != 0)
    {
        LOG_E("create error: set param");
        goto exit;
    }

    char name_buf[RT_NAME_MAX] = {0};

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_recv");
    obj->rx_tid = rt_thread_create(name_buf, upload_net_recv, obj, 2048, CUSTOM_THREAD_PRIORITY, 20);
    if (obj->rx_tid == RT_NULL) {
        LOG_E("create error: thread create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_sub_event");
    if((obj->rx_sub_event_tid = rt_thread_create(name_buf, upload_net_recv_sub_event, obj, 1024, CUSTOM_THREAD_PRIORITY - 1, 20)) == RT_NULL) {
        LOG_E("create error: rx sub event thread create");
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

    if (ppp_link_init(&obj->link, obj->config.number) != RT_EOK) {
        LOG_E("create error: ppp link init");
        goto exit;
    }

    rt_thread_startup(obj->rx_tid);
    rt_thread_startup(obj->rx_sub_event_tid);

    rt_enter_critical();
    rt_list_insert_after(&net_link_list, &obj->list);
    rt_exit_critical();

    rt_object_init(rt_obj, RT_Object_Class_Custom, name);

    LOG_I("create success: %s", name);
    return upload_obj;

exit:
    upload_net_delete(upload_obj);
    return RT_NULL;
}

static rt_size_t upload_net_get_history(upload_obj_t self, rt_uint8_t *buffer, rt_size_t size, event_history_t event_history, rt_bool_t *is_end)
{
    RT_UNUSED(self);
    RT_UNUSED(buffer);
    RT_UNUSED(size);
    RT_UNUSED(event_history);
    if (is_end != RT_NULL)
        *is_end = RT_TRUE;
    return 0;
}

static const struct upload_obj_ops upload_net_ops =
{
    .create = upload_net_create,
    .delete = upload_net_delete,
    .set_param = upload_net_set_param,
    .get_history = upload_net_get_history,
};

static rt_err_t  upload_net_gc(custom_obj_t self)
{
    if (self == RT_NULL)
        return -RT_EINVAL;

    upload_obj_t upload_obj = (upload_obj_t)self;
    return upload_net_delete(upload_obj);
}

static const struct custom_object_ops custom_obj_ops =
{
    .gc = upload_net_gc,
};

/* 打开 UART、初始化 PPP 适配器、启动 parent_recv 收包线程 */
static int upload_net_init(void)
{
    static struct upload_net_obj object = {0};
    upload_net_obj_t obj = &object;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    upload_obj_t upload_obj = (upload_obj_t)obj;

    if (rt_object_find(UPLOAD_NET_PARENT_NAME, RT_Object_Class_Custom) != RT_NULL)
        return -RT_ERROR;

    custom_obj->type = Custom_Object_Class_Upload;

    obj->link_idle_flag = RT_TRUE;

    upload_obj_init(custom_obj);

    custom_obj->ops = &custom_obj_ops;

    upload_obj->ops = &upload_net_ops;

    // rt_pin_mode(GET_PIN(F, 2), PIN_MODE_OUTPUT);
    // rt_pin_write(GET_PIN(F, 2), PIN_HIGH);
    // net_dev = rt_device_find("uart3");

    // rt_pin_mode(GET_PIN(F, 3), PIN_MODE_OUTPUT);
    // rt_pin_write(GET_PIN(F, 3), PIN_HIGH);
    // net_dev = rt_device_find("uart6");

    net_dev = rt_device_find("uart5");
    if (net_dev == RT_NULL) 
        return -RT_ERROR;

    if (net_dev->ref_count != 0)
    {
        LOG_E("init error: net_dev ref_count is not 0");
        return -RT_ERROR;
    }

    struct serial_configure serial_cfg = RT_SERIAL_CONFIG_DEFAULT;
    serial_cfg.baud_rate = BAUD_RATE_2000000;
    serial_cfg.data_bits = DATA_BITS_8;
    serial_cfg.stop_bits = STOP_BITS_1;
    serial_cfg.parity = PARITY_NONE;
    serial_cfg.bit_order = BIT_ORDER_LSB;
    serial_cfg.rx_bufsz = RT_ALIGN(PPP_PKT_WIRE_MAX, RT_ALIGN_SIZE) * 2;
    #ifdef RT_SERIAL_USING_DMA
        serial_cfg.dma_ping_bufsz = serial_cfg.rx_bufsz / 2;
    #endif
    serial_cfg.tx_bufsz = RT_ALIGN(PPP_PKT_WIRE_MAX, RT_ALIGN_SIZE);

    rt_err_t err = rt_device_control(net_dev, RT_DEVICE_CTRL_CONFIG, &serial_cfg);
    if (err != RT_EOK) {
        LOG_E("init error: failed to set serial config");
        return -RT_ERROR;
    }

    err = ppp_adapter_init(&net_ppp_adapter, "net_ppp_adapter", net_dev, &net_ppp_ops);
    if (err != RT_EOK)
    {
        LOG_E("init error: failed to init ppp adapter");
        return -RT_ERROR;
    }

    err = rt_sem_init(&net_parent_rx_sem, "net_parent_rx_sem", 0, RT_IPC_FLAG_FIFO);
    if (err != RT_EOK)    
    {
        LOG_E("init error: failed to init net_dev rx semaphore");
        ppp_adapter_deinit(&net_ppp_adapter);
        return -RT_ERROR;
    }

    rt_list_init(&net_link_list);

    rt_thread_t tid = rt_thread_create("upload_net_parent_recv",
                                     upload_net_parent_recv,
                                     obj,
                                     2048,
                                     CUSTOM_THREAD_PRIORITY,
                                     20);
    if (tid == RT_NULL)
    {
        LOG_E("init error: failed to create upload_net_parent_recv thread");
        rt_sem_detach(&net_parent_rx_sem);
        ppp_adapter_deinit(&net_ppp_adapter);
        return -RT_ERROR;
    }
    rt_thread_startup(tid);

    rt_device_set_rx_indicate(net_dev, upload_net_parent_rx_ind);

    if (rt_device_open(net_dev, RT_DEVICE_OFLAG_RDWR | RT_SERIAL_RX_NON_BLOCKING | RT_SERIAL_TX_BLOCKING) != RT_EOK)
    {
        LOG_E("init error: failed to open net_dev");
        rt_device_set_rx_indicate(net_dev, RT_NULL);
        rt_sem_detach(&net_parent_rx_sem);
        ppp_adapter_deinit(&net_ppp_adapter);
        rt_thread_delete(tid);
        return -RT_ERROR;
    }

    rt_object_init(rt_obj, RT_Object_Class_Custom, UPLOAD_NET_PARENT_NAME);

    return RT_EOK;
}

INIT_ENV_EXPORT(upload_net_init);

static int cmd_upload_net_create(int argc, char *argv[])
{
    if (argc < 2)
    {
        rt_kprintf("Usage: upload_net_create <name> [-n number]\n");
        rt_kprintf("Example: upload_net_create upload_net_1 -n 1\n");
        return -RT_ERROR;
    }

    const char *name = argv[1];
    rt_err_t err = msh_check_create_name(name, "upload_net");

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

    upload_obj_t parent = (upload_obj_t)rt_object_find(UPLOAD_NET_PARENT_NAME, RT_Object_Class_Custom);
    if (parent == RT_NULL || parent->ops == RT_NULL || parent->ops->create == RT_NULL)
    {
        rt_kprintf("Parent not ready: %s\n", UPLOAD_NET_PARENT_NAME);
        return -RT_ERROR;
    }

    struct upload_net_config *config_ptr = RT_NULL;
    struct upload_net_config config = {0};
    const char *number_str = msh_get_opt(argc, argv, "n");

    if (number_str != RT_NULL)
    {
        config_ptr = &config;
        config.number = (rt_uint8_t)strtoul(number_str, RT_NULL, 10);
    }

    if (parent->ops->create(parent, (char *)name, config_ptr) == RT_NULL)
    {
        rt_kprintf("Failed to create upload_net: %s\n", name);
        return -RT_ERROR;
    }

    rt_kprintf("Created upload_net: %s\n", name);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_upload_net_create, upload_net_create, Create upload_net object);

static int cmd_upload_net_delete(int argc, char *argv[])
{
    const char *name = msh_get_opt(argc, argv, "n");

    if (name == RT_NULL)
    {
        rt_kprintf("Usage: upload_net_delete -n <name>\n");
        rt_kprintf("Example: upload_net_delete -n upload_net_1\n");
        return -RT_ERROR;
    }

    if (msh_check_delete_name(name) != RT_EOK)
    {
        rt_kprintf("Invalid name: %s\n", name);
        return -RT_ERROR;
    }

    upload_obj_t obj = (upload_obj_t)rt_object_find(name, RT_Object_Class_Custom);
    if (obj == RT_NULL)
    {
        rt_kprintf("Object not found: %s\n", name);
        return -RT_ERROR;
    }

    if (((custom_obj_t)obj)->type != Custom_Object_Class_Upload)
    {
        rt_kprintf("Not an upload_net object: %s\n", name);
        return -RT_ERROR;
    }

    if (obj->ops == RT_NULL || obj->ops->delete == RT_NULL)
    {
        rt_kprintf("Delete not supported: %s\n", name);
        return -RT_ERROR;
    }

    if (obj->ops->delete(obj) != RT_EOK)
    {
        rt_kprintf("Failed to delete upload_net: %s\n", name);
        return -RT_ERROR;
    }

    rt_kprintf("Deleted upload_net: %s\n", name);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_upload_net_delete, upload_net_delete, Delete upload_net object);
