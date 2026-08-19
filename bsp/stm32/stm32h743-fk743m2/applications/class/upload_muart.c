#include "upload_muart.h"
#include "custom_obj.h"

#define DBG_TAG "upload_muart"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define MUART_BUFSZ                 4096

static rt_device_t muart = RT_NULL;
static rt_list_t muart_uart_list;
static struct rt_mutex muart_send_mutex;
static struct rt_semaphore muart_rx_sem;
static union muart_pkt muart_tx_pkt;
static uint8_t muart_wire_tx[MUART_PKT_WIRE_MAX];
static uint8_t muart_raw_decode[MUART_PKT_MAX];
volatile rt_bool_t thread2_idle_flag = RT_FALSE;

static rt_ssize_t upload_muart_pkt_write_all(const void *buf, rt_size_t size)
{
    const uint8_t *p = (const uint8_t *)buf;
    rt_ssize_t send_len = 0;
    rt_uint8_t retry = 0;

    if (buf == RT_NULL || size == 0)
        return 0;

    while ((rt_size_t)send_len < size)
    {
        rt_ssize_t len = rt_device_write(muart, 0, p + send_len, size - (rt_size_t)send_len);

        if (len > 0)
        {
            send_len += len;
            retry = 0;
            continue;
        }

        if (++retry > 50)
            break;

        rt_thread_mdelay(20);
    }

    return send_len;
}

static rt_size_t upload_muart_pkt_send_wire_locked(const uint8_t *raw, rt_size_t raw_len)
{
    rt_ssize_t wire_len;
    rt_ssize_t send_len;

    if (raw == RT_NULL || raw_len == 0)
        return 0;

    wire_len = ppp_to_wire(raw, raw_len, muart_wire_tx, sizeof(muart_wire_tx));
    if (wire_len <= 0)
        return 0;

    send_len = upload_muart_pkt_write_all(muart_wire_tx, (rt_size_t)wire_len);

    return ((rt_size_t)send_len == (rt_size_t)wire_len) ? raw_len : 0;
}

static rt_size_t upload_muart_pkt_send_wire(const uint8_t *raw, rt_size_t raw_len)
{
    rt_size_t sent;

    if (raw == RT_NULL || raw_len == 0)
        return 0;

    if (rt_mutex_take(&muart_send_mutex, RT_WAITING_FOREVER) != RT_EOK)
        return 0;

    sent = upload_muart_pkt_send_wire_locked(raw, raw_len);

    rt_mutex_release(&muart_send_mutex);

    return sent;
}

static rt_size_t upload_muart_pkt_send_ack_id(rt_uint8_t uart_id, rt_uint8_t ack_status, rt_uint8_t seq)
{
    union muart_pkt *pkt = &muart_tx_pkt;
    rt_uint16_t crc;
    rt_size_t sent;

    if (rt_mutex_take(&muart_send_mutex, RT_WAITING_FOREVER) != RT_EOK)
        return 0;

    muart_pkt_build_hdr(pkt, uart_id, 0, 0, RT_TRUE, ack_status, seq);
    crc = muart_pkt_calc_crc(pkt->raw, 0);
    muart_pkt_set_crc(pkt, 0, crc);

    sent = upload_muart_pkt_send_wire_locked(pkt->raw, MUART_PKT_MIN);

    rt_mutex_release(&muart_send_mutex);

    return sent;
}

