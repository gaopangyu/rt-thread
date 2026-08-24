#include "interface_rs232.h"
#include "protocol_obj.h"
#include "bind.h"

#define DBG_TAG "interface_rs232"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static rt_err_t interface_rs232_rx_ind(rt_device_t dev, rt_size_t size)
{
    if(dev == RT_NULL || size == 0)
        return -RT_EINVAL;

    interface_rs232_obj_t obj = (interface_rs232_obj_t)dev->user_data;
    if (obj != RT_NULL)
        rt_sem_release(obj->rx_ind_sem);

    return RT_EOK;
}

// static void interface_rs232_recv_sub_event(void *parameter)
// {
//     if (parameter == RT_NULL)
//         return;
// }

static void interface_rs232_send_sub_event(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    interface_rs232_obj_t obj = (interface_rs232_obj_t)parameter;
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

static double_ret interface_rs232_set_param(interface_obj_t self, void *config)
{
    double_ret result = {0, 0};

    if (self == RT_NULL || config == RT_NULL) 
    {
        result.fail++;
        return result;
    }

    interface_rs232_config_t cfg = (interface_rs232_config_t)config;
    interface_rs232_obj_t obj = (interface_rs232_obj_t)self;
    interface_rs232_config_t obj_cfg = &(obj->config);

    obj->uart = rt_device_find(cfg->uart_name);
    if (obj->uart == RT_NULL) {
        result.fail++;
        return result;
    }

    rt_strncpy(obj_cfg->uart_name, cfg->uart_name, RT_NAME_MAX - 1);
    obj_cfg->uart_name[RT_NAME_MAX - 1] = 0;
    result.succ++;

    struct serial_configure serial_cfg;
    rt_err_t err = rt_device_control(obj->uart, RT_SERIAL_CTRL_GET_CONFIG, &serial_cfg);
    if (err != RT_EOK) {
        result.fail++;
        return result;
    }

    obj_cfg->bit_order = serial_cfg.bit_order;
    result.succ++;

    serial_cfg.baud_rate = cfg->baud_rate;
    obj_cfg->baud_rate = cfg->baud_rate;
    result.succ++;

    if(cfg->data_bits == DATA_BITS_8 || cfg->data_bits == DATA_BITS_9)
    {
        serial_cfg.data_bits = cfg->data_bits;
        obj_cfg->data_bits = cfg->data_bits;
        result.succ++;
    }
    else
    {
        obj_cfg->data_bits = serial_cfg.data_bits;
        result.fail++;
    }

    if(cfg->stop_bits == STOP_BITS_1 || cfg->stop_bits == STOP_BITS_2)
    {
        serial_cfg.stop_bits = cfg->stop_bits;
        obj_cfg->stop_bits = cfg->stop_bits;
        result.succ++;
    }
    else
    {
        obj_cfg->stop_bits = serial_cfg.stop_bits;
        result.fail++;
    }

    if(cfg->parity == UART_PARITY_NONE || cfg->parity == UART_PARITY_ODD || cfg->parity == UART_PARITY_EVEN)
    {
        serial_cfg.parity = cfg->parity;
        obj_cfg->parity = cfg->parity;
        result.succ++;
    }
    else
    {
        obj_cfg->parity = serial_cfg.parity;
        result.fail++;
    }

    if (obj->uart->ref_count == 0)
    {
        serial_cfg.rx_bufsz = cfg->rx_bufsz;
    #ifdef RT_SERIAL_USING_DMA
        serial_cfg.dma_ping_bufsz = cfg->dma_ping_bufsz;
    #endif
        serial_cfg.tx_bufsz = cfg->tx_bufsz;

        obj_cfg->rx_bufsz = cfg->rx_bufsz;
        result.succ++;

        obj_cfg->tx_bufsz = cfg->tx_bufsz;
        result.succ++;
    }

    rt_enter_critical();
    err = rt_device_control(obj->uart, RT_DEVICE_CTRL_CONFIG, &serial_cfg);
    if (err != RT_EOK)
    {
        result.fail++;
        result.succ = 0;
    }
    rt_exit_critical();

    obj->config.rx_en_pin = cfg->rx_en_pin;
    rt_pin_mode(obj->config.rx_en_pin, PIN_MODE_OUTPUT);
    rt_pin_write(obj->config.rx_en_pin, PIN_HIGH);

    return result;
}

static void interface_rs232_send(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    interface_rs232_obj_t obj = (interface_rs232_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;

    if (obj->uart == RT_NULL || custom_obj->txb.sub_mb == RT_NULL)
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

            custom_obj_bind_link_xfer(tx_custom_obj, custom_obj, tx_custom_obj->txb.len, RT_FALSE);

            rt_ssize_t len = 0;
            while(len < tx_custom_obj->txb.len)
            {
                rt_ssize_t write_len = rt_device_write(obj->uart, 0, tx_custom_obj->txb.buffer + len, tx_custom_obj->txb.len - len);
                if(write_len == 0)
                    break;
                len += write_len;
            }

            LOG_D("%s send to %s: %d\r\n", tx_custom_obj->parent.name, rt_obj->name, len);
            custom_obj_bind_ext_add_send(custom_obj, (rt_size_t)len, RT_FALSE);

            LOG_I("%s ---> %s send txb.pub_succ_event: 0x%X\r\n", rt_obj->name, tx_custom_obj->parent.name, custom_obj->event_flag);
            if( rt_event_send(tx_custom_obj->txb.pub_succ_event, custom_obj->event_flag) != RT_EOK )
                LOG_E("%s send tx_pub_succ_event error: %d/%d\r\n", rt_obj->name, custom_obj->event_flag, tx_custom_obj->txb.pre_event_mask);
        }

        rt_mutex_release(custom_obj->txb.bind_mutex);
    }
}

static rt_err_t interface_rs232_handle_rx_bind_next(interface_rs232_obj_t obj,
                                                 rt_object_t rt_obj,
                                                 custom_obj_t custom_obj,
                                                 rt_ssize_t len)
{
    if ( custom_obj->rxb.pub_event == RT_NULL || custom_obj->event_flag == 0 
      || custom_obj->rxb.pub_succ_event == RT_NULL || custom_obj->rxb.next_event_mask == 0 )
        return -RT_EINVAL;

    custom_obj_bind_ext_add_recv(custom_obj, (rt_size_t)len, RT_TRUE);
    custom_obj->rxb.buffer = obj->rx_buffer;
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

static void interface_rs232_recv(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    interface_rs232_obj_t obj = (interface_rs232_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    interface_obj_t interface_obj = (interface_obj_t)obj;

    if (obj->uart == RT_NULL || obj->rx_buffer == RT_NULL || obj->rx_bufsz == 0)
        return;

    while (1)
    {
        if(rt_sem_take(obj->rx_ind_sem, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s take rx_ind_sem error", rt_obj->name);
            break;
        }

        rt_ssize_t len = 0;
        while ((len = rt_device_read(obj->uart, 0, obj->rx_buffer, obj->rx_bufsz)) > 0)
        {
            LOG_D("%s recv from %s: %d\r\n", rt_obj->name, obj->uart->parent.name, len);

            if (rt_mutex_take(custom_obj->rxb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
            {
                LOG_E("%s take rx_bind_mutex error", rt_obj->name);
                break;
            }

            interface_rs232_handle_rx_bind_next(obj, rt_obj, custom_obj, len);

            rt_mutex_release(custom_obj->rxb.bind_mutex);
        }
    }
}
 
static rt_err_t interface_rs232_delete(interface_obj_t self)
{
    if (self == RT_NULL) {
        return -RT_EINVAL;
    }

    if (rt_strncmp(self->parent.parent.name, INTRFACE_RS232_PARENT_NAME, RT_NAME_MAX) == 0) {
        return -RT_EINVAL;
    }

    interface_rs232_obj_t obj = (interface_rs232_obj_t)self;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;

    if(rt_object_find(rt_obj->name, RT_Object_Class_Custom) != RT_NULL)
        rt_object_detach(rt_obj);

    bind_delete_all_for_obj(custom_obj);

    if (obj->uart != RT_NULL) {
        rt_device_set_rx_indicate(obj->uart, RT_NULL);
        obj->uart->user_data = RT_NULL;
        rt_device_close(obj->uart);
        obj->uart = RT_NULL;
    }

    // if (obj->rx_sub_event_tid != RT_NULL)
    // {
    //     rt_thread_delete(obj->rx_sub_event_tid);
    //     obj->rx_sub_event_tid = RT_NULL;
    // }

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

    if(obj->rx_ind_sem != RT_NULL)
    {
        rt_sem_delete(obj->rx_ind_sem);
        obj->rx_ind_sem = RT_NULL;
    }
    
    // if(custom_obj->rxb.sub_mb != RT_NULL)
    // {
    //     rt_mb_delete(custom_obj->rxb.sub_mb);
    //     custom_obj->rxb.sub_mb = RT_NULL;
    // }

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

    if (obj->rx_buffer != RT_NULL)
    {
        rt_free(obj->rx_buffer);
        obj->rx_buffer = RT_NULL;
        obj->rx_bufsz = 0;
    }

    rt_free(obj);
    obj = RT_NULL;

    return RT_EOK;
}

static interface_obj_t  interface_rs232_create(interface_obj_t self, char *name, void *config)
{
    if (self == RT_NULL || name == RT_NULL) {
        LOG_E("create error: param");
        return RT_NULL;
    }

    if (rt_strncmp(self->parent.parent.name, INTRFACE_RS232_PARENT_NAME, RT_NAME_MAX) != 0) {
        LOG_E("create error: invalid parent");
        return RT_NULL;
    }

    if (rt_object_find(name, RT_Object_Class_Custom) != RT_NULL)
    {
        LOG_E("create error: name exist");
        return RT_NULL;
    }

    interface_rs232_obj_t obj = rt_malloc(sizeof(struct interface_rs232_obj));
    if (obj == RT_NULL) {
        LOG_E("create error: rt_malloc");
        return RT_NULL;
    }

    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    interface_obj_t interface_obj = (interface_obj_t)obj;

    rt_memcpy(obj, self, sizeof(struct interface_rs232_obj));
    rt_strncpy(rt_obj->name, name, RT_NAME_MAX - 1);
    rt_obj->name[RT_NAME_MAX - 1] = 0;

    if(config == RT_NULL)
        config = &(obj->config);

    obj->rx_bufsz = ((interface_rs232_config_t)config)->dma_ping_bufsz;
    obj->rx_buffer = rt_malloc(obj->rx_bufsz);
    if (obj->rx_buffer == RT_NULL) {
        LOG_E("create error: rx buffer allocation");
        goto exit;
    }

    double_ret result = interface_rs232_set_param(interface_obj, config);
    if (result.fail != 0)
    {
        LOG_E("create error: set param");
        goto exit;
    }

    char name_buf[RT_NAME_MAX] = {0};

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_ind");
    if((obj->rx_ind_sem = rt_sem_create(name_buf, 0, RT_IPC_FLAG_FIFO)) == RT_NULL) {
        LOG_E("create error: rx ind sem create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx");
    if((obj->rx_tid = rt_thread_create(name_buf, interface_rs232_recv, obj, 1024, CUSTOM_THREAD_PRIORITY, 20)) == RT_NULL) {
        LOG_E("create error: rx thread create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_tx");
    if((obj->tx_tid = rt_thread_create(name_buf, interface_rs232_send, obj, 1024, CUSTOM_THREAD_PRIORITY, 20)) == RT_NULL) {
        LOG_E("create error: tx thread create");
        goto exit;
    }

    // rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_sub_event");
    // if((obj->rx_sub_event_tid = rt_thread_create(name_buf, interface_rs232_recv_sub_event, obj, 1024, CUSTOM_THREAD_PRIORITY - 1, 20)) == RT_NULL) {
    //     LOG_E("create error: rx sub event thread create");
    //     goto exit;
    // }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_tx_sub_event");
    if((obj->tx_sub_event_tid = rt_thread_create(name_buf, interface_rs232_send_sub_event, obj, 1024, CUSTOM_THREAD_PRIORITY - 1, 20)) == RT_NULL) {
        LOG_E("create error: tx sub event thread create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_bind_mutex");
    if((custom_obj->rxb.bind_mutex = rt_mutex_create(name_buf, RT_IPC_FLAG_FIFO)) == RT_NULL) {
        LOG_E("create error: rx bind mutex create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_tx_bind_mutex");
    if((custom_obj->txb.bind_mutex = rt_mutex_create(name_buf, RT_IPC_FLAG_FIFO)) == RT_NULL) {
        LOG_E("create error: tx bind mutex create");
        goto exit;
    }

    // rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_sub_mb");
    // if ((custom_obj->rxb.sub_mb = rt_mb_create(name_buf, CUSTOM_PER_LAYER_OBJ_MAX, RT_IPC_FLAG_FIFO)) == RT_NULL) {
    //     LOG_E("create error: rx mailbox create");
    //     goto exit;
    // }

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
    // rt_thread_startup(obj->rx_sub_event_tid);
    rt_thread_startup(obj->tx_sub_event_tid);

    obj->uart->user_data = obj;
    rt_device_set_rx_indicate(obj->uart, interface_rs232_rx_ind);

    if (rt_device_open(obj->uart, RT_DEVICE_OFLAG_RDWR | RT_SERIAL_RX_NON_BLOCKING | RT_SERIAL_TX_BLOCKING) != RT_EOK)
    {
        LOG_E("create error: device open");
        goto exit;
    }

    rt_object_init(rt_obj, RT_Object_Class_Custom, name);
    LOG_I("create success: %s", name);
    return interface_obj;

exit:
    interface_rs232_delete(interface_obj);
    return RT_NULL;
}

static const struct interface_obj_ops interface_rs232_ops =
{
    .create = interface_rs232_create,
    .delete = interface_rs232_delete,
    .set_param = interface_rs232_set_param,
};


static rt_err_t  interface_rs232_gc(custom_obj_t self)
{
    if (self == RT_NULL)
        return -RT_EINVAL;

    interface_obj_t interface_obj = (interface_obj_t)self;
    return interface_rs232_delete(interface_obj);
}

static const struct custom_object_ops custom_obj_ops =
{
    .gc = interface_rs232_gc,
};

static int interface_rs232_init(void)
{
    static struct interface_rs232_obj object = {0};
    interface_rs232_obj_t obj = &object;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    interface_obj_t interface_obj = (interface_obj_t)obj;

    if (rt_object_find(INTRFACE_RS232_PARENT_NAME, RT_Object_Class_Custom) != RT_NULL)
        return -RT_ERROR;

    custom_obj->type = Custom_Object_Class_Interface;

    interface_obj_init(custom_obj);

    custom_obj->ops = &custom_obj_ops;

    interface_obj->ops = &interface_rs232_ops;

    rt_strncpy(obj->config.uart_name, "vuart1", rt_strnlen("vuart1", RT_NAME_MAX - 1));
    obj->config.baud_rate = BAUD_RATE_115200;
    obj->config.data_bits = DATA_BITS_8;
    obj->config.stop_bits = STOP_BITS_1;
    obj->config.parity = PARITY_NONE;
    obj->config.bit_order = BIT_ORDER_LSB;
    obj->config.rx_bufsz = 1024;
    obj->config.dma_ping_bufsz = 512;
    obj->config.tx_bufsz = 1024;
    obj->config.rx_en_pin = GET_PIN(B, 7);

    rt_object_init(rt_obj, RT_Object_Class_Custom, INTRFACE_RS232_PARENT_NAME);

    return RT_EOK;
}

INIT_ENV_EXPORT(interface_rs232_init);

static int cmd_create_interface_rs232(int argc, char *argv[])
{
    if (argc < 2)
    {
        rt_kprintf("Usage: create_interface_rs232 <name> [-u uart] [-b baud] [-p rx_en_pin] [-r rx_bufsz] [-d dma_ping_bufsz] [-t tx_bufsz]\n");
        rt_kprintf("Example: create_interface_rs232 interface_rs232_1 -u vuart1 -b 115200\n");
        return -RT_ERROR;
    }

    const char *name = argv[1];
    rt_err_t err = custom_obj_msh_check_create_name(name, "interface_rs232");

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

    interface_obj_t parent = (interface_obj_t)rt_object_find(INTRFACE_RS232_PARENT_NAME, RT_Object_Class_Custom);
    if (parent == RT_NULL || parent->ops == RT_NULL || parent->ops->create == RT_NULL)
    {
        rt_kprintf("Parent not ready: %s\n", INTRFACE_RS232_PARENT_NAME);
        return -RT_ERROR;
    }

    struct interface_rs232_config *config_ptr = RT_NULL;
    struct interface_rs232_config config = {0};
    const char *uart_name = custom_obj_msh_get_opt(argc, argv, "u");
    const char *baud_str = custom_obj_msh_get_opt(argc, argv, "b");
    const char *pin_str = custom_obj_msh_get_opt(argc, argv, "p");
    const char *rx_buf_str = custom_obj_msh_get_opt(argc, argv, "r");
    const char *dma_ping_buf_str = custom_obj_msh_get_opt(argc, argv, "d");
    const char *tx_buf_str = custom_obj_msh_get_opt(argc, argv, "t");

    if (uart_name != RT_NULL || baud_str != RT_NULL || pin_str != RT_NULL ||
        rx_buf_str != RT_NULL || dma_ping_buf_str != RT_NULL || tx_buf_str != RT_NULL)
    {
        config_ptr = &config;
        config.baud_rate = BAUD_RATE_115200;
        config.data_bits = DATA_BITS_8;
        config.stop_bits = STOP_BITS_1;
        config.parity = PARITY_NONE;
        config.bit_order = BIT_ORDER_LSB;
        config.rx_bufsz = 1024;
        config.dma_ping_bufsz = 512;
        config.tx_bufsz = 1024;
        config.rx_en_pin = GET_PIN(B, 7);

        if (uart_name != RT_NULL)
            rt_strncpy(config.uart_name, uart_name, RT_NAME_MAX - 1);
        if (baud_str != RT_NULL)
            config.baud_rate = (rt_uint32_t)strtoul(baud_str, RT_NULL, 10);
        if (pin_str != RT_NULL)
            config.rx_en_pin = (rt_base_t)strtoul(pin_str, RT_NULL, 10);
        if (rx_buf_str != RT_NULL)
            config.rx_bufsz = (rt_size_t)strtoul(rx_buf_str, RT_NULL, 10);
        if (dma_ping_buf_str != RT_NULL)
            config.dma_ping_bufsz = (rt_size_t)strtoul(dma_ping_buf_str, RT_NULL, 10);
        if (tx_buf_str != RT_NULL)
            config.tx_bufsz = (rt_size_t)strtoul(tx_buf_str, RT_NULL, 10);
    }

    if (parent->ops->create(parent, (char *)name, config_ptr) == RT_NULL)
    {
        rt_kprintf("Failed to create interface_rs232: %s\n", name);
        return -RT_ERROR;
    }

    rt_kprintf("Created interface_rs232: %s\n", name);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_create_interface_rs232, create_interface_rs232, Create interface_rs232 object);

static int cmd_delete_interface_rs232(int argc, char *argv[])
{
    const char *name = custom_obj_msh_get_opt(argc, argv, "n");

    if (name == RT_NULL)
    {
        rt_kprintf("Usage: delete_interface_rs232 -n <name>\n");
        rt_kprintf("Example: delete_interface_rs232 -n interface_rs232_1\n");
        return -RT_ERROR;
    }

    if (custom_obj_msh_check_delete_name(name) != RT_EOK)
    {
        rt_kprintf("Invalid name: %s\n", name);
        return -RT_ERROR;
    }

    interface_obj_t obj = (interface_obj_t)rt_object_find(name, RT_Object_Class_Custom);
    if (obj == RT_NULL)
    {
        rt_kprintf("Object not found: %s\n", name);
        return -RT_ERROR;
    }

    if (((custom_obj_t)obj)->type != Custom_Object_Class_Interface)
    {
        rt_kprintf("Not an interface_rs232 object: %s\n", name);
        return -RT_ERROR;
    }

    if (obj->ops == RT_NULL || obj->ops->delete == RT_NULL)
    {
        rt_kprintf("Delete not supported: %s\n", name);
        return -RT_ERROR;
    }

    if (obj->ops->delete(obj) != RT_EOK)
    {
        rt_kprintf("Failed to delete interface_rs232: %s\n", name);
        return -RT_ERROR;
    }

    rt_kprintf("Deleted interface_rs232: %s\n", name);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_delete_interface_rs232, delete_interface_rs232, Delete interface_rs232 object);