static rt_bool_t upload_muart_pkt_check(uint8_t *buf, rt_size_t *buflen, rt_size_t *pkt_len)
{
    rt_size_t len, start, pos, raw_len, rest;
    rt_ssize_t flag, consumed;

    if (buf == RT_NULL || buflen == RT_NULL || pkt_len == RT_NULL)
        return RT_FALSE;

    len = *buflen;
    *pkt_len = 0;
    pos = 0;

    while (pos < len)
    {
        flag = ppp_find_flag(buf, len, pos);
        if (flag < 0)
            break;

        start = (rt_size_t)flag;

        /* 跳过连续 0x7E 空帧 */
        if (start + 1 < len && buf[start + 1] == MUART_PKT_FLAG)
        {
            pos = start + 1;
            continue;
        }

        consumed = ppp_from_wire(buf, len, start, muart_raw_decode,
                                 MUART_PKT_MAX, &raw_len);

        if (consumed == 0)
        {
            if (start > 0)
            {
                len -= start;
                rt_memmove(buf, buf + start, len);
            }
            *buflen = len;
            return RT_FALSE;
        }

        if (consumed < 0)
        {
            pos = start + 1;
            continue;
        }

        if (!muart_pkt_shape_ok(muart_raw_decode, raw_len))
        {
            pos = start + 1;
            continue;
        }

        if (!muart_pkt_crc_ok(muart_raw_decode, raw_len))
        {
            upload_muart_pkt_send_ack_id(muart_pkt_uart_id(muart_raw_decode), MUART_PKT_ACK_RETX,
                                         muart_pkt_seq(muart_raw_decode[0]));
            pos = start + 1;
            continue;
        }

        rest = len - (start + (rt_size_t)consumed);
        rt_memcpy(buf, muart_raw_decode, raw_len);
        if (rest > 0)
            rt_memmove(buf + raw_len, buf + start + (rt_size_t)consumed, rest);

        *pkt_len = raw_len;
        *buflen = raw_len + rest;
        return RT_TRUE;
    }

    if (len > 0 && buf[len - 1] == MUART_PKT_FLAG)
    {
        buf[0] = MUART_PKT_FLAG;
        *buflen = 1;
        return RT_FALSE;
    }

    *buflen = 0;
    return RT_FALSE;
}

static upload_muart_obj_t upload_muart_pkt_find_by_id(rt_uint8_t uart_id)
{
    struct upload_muart_obj *node;

    rt_enter_critical();
    rt_list_for_each_entry(node, &muart_uart_list, list)
    {
        if (node->config.number == uart_id)
        {
            rt_exit_critical();
            return node;
        }
    }
    rt_exit_critical();

    return RT_NULL;
}

static rt_size_t upload_muart_pkt_send(upload_muart_obj_t obj, const uint8_t *buf, rt_size_t len,
                                       rt_size_t residue, rt_bool_t is_ack, rt_uint8_t ack_status,
                                       rt_uint8_t seq)
{
    rt_size_t wire_size;
    rt_size_t sent;

    if (obj == RT_NULL || len > MUART_PKT_PAYLOAD_MAX)
        return 0;

    if (buf == RT_NULL && len != 0)
        return 0;

    if (rt_mutex_take(&muart_send_mutex, RT_WAITING_FOREVER) != RT_EOK)
        return 0;

    if (!muart_pkt_build(&muart_tx_pkt, obj->config.number, buf, (rt_uint16_t)len, (rt_uint8_t)residue, is_ack, ack_status, seq))
    {
        rt_mutex_release(&muart_send_mutex);
        return 0;
    }

    wire_size = muart_pkt_wire_size_by_len((rt_uint16_t)len);
    sent = upload_muart_pkt_send_wire_locked(muart_tx_pkt.raw, wire_size);

    rt_mutex_release(&muart_send_mutex);

    return sent;
}

static rt_err_t upload_muart_parent_rx_ind(rt_device_t dev, rt_size_t size)
{
    UNUSED(dev);
    UNUSED(size);

    rt_sem_release(&muart_rx_sem);

    return RT_EOK;
}

static void upload_muart_recv_sub_event(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    upload_muart_obj_t obj = (upload_muart_obj_t)parameter;
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

static double_ret upload_muart_set_param(upload_obj_t self, void *config)
{
    double_ret result = {0, 0};

    if (self == RT_NULL || config == RT_NULL) 
    {
        result.fail++;
        return result;
    }

    upload_muart_config_t cfg = (upload_muart_config_t)config;
    upload_muart_obj_t obj = (upload_muart_obj_t)self;
    upload_muart_config_t obj_cfg = &(obj->config);

    obj_cfg->number = cfg->number;
    result.succ++;

    return result;
}

static rt_ssize_t upload_muart_send(upload_obj_t self, const void *buffer, rt_size_t size)
{
    if (self == RT_NULL || buffer == RT_NULL || size == 0)
        return 0;

    upload_muart_obj_t obj = (upload_muart_obj_t)self;
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

static void upload_muart_parent_handle_rx(const uint8_t *pkt, rt_size_t pkt_len)
{
    upload_muart_obj_t node;
    rt_uint8_t uart_id;
    rt_uint8_t seq;
    rt_size_t payload_len;

    uart_id = muart_pkt_uart_id(pkt);
    seq = muart_pkt_seq(pkt[0]);

    if (muart_pkt_is_ack_byte(pkt[0]))
    {
        node = upload_muart_pkt_find_by_id(uart_id);
        if (node == RT_NULL)
            return;

        if (seq != node->seq)
        {
            LOG_W("muart ack seq mismatch, expect %d got %d", node->seq, seq);
            return;
        }

        node->ack_status = muart_pkt_ack_status(pkt[0]);
        rt_sem_release(node->ack_sem);
        return;
    }

    node = upload_muart_pkt_find_by_id(uart_id);
    if (node == RT_NULL)
    {
        LOG_W("muart rx unknown uart_id %d, ack skip", uart_id);
        upload_muart_pkt_send_ack_id(uart_id, MUART_PKT_ACK_SKIP, seq);
        return;
    }

    if (node->rx_seq != MUART_PKT_RX_SEQ_NONE && seq == node->rx_seq)
    {
        LOG_D("muart rx duplicate seq %d, ignore", seq);
        upload_muart_pkt_send(node, RT_NULL, 0, 0, RT_TRUE, MUART_PKT_ACK_NORMAL, seq);
        return;
    }

    payload_len = pkt_len - MUART_PKT_MIN;
    upload_muart_send((upload_obj_t)node, muart_pkt_payload_ptr(pkt), payload_len);
    upload_muart_pkt_send(node, RT_NULL, 0, 0, RT_TRUE, MUART_PKT_ACK_NORMAL, seq);
    node->rx_seq = seq;
}

static void upload_muart_parent_recv(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    upload_muart_obj_t obj = (upload_muart_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;

    static uint8_t muart_recv_buffer[MUART_BUFSZ];
    static rt_size_t muart_recv_index = 0;

    while (1)
    {
        thread2_idle_flag = RT_TRUE;
        if (rt_sem_take(&muart_rx_sem, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s take muart_rx_sem error", rt_obj->name);
            break;
        }
        thread2_idle_flag = RT_FALSE;

        rt_size_t len;
        while (muart_recv_index < sizeof(muart_recv_buffer) &&
               (len = rt_device_read(muart, 0, muart_recv_buffer + muart_recv_index,
                                      sizeof(muart_recv_buffer) - muart_recv_index)) > 0)
        {
            // LOG_D("%s recv len: %d", rt_obj->name, len);
            muart_recv_index += len;
        }

        if (muart_recv_index >= sizeof(muart_recv_buffer))
        {
            LOG_W("%s rx buffer full, reset", rt_obj->name);
            muart_recv_index = 0;
            continue;
        }

        while (muart_recv_index >= 3)
        {
            rt_size_t pkt_len = 0;

            if (!upload_muart_pkt_check(muart_recv_buffer, &muart_recv_index, &pkt_len))
                break;

            upload_muart_parent_handle_rx(muart_recv_buffer, pkt_len);

            muart_recv_index -= pkt_len;
            if (muart_recv_index > 0)
                rt_memmove(muart_recv_buffer, muart_recv_buffer + pkt_len, muart_recv_index);
        }
    }
}

static rt_size_t upload_muart_parent_send(upload_muart_obj_t obj, const void *buffer, rt_size_t size)
{
    if (obj == RT_NULL || buffer == RT_NULL || size == 0)
        return 0;

    const uint8_t *data = (const uint8_t *)buffer;
    rt_size_t offset = 0;
    rt_size_t send_total = 0;
    rt_size_t send_pkt_total = (size + MUART_PKT_PAYLOAD_MAX - 1) / MUART_PKT_PAYLOAD_MAX;
    rt_size_t send_pkt_index = 1;

    while (offset < size)
    {
        rt_size_t chunk = size - offset;
        if (chunk > MUART_PKT_PAYLOAD_MAX)
            chunk = MUART_PKT_PAYLOAD_MAX;

        rt_bool_t pkt_done = RT_FALSE;
        rt_uint8_t retry = 3;

        while (!pkt_done && retry > 0)
        {
            while (rt_sem_take(obj->ack_sem, 0) == RT_EOK)
                ;
            obj->ack_status = MUART_PKT_ACK_NORMAL;

            upload_muart_pkt_send(obj, data + offset, chunk, send_pkt_total - send_pkt_index, RT_FALSE, 0, obj->seq);

            if (rt_sem_take(obj->ack_sem, 3000) != RT_EOK)
            {
                LOG_W("%s sub-pkt ack timeout, retry %d", ((rt_object_t)obj)->name, retry);
                retry--;
                continue;
            }

            switch (obj->ack_status)
            {
            case MUART_PKT_ACK_NORMAL:
                pkt_done = RT_TRUE;
                obj->seq = muart_pkt_seq_next(obj->seq);
                break;
            case MUART_PKT_ACK_RETX:
                LOG_W("%s sub-pkt ack retx, resend now, retry %d", ((rt_object_t)obj)->name, retry);
                retry--;
                break;
            case MUART_PKT_ACK_SKIP:
                LOG_W("%s sub-pkt ack skip", ((rt_object_t)obj)->name);
                pkt_done = RT_TRUE;
                obj->seq = muart_pkt_seq_next(obj->seq);
                break;
            default:
                LOG_W("%s sub-pkt ack unknown status %d", ((rt_object_t)obj)->name, obj->ack_status);
                retry--;
                break;
            }
        }

        if (!pkt_done)
        {
            LOG_E("%s sub-pkt send failed at offset %d", ((rt_object_t)obj)->name, offset);
            return send_total;
        }

        // LOG_D("%s sub-pkt %d/%d send: %d", ((rt_object_t)obj)->name, send_pkt_index, send_pkt_total, chunk);
        offset += chunk;
        send_total += chunk;
        send_pkt_index++;
    }

    return send_total;
}

static rt_err_t upload_muart_handle_rx_bind_pre(upload_obj_t self, const void *buffer, rt_size_t size)
{
    if (self == RT_NULL || buffer == RT_NULL || size == 0) {
        return 0;
    }

    upload_muart_obj_t obj = (upload_muart_obj_t)self;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    rt_size_t sent;

    sent = upload_muart_parent_send(obj, buffer, size);
    custom_obj_bind_ext_add_send(custom_obj, sent, RT_TRUE);

    return RT_EOK;
}

static void upload_muart_recv(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    upload_muart_obj_t obj = (upload_muart_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    upload_obj_t upload_obj = (upload_obj_t)obj;

    if(custom_obj->rxb.sub_mb == RT_NULL)
        return;

    while (1)
    {
        obj->thread1_idle_flag = RT_TRUE;
        rt_uint32_t recv_event_flag = 0;
        if(rt_mb_recv(custom_obj->rxb.sub_mb, (rt_ubase_t *)&recv_event_flag, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s recv rxb.sub_mb error", rt_obj->name);
            break;
        }
        obj->thread1_idle_flag = RT_FALSE;
        
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

            upload_muart_handle_rx_bind_pre(upload_obj, rx_custom_obj->rxb.buffer, rx_custom_obj->rxb.len);

            LOG_I("%s ---> %s send rxb.pub_succ_event: 0x%X\r\n", rt_obj->name, rx_custom_obj->parent.name, custom_obj->event_flag);
            if( rt_event_send(rx_custom_obj->rxb.pub_succ_event, custom_obj->event_flag) != RT_EOK )
                LOG_E("%s send rx_pub_succ_event error: %d/%d\r\n", rt_obj->name, custom_obj->event_flag, rx_custom_obj->rxb.next_event_mask);
        }

        rt_mutex_release(custom_obj->rxb.bind_mutex);
    }
}
 
static rt_err_t upload_muart_delete(upload_obj_t self)
{
    if (self == RT_NULL) {
        return -RT_EINVAL;
    }

    upload_muart_obj_t obj = (upload_muart_obj_t)self;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;

    if(rt_object_find(rt_obj->name, RT_Object_Class_Custom) != RT_NULL)
        rt_object_detach(rt_obj);

    bind_delete_all_for_obj(custom_obj);

    rt_enter_critical();
    struct upload_muart_obj *muart_obj, *next_muart_obj;
    rt_list_for_each_entry_safe(muart_obj, next_muart_obj, &muart_uart_list, list)
    {
        if(muart_obj == obj)
            rt_list_remove(&muart_obj->list);
    }
    rt_exit_critical();

    if (obj->rx_sub_event_tid != RT_NULL)
    {
        rt_thread_delete(obj->rx_sub_event_tid);
        obj->rx_sub_event_tid = RT_NULL;
    }

    while(obj->rx_tid != RT_NULL && obj->thread1_idle_flag == RT_FALSE)
    {
        rt_thread_mdelay(20);
    }

    while(thread2_idle_flag == RT_FALSE)
    {
        rt_thread_mdelay(20);
    }

    if (obj->rx_tid != RT_NULL)
    {
        rt_thread_delete(obj->rx_tid);
        obj->rx_tid = RT_NULL;
    }
    
    if(obj->ack_sem != RT_NULL)
    {
        rt_sem_delete(obj->ack_sem);
        obj->ack_sem = RT_NULL;
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

static upload_obj_t  upload_muart_create(upload_obj_t self, char *name, void *config)
{
    if (self == RT_NULL || name == RT_NULL) {
        LOG_E("create error: param");
        return RT_NULL;
    }

    if (rt_strncmp(self->parent.parent.name, UPLOAD_MUART_PARENT_NAME, RT_NAME_MAX) != 0) {
        LOG_E("create error: invalid parent");
        return RT_NULL;
    }

    if (rt_object_find(name, RT_Object_Class_Custom) != RT_NULL)
    {
        LOG_E("create error: name exist");
        return RT_NULL;
    }

    upload_muart_obj_t obj = rt_malloc(sizeof(struct upload_muart_obj));
    if (obj == RT_NULL) {
        LOG_E("create error: memory allocation");
        return RT_NULL;
    }
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    upload_obj_t upload_obj = (upload_obj_t)obj;

    rt_memcpy(obj, self, sizeof(struct upload_muart_obj));

    if(config == RT_NULL)
        config = &(obj->config);

    double_ret result = upload_muart_set_param(upload_obj, config);
    if (result.fail != 0)
    {
        LOG_E("create error: set param");
        goto exit;
    }

    char name_buf[RT_NAME_MAX] = {0};

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_recv");
    obj->rx_tid = rt_thread_create(name_buf, upload_muart_recv, obj, 1024, CUSTOM_THREAD_PRIORITY, 20);
    if (obj->rx_tid == RT_NULL) {
        LOG_E("create error: thread create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_sub_event");
    if((obj->rx_sub_event_tid = rt_thread_create(name_buf, upload_muart_recv_sub_event, obj, 1024, CUSTOM_THREAD_PRIORITY - 1, 20)) == RT_NULL) {
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

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_ack");
    if((obj->ack_sem = rt_sem_create(name_buf, 0, RT_IPC_FLAG_FIFO)) == RT_NULL) {
        LOG_E("create error: ack sem create");
        goto exit;
    }

    obj->ack_status = MUART_PKT_ACK_NORMAL;
    obj->seq = 0;
    obj->rx_seq = MUART_PKT_RX_SEQ_NONE;

    rt_thread_startup(obj->rx_tid);
    rt_thread_startup(obj->rx_sub_event_tid);

    rt_enter_critical();
    rt_list_insert_after(&muart_uart_list, &obj->list);
    rt_exit_critical();

    rt_object_init(rt_obj, RT_Object_Class_Custom, name);

    LOG_I("create success: %s", name);
    return upload_obj;

exit:
    upload_muart_delete(upload_obj);
    return RT_NULL;
}

static rt_size_t upload_muart_get_history(upload_obj_t self, rt_uint8_t *buffer, rt_size_t size, event_history_t event_history)
{
    RT_UNUSED(self);
    RT_UNUSED(buffer);
    RT_UNUSED(size);
    RT_UNUSED(event_history);
    return 0;
}

static const struct upload_obj_ops upload_muart_ops =
{
    .create = upload_muart_create,
    .delete = upload_muart_delete,
    .set_param = upload_muart_set_param,
    .get_history = upload_muart_get_history,
};

static rt_err_t  upload_muart_gc(custom_obj_t self)
{
    if (self == RT_NULL)
        return -RT_EINVAL;

    upload_obj_t upload_obj = (upload_obj_t)self;
    return upload_muart_delete(upload_obj);
}

static const struct custom_object_ops custom_obj_ops =
{
    .gc = upload_muart_gc,
};

static int upload_muart_init(void)
{
    static struct upload_muart_obj object = {0};
    upload_muart_obj_t obj = &object;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    upload_obj_t upload_obj = (upload_obj_t)obj;

    if (rt_object_find(UPLOAD_MUART_PARENT_NAME, RT_Object_Class_Custom) != RT_NULL)
        return -RT_ERROR;

    custom_obj->type = Custom_Object_Class_Upload;

    obj->thread1_idle_flag = RT_FALSE;

    upload_obj_init(custom_obj);

    custom_obj->ops = &custom_obj_ops;

    upload_obj->ops = &upload_muart_ops;

    // rt_pin_mode(GET_PIN(F, 2), PIN_MODE_OUTPUT);
    // rt_pin_write(GET_PIN(F, 2), PIN_HIGH);
    // muart = rt_device_find("uart3");

    // rt_pin_mode(GET_PIN(F, 3), PIN_MODE_OUTPUT);
    // rt_pin_write(GET_PIN(F, 3), PIN_HIGH);
    // muart = rt_device_find("uart6");

    muart = rt_device_find("uart5");
    if (muart == RT_NULL) 
        return -RT_ERROR;

    if (muart->ref_count != 0)
    {
        LOG_E("init error: muart ref_count is not 0");
        return -RT_ERROR;
    }

    struct serial_configure serial_cfg;
    serial_cfg.baud_rate = BAUD_RATE_2000000;
    serial_cfg.data_bits = DATA_BITS_8;
    serial_cfg.stop_bits = STOP_BITS_1;
    serial_cfg.parity = PARITY_NONE;
    serial_cfg.bit_order = BIT_ORDER_LSB;
    serial_cfg.rx_bufsz = MUART_BUFSZ;
    #ifdef RT_SERIAL_USING_DMA
        serial_cfg.dma_ping_bufsz = serial_cfg.rx_bufsz / 2;
    #endif
    serial_cfg.tx_bufsz = MUART_BUFSZ;

    rt_err_t err = rt_device_control(muart, RT_DEVICE_CTRL_CONFIG, &serial_cfg);
    if (err != RT_EOK) {
        LOG_E("init error: failed to set serial config");
        return -RT_ERROR;
    }

    err = rt_mutex_init(&muart_send_mutex, "muart_send_mutex", RT_IPC_FLAG_PRIO);
    if (err != RT_EOK)
    {
        LOG_E("init error: failed to init muart send mutex");
        return -RT_ERROR;
    }

    err = rt_sem_init(&muart_rx_sem, "muart_rx_sem", 0, RT_IPC_FLAG_FIFO);
    if (err != RT_EOK)    
    {
        LOG_E("init error: failed to init muart rx semaphore");
        rt_mutex_detach(&muart_send_mutex);
        return -RT_ERROR;
    }

    rt_list_init(&muart_uart_list);

    rt_thread_t tid = rt_thread_create("upload_muart_parent_recv",
                                     upload_muart_parent_recv,
                                     obj,
                                     1024,
                                     CUSTOM_THREAD_PRIORITY,
                                     20);
    if (tid == RT_NULL)
    {
        LOG_E("init error: failed to create upload_muart_parent_recv thread");
        rt_sem_detach(&muart_rx_sem);
        rt_mutex_detach(&muart_send_mutex);
        return -RT_ERROR;
    }
    rt_thread_startup(tid);

    rt_device_set_rx_indicate(muart, upload_muart_parent_rx_ind);

    if (rt_device_open(muart, RT_DEVICE_OFLAG_RDWR | RT_SERIAL_RX_NON_BLOCKING | RT_SERIAL_TX_BLOCKING) != RT_EOK)
    {
        LOG_E("init error: failed to open muart");
        rt_device_set_rx_indicate(muart, RT_NULL);
        rt_sem_detach(&muart_rx_sem);
        rt_mutex_detach(&muart_send_mutex);
        rt_thread_delete(tid);
        return -RT_ERROR;
    }

    rt_object_init(rt_obj, RT_Object_Class_Custom, UPLOAD_MUART_PARENT_NAME);

    return RT_EOK;
}

INIT_ENV_EXPORT(upload_muart_init);

static int cmd_create_upload_muart(int argc, char *argv[])
{
    if (argc < 2)
    {
        rt_kprintf("Usage: create_upload_muart <name> [-n number]\n");
        rt_kprintf("Example: create_upload_muart upload_muart_1 -n 1\n");
        return -RT_ERROR;
    }

    const char *name = argv[1];
    rt_err_t err = custom_obj_msh_check_create_name(name, "upload_muart");

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

    upload_obj_t parent = (upload_obj_t)rt_object_find(UPLOAD_MUART_PARENT_NAME, RT_Object_Class_Custom);
    if (parent == RT_NULL || parent->ops == RT_NULL || parent->ops->create == RT_NULL)
    {
        rt_kprintf("Parent not ready: %s\n", UPLOAD_MUART_PARENT_NAME);
        return -RT_ERROR;
    }

    struct upload_muart_config *config_ptr = RT_NULL;
    struct upload_muart_config config = {0};
    const char *number_str = custom_obj_msh_get_opt(argc, argv, "n");

    if (number_str != RT_NULL)
    {
        config_ptr = &config;
        config.number = (rt_uint8_t)strtoul(number_str, RT_NULL, 10);
    }

    if (parent->ops->create(parent, (char *)name, config_ptr) == RT_NULL)
    {
        rt_kprintf("Failed to create upload_muart: %s\n", name);
        return -RT_ERROR;
    }

    rt_kprintf("Created upload_muart: %s\n", name);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_create_upload_muart, create_upload_muart, Create upload_muart object);

static int cmd_delete_upload_muart(int argc, char *argv[])
{
    const char *name = custom_obj_msh_get_opt(argc, argv, "n");

    if (name == RT_NULL)
    {
        rt_kprintf("Usage: delete_upload_muart -n <name>\n");
        rt_kprintf("Example: delete_upload_muart -n upload_muart_1\n");
        return -RT_ERROR;
    }

    if (custom_obj_msh_check_delete_name(name) != RT_EOK)
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
        rt_kprintf("Not an upload_muart object: %s\n", name);
        return -RT_ERROR;
    }

    if (obj->ops == RT_NULL || obj->ops->delete == RT_NULL)
    {
        rt_kprintf("Delete not supported: %s\n", name);
        return -RT_ERROR;
    }

    if (obj->ops->delete(obj) != RT_EOK)
    {
        rt_kprintf("Failed to delete upload_muart: %s\n", name);
        return -RT_ERROR;
    }

    rt_kprintf("Deleted upload_muart: %s\n", name);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_delete_upload_muart, delete_upload_muart, Delete upload_muart object);