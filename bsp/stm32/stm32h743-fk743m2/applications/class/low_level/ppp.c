#include "ppp.h"

#define DBG_TAG "ppp"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* -------------------------------------------------------------------------- */
/* 1. ppp逻辑包                                                                */
/* -------------------------------------------------------------------------- */

static void ppp_pkt_info_set(union ppp_pkt *ppp, rt_uint8_t residue, rt_uint16_t data_len)
{
    rt_uint16_t word = (rt_uint16_t)(((residue & PPP_PKT_RESIDUE_MAX) << PPP_PKT_INFO_RESIDUE_SHIFT)
                                   | (data_len & PPP_PKT_INFO_LEN_MASK));

    ppp->pkt.info.hi = (rt_uint8_t)(word >> 8);
    ppp->pkt.info.lo = (rt_uint8_t)word;
}

static rt_uint16_t ppp_pkt_info_word(const union ppp_pkt *ppp)
{
    return (rt_uint16_t)(((rt_uint16_t)ppp->pkt.info.hi << 8) | ppp->pkt.info.lo);
}

static rt_uint16_t ppp_pkt_data_len(const union ppp_pkt *ppp)
{
    return ppp_pkt_info_word(ppp) & PPP_PKT_INFO_LEN_MASK;
}

static rt_uint8_t ppp_pkt_residue(const union ppp_pkt *ppp)
{
    return (rt_uint8_t)(ppp_pkt_info_word(ppp) >> PPP_PKT_INFO_RESIDUE_SHIFT);
}

static rt_uint16_t crc16_modbus(const uint8_t *buf, rt_size_t len)
{
    rt_uint16_t crc = 0xFFFF;
    rt_size_t i;
    int j;

    for (i = 0; i < len; i++)
    {
        crc ^= buf[i];
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }

    return crc;
}

static rt_uint16_t ppp_pkt_crc(const union ppp_pkt *ppp, rt_uint16_t data_len)
{
    return crc16_modbus(ppp->raw, PPP_PKT_HDR_SIZE + data_len);
}

static void ppp_pkt_put_crc(union ppp_pkt *ppp, rt_uint16_t data_len)
{
    rt_uint16_t crc = ppp_pkt_crc(ppp, data_len);
    rt_uint8_t *p = &ppp->raw[PPP_PKT_HDR_SIZE + data_len];

    p[0] = (rt_uint8_t)(crc >> 8);
    p[1] = (rt_uint8_t)crc;
}

/* 组一帧逻辑包，成功返回帧长，失败返回 0 */
static rt_size_t ppp_pkt_make(union ppp_pkt *ppp, rt_uint8_t link_id, rt_uint8_t seq,
                              rt_bool_t is_ack, rt_uint8_t status,
                              rt_uint8_t residue, const uint8_t *payload, rt_uint16_t data_len)
{
    if (ppp == RT_NULL || data_len > PPP_PKT_PAYLOAD_MAX)
        return 0;
    if (data_len > 0 && payload == RT_NULL)
        return 0;

    ppp->pkt.ctrl.byte = 0;
    ppp->pkt.ctrl.bits.seq = seq & PPP_PKT_SEQ_MASK;
    ppp->pkt.ctrl.bits.is_ack = is_ack ? 1 : 0;
    ppp->pkt.ctrl.bits.status = status & 0x07;
    ppp->pkt.link_id = link_id;
    ppp_pkt_info_set(ppp, residue, data_len);

    if (data_len > 0)
        rt_memcpy(ppp->pkt.data, payload, data_len);

    ppp_pkt_put_crc(ppp, data_len);
    return (rt_size_t)PPP_PKT_MIN + data_len;
}

static rt_bool_t ppp_pkt_shape_ok(const union ppp_pkt *ppp, rt_size_t pkt_len)
{
    if (ppp == RT_NULL || pkt_len < PPP_PKT_MIN || pkt_len > PPP_PKT_MAX)
        return RT_FALSE;

    return pkt_len == ((rt_size_t)PPP_PKT_MIN + ppp_pkt_data_len(ppp));
}

static rt_bool_t ppp_pkt_crc_ok(const union ppp_pkt *ppp)
{
    rt_uint16_t data_len = ppp_pkt_data_len(ppp);
    const rt_uint8_t *p = &ppp->raw[PPP_PKT_HDR_SIZE + data_len];
    rt_uint16_t crc_rx = (rt_uint16_t)(((rt_uint16_t)p[0] << 8) | p[1]);

    return ppp_pkt_crc(ppp, data_len) == crc_rx;
}

/* -------------------------------------------------------------------------- */
/* 2. 线路成帧：0x7E 定界，0x7D 转义                                             */
/* -------------------------------------------------------------------------- */

/* 逻辑包 → 线路帧。成功返回线路长度，失败 -1 */
static rt_ssize_t ppp_to_wire(const union ppp_pkt *ppp, rt_size_t raw_len,
                              uint8_t *wire, rt_size_t wire_cap)
{
    rt_size_t i, off;

    if (ppp == RT_NULL || wire == RT_NULL ||
        raw_len < PPP_PKT_MIN || raw_len > PPP_PKT_MAX || wire_cap < 3)
        return -1;

    wire[0] = PPP_FLAG;
    off = 1;

    for (i = 0; i < raw_len; i++)
    {
        uint8_t b = ppp->raw[i];

        if (b == PPP_FLAG || b == PPP_ESC)
        {
            if (off + 3 > wire_cap)
                return -1;
            wire[off++] = PPP_ESC;
            wire[off++] = (uint8_t)(b ^ PPP_XOR);
        }
        else
        {
            if (off + 2 > wire_cap)
                return -1;
            wire[off++] = b;
        }
    }

    wire[off++] = PPP_FLAG;
    return (rt_ssize_t)off;
}

/*
 * 从 buf[0]==0x7E 解一帧到 ppp。
 * >0 已消费字节，0 数据不够，<0 帧错误。
 */
static rt_ssize_t ppp_from_wire(const uint8_t *wire, rt_size_t wire_len,
                                union ppp_pkt *ppp, rt_size_t *raw_len)
{
    rt_size_t i, o;

    if (wire == RT_NULL || ppp == RT_NULL || raw_len == RT_NULL || wire_len < 2)
        return 0;
    if (wire[0] != PPP_FLAG)
        return -1;

    o = 0;
    i = 1;
    while (i < wire_len)
    {
        if (wire[i] == PPP_FLAG)
        {
            if (o < PPP_PKT_MIN)
                return -1;
            *raw_len = o;
            return (rt_ssize_t)(i + 1);
        }

        if (wire[i] == PPP_ESC)
        {
            if (i + 1 >= wire_len)
                return 0;
            if (wire[i + 1] != PPP_ESC_FLAG && wire[i + 1] != PPP_ESC_ESC)
                return -1;
            if (o >= PPP_PKT_MAX)
                return -1;
            ppp->raw[o++] = (uint8_t)(wire[i + 1] ^ PPP_XOR);
            i += 2;
            continue;
        }

        if (o >= PPP_PKT_MAX)
            return -1;
        ppp->raw[o++] = wire[i++];
    }

    return 0;
}

static rt_ssize_t ppp_find_flag(const uint8_t *buf, rt_size_t len)
{
    rt_size_t i;

    for (i = 0; i < len; i++)
    {
        if (buf[i] == PPP_FLAG)
            return (rt_ssize_t)i;
    }
    return -1;
}

static void ppp_buf_drop(uint8_t *buf, rt_size_t *len, rt_size_t n)
{
    if (n >= *len)
    {
        *len = 0;
        return;
    }

    *len -= n;
    if (*len > 0)
        rt_memmove(buf, buf + n, *len);
}

/* -------------------------------------------------------------------------- */
/* 3. 会话 / 组包                                                               */
/* -------------------------------------------------------------------------- */

rt_err_t ppp_link_init(struct ppp_link *link, rt_uint8_t id)
{
    char name[RT_NAME_MAX];

    if (link == RT_NULL)
        return -RT_EINVAL;

    if (link->ack_sem != RT_NULL || link->asm_buf != RT_NULL)
        ppp_link_deinit(link);

    rt_snprintf(name, sizeof(name), "ppp%u_ack", (unsigned)id);
    link->ack_sem = rt_sem_create(name, 0, RT_IPC_FLAG_FIFO);
    if (link->ack_sem == RT_NULL)
        return -RT_ENOMEM;

    link->id = id;
    link->ack_status = PPP_PKT_ACK_NORMAL;
    link->seq = 0;
    link->rx_seq = PPP_PKT_RX_SEQ_NONE;
    link->asm_buf = RT_NULL;
    link->asm_cap = 0;
    link->asm_len = 0;
    link->asm_next_residue = PPP_ASM_IDLE;
    return RT_EOK;
}

void ppp_link_deinit(struct ppp_link *link)
{
    if (link == RT_NULL)
        return;

    if (link->ack_sem != RT_NULL)
    {
        rt_sem_delete(link->ack_sem);
        link->ack_sem = RT_NULL;
    }

    if (link->asm_buf != RT_NULL)
    {
        rt_free(link->asm_buf);
        link->asm_buf = RT_NULL;
    }
    link->asm_cap = 0;
    link->asm_len = 0;
    link->asm_next_residue = PPP_ASM_IDLE;
}

static void ppp_link_asm_reset(struct ppp_link *link)
{
    link->asm_len = 0;
    link->asm_next_residue = PPP_ASM_IDLE;
}

static void ppp_link_asm_free(struct ppp_link *link)
{
    if (link->asm_buf != RT_NULL)
    {
        rt_free(link->asm_buf);
        link->asm_buf = RT_NULL;
    }
    link->asm_cap = 0;
    ppp_link_asm_reset(link);
}

/* 首包按 residue 申请：本包 len + 后续 residue 包 * PAYLOAD_MAX */
static rt_bool_t ppp_link_asm_alloc(struct ppp_link *link, rt_size_t first_len, rt_uint8_t residue)
{
    rt_size_t need = first_len + (rt_size_t)residue * PPP_PKT_PAYLOAD_MAX;

    if (need == 0 || need > PPP_PKT_ASM_MAX)
        return RT_FALSE;

    if (link->asm_buf != RT_NULL)
    {
        if (link->asm_cap >= need)
            return RT_TRUE;
        rt_free(link->asm_buf);
        link->asm_buf = RT_NULL;
        link->asm_cap = 0;
    }

    link->asm_buf = rt_malloc(need);
    if (link->asm_buf == RT_NULL)
        return RT_FALSE;

    link->asm_cap = need;
    return RT_TRUE;
}

static rt_bool_t ppp_link_asm_append(struct ppp_link *link, const uint8_t *data, rt_size_t len)
{
    if (len == 0)
        return RT_TRUE;

    if (link->asm_buf == RT_NULL || link->asm_len + len > link->asm_cap)
        return RT_FALSE;

    rt_memcpy(link->asm_buf + link->asm_len, data, len);
    link->asm_len += len;
    return RT_TRUE;
}

static void ppp_deliver(struct ppp_adapter *adp, struct ppp_link *link,
                        const uint8_t *payload, rt_size_t len)
{
    if (adp->ops != RT_NULL && adp->ops->on_data != RT_NULL)
        adp->ops->on_data(link, payload, len);
}

/*
 * residue==0 且未在组包：一包传完，直接推 payload，不申请内存。
 * residue>0：按首包 residue 申请组包缓冲；收齐后再 on_data。
 */
static rt_bool_t ppp_link_asm_push(struct ppp_adapter *adp, struct ppp_link *link,
                                   const uint8_t *payload, rt_size_t len, rt_uint8_t residue)
{
    if (link->asm_next_residue != PPP_ASM_IDLE && residue != link->asm_next_residue)
    {
        LOG_W("ppp id=%d residue mismatch, expect %d got %d, drop",
              link->id, link->asm_next_residue, residue);
        ppp_link_asm_free(link);
    }

    if (residue == 0 && link->asm_len == 0)
    {
        ppp_deliver(adp, link, payload, len);
        return RT_TRUE;
    }

    if (link->asm_len == 0 &&
        !ppp_link_asm_alloc(link, len, residue))
    {
        LOG_E("ppp id=%d assemble alloc failed, drop", link->id);
        ppp_link_asm_free(link);
        return RT_FALSE;
    }

    if (!ppp_link_asm_append(link, payload, len))
    {
        LOG_E("ppp id=%d assemble overflow, drop", link->id);
        ppp_link_asm_free(link);
        return RT_FALSE;
    }

    if (residue == 0)
    {
        ppp_deliver(adp, link, link->asm_buf, link->asm_len);
        ppp_link_asm_reset(link);
        return RT_TRUE;
    }

    link->asm_next_residue = (rt_uint8_t)(residue - 1);
    return RT_TRUE;
}

/* -------------------------------------------------------------------------- */
/* 4. 发送：组包 → 成帧 → rt_device_write → 等 ACK                              */
/* -------------------------------------------------------------------------- */

rt_err_t ppp_adapter_init(struct ppp_adapter *adp, const char *name,
                          rt_device_t dev, const struct ppp_adapter_ops *ops)
{
    if (adp == RT_NULL || name == RT_NULL || dev == RT_NULL || ops == RT_NULL)
        return -RT_EINVAL;

    rt_memset(adp, 0, sizeof(*adp));
    adp->dev = dev;
    adp->ops = ops;
    adp->ack_timeout_ms = PPP_SEND_ACK_TIMEOUT_MS;
    adp->send_retry = PPP_SEND_RETRY;

    return rt_mutex_init(&adp->lock, name, RT_IPC_FLAG_PRIO);
}

rt_err_t ppp_adapter_deinit(struct ppp_adapter *adp)
{
    if (adp == RT_NULL)
        return -RT_EINVAL;

    return rt_mutex_detach(&adp->lock);
}

static rt_ssize_t ppp_device_write_all(rt_device_t dev, const void *buf, rt_size_t size)
{
    const uint8_t *p = (const uint8_t *)buf;
    rt_ssize_t send_len = 0;
    rt_uint8_t retry = 0;

    if (dev == RT_NULL || buf == RT_NULL || size == 0)
        return 0;

    while ((rt_size_t)send_len < size)
    {
        rt_ssize_t n = rt_device_write(dev, 0, p + send_len, size - (rt_size_t)send_len);

        if (n > 0)
        {
            send_len += n;
            retry = 0;
            continue;
        }

        if (++retry > 50)
            break;
        rt_thread_mdelay(20);
    }

    return send_len;
}

/* 组 adp->tx，成帧后写出。内部加锁。成功返回逻辑包长度 */
static rt_size_t ppp_adapter_out(struct ppp_adapter *adp, rt_uint8_t id, rt_uint8_t seq,
                                 rt_bool_t is_ack, rt_uint8_t status,
                                 rt_uint8_t residue, const uint8_t *payload, rt_uint16_t data_len)
{
    rt_size_t raw_len = 0;
    rt_ssize_t wire_len;
    rt_ssize_t n;

    if (rt_mutex_take(&adp->lock, RT_WAITING_FOREVER) != RT_EOK)
        return 0;

    raw_len = ppp_pkt_make(&adp->ppp_tx, id, seq, is_ack, status, residue, payload, data_len);
    if (raw_len != 0)
    {
        wire_len = ppp_to_wire(&adp->ppp_tx, raw_len, adp->wire, sizeof(adp->wire));
        if (wire_len > 0)
        {
            n = ppp_device_write_all(adp->dev, adp->wire, (rt_size_t)wire_len);
            if ((rt_size_t)n != (rt_size_t)wire_len)
                raw_len = 0;
        }
        else
        {
            raw_len = 0;
        }
    }

    rt_mutex_release(&adp->lock);
    return raw_len;
}

static rt_size_t ppp_adapter_send_ack(struct ppp_adapter *adp, rt_uint8_t id,
                                      rt_uint8_t ack_status, rt_uint8_t seq)
{
    return ppp_adapter_out(adp, id, seq, RT_TRUE, ack_status, 0, RT_NULL, 0);
}

static void ppp_link_flush_ack(struct ppp_link *link)
{
    if (link->ack_sem == RT_NULL)
        return;

    while (rt_sem_take(link->ack_sem, 0) == RT_EOK)
        ;
}

static rt_uint8_t ppp_link_wait_ack(struct ppp_link *link, rt_int32_t timeout)
{
    if (link->ack_sem == RT_NULL)
        return PPP_ACK_TIMEOUT;
    if (rt_sem_take(link->ack_sem, rt_tick_from_millisecond(timeout)) != RT_EOK)
        return PPP_ACK_TIMEOUT;

    return link->ack_status;
}

/* 发一子包并等 ACK。成功则 seq++ */
static rt_bool_t ppp_adapter_send_one(struct ppp_adapter *adp, struct ppp_link *link,
                                      const uint8_t *data, rt_uint16_t chunk, rt_uint8_t residue)
{
    rt_uint8_t retry = adp->send_retry;

    while (retry > 0)
    {
        rt_uint8_t ack;

        ppp_link_flush_ack(link);
        link->ack_status = PPP_PKT_ACK_NORMAL;
        if (ppp_adapter_out(adp, link->id, link->seq, RT_FALSE, PPP_PKT_SEND_NORMAL,
                            residue, data, chunk) == 0)
        {
            LOG_W("ppp id=%d tx failed, retry %d", link->id, retry);
            retry--;
            continue;
        }

        ack = ppp_link_wait_ack(link, adp->ack_timeout_ms);
        if (ack == PPP_PKT_ACK_NORMAL || ack == PPP_PKT_ACK_SKIP)
        {
            if (ack == PPP_PKT_ACK_SKIP)
                LOG_W("ppp id=%d ack skip", link->id);
            link->seq = (rt_uint8_t)((link->seq + 1) & PPP_PKT_SEQ_MASK);
            return RT_TRUE;
        }

        LOG_W("ppp id=%d ack %s, retry %d",
              link->id, (ack == PPP_PKT_ACK_RETX) ? "retx" : "timeout", retry);
        retry--;
    }

    return RT_FALSE;
}

rt_size_t ppp_adapter_send(struct ppp_adapter *adp, struct ppp_link *link,
                           const uint8_t *data, rt_size_t size)
{
    rt_size_t offset = 0;

    if (adp == RT_NULL || adp->dev == RT_NULL ||
        link == RT_NULL || data == RT_NULL || size == 0)
        return 0;

    if (size > PPP_PKT_ASM_MAX)
    {
        LOG_E("ppp id=%d send too large: %d, max %d bytes",
              link->id, (int)size, PPP_PKT_ASM_MAX);
        return 0;
    }

    while (offset < size)
    {
        rt_size_t remain = size - offset;
        rt_uint16_t chunk = (remain > PPP_PKT_PAYLOAD_MAX)
                            ? PPP_PKT_PAYLOAD_MAX : (rt_uint16_t)remain;
        rt_uint8_t residue = (rt_uint8_t)((remain - chunk + PPP_PKT_PAYLOAD_MAX - 1)
                                          / PPP_PKT_PAYLOAD_MAX);

        if (!ppp_adapter_send_one(adp, link, data + offset, chunk, residue))
        {
            LOG_E("ppp id=%d send failed at offset %d", link->id, (int)offset);
            return offset;
        }
        offset += chunk;
    }

    return offset;
}

/* -------------------------------------------------------------------------- */
/* 5. 接收：抽帧 → ACK 唤醒发送 / DATA 组包后 on_data                            */
/* -------------------------------------------------------------------------- */

/* 抽出一帧到 adp->rx。完整有效帧 RT_TRUE；不够或已丢掉无效帧 RT_FALSE */
static rt_bool_t ppp_adapter_pull(struct ppp_adapter *adp, uint8_t *buf, rt_size_t *buflen)
{
    rt_size_t len, raw_len;
    rt_ssize_t off, n;

    if (adp == RT_NULL || buf == RT_NULL || buflen == RT_NULL)
        return RT_FALSE;

    len = *buflen;

    while(1)
    {
        off = ppp_find_flag(buf, len);
        if (off < 0)
        {
            *buflen = 0;
            return RT_FALSE;
        }
        if (off > 0)
            ppp_buf_drop(buf, &len, (rt_size_t)off);

        /* 连续 0x7E：丢掉前一个 */
        if (len >= 2 && buf[1] == PPP_FLAG)
        {
            ppp_buf_drop(buf, &len, 1);
            continue;
        }

        n = ppp_from_wire(buf, len, &adp->ppp_rx, &raw_len);
        if (n == 0)
        {
            *buflen = len;
            return RT_FALSE;
        }
        if (n < 0)
        {
            ppp_buf_drop(buf, &len, 1);
            continue;
        }

        if (!ppp_pkt_shape_ok(&adp->ppp_rx, raw_len))
        {
            ppp_buf_drop(buf, &len, (rt_size_t)n);
            continue;
        }

        if (!ppp_pkt_crc_ok(&adp->ppp_rx))
        {
            if (!adp->ppp_rx.pkt.ctrl.bits.is_ack)
                ppp_adapter_send_ack(adp, adp->ppp_rx.pkt.link_id,
                                     PPP_PKT_ACK_RETX, adp->ppp_rx.pkt.ctrl.bits.seq);
            ppp_buf_drop(buf, &len, (rt_size_t)n);
            continue;
        }

        ppp_buf_drop(buf, &len, (rt_size_t)n);
        *buflen = len;
        return RT_TRUE;
    }
}

static void ppp_dispatch_ack(struct ppp_link *link, const union ppp_pkt *ppp)
{
    rt_uint8_t seq = ppp->pkt.ctrl.bits.seq;

    if (link == RT_NULL || seq != link->seq)
    {
        if (link != RT_NULL)
            LOG_W("ppp ack seq mismatch, id=%d expect %d got %d",
                  link->id, link->seq, seq);
        return;
    }

    link->ack_status = ppp->pkt.ctrl.bits.status;
    if (link->ack_sem != RT_NULL)
        rt_sem_release(link->ack_sem);
}

static void ppp_dispatch_data(struct ppp_adapter *adp, struct ppp_link *link,
                              const union ppp_pkt *ppp)
{
    rt_uint8_t id = ppp->pkt.link_id;
    rt_uint8_t seq = ppp->pkt.ctrl.bits.seq;

    if (link == RT_NULL)
    {
        LOG_W("ppp ppp_rx unknown id %d, ack skip", id);
        ppp_adapter_send_ack(adp, id, PPP_PKT_ACK_SKIP, seq);
        return;
    }

    if (link->rx_seq != PPP_PKT_RX_SEQ_NONE && seq == link->rx_seq)
    {
        LOG_D("ppp ppp_rx duplicate seq %d, ignore", seq);
        ppp_adapter_send_ack(adp, id, PPP_PKT_ACK_NORMAL, seq);
        return;
    }

    if (!ppp_link_asm_push(adp, link, ppp->pkt.data,
                           ppp_pkt_data_len(ppp), ppp_pkt_residue(ppp)))
    {
        ppp_adapter_send_ack(adp, id, PPP_PKT_ACK_SKIP, seq);
        return;
    }

    ppp_adapter_send_ack(adp, id, PPP_PKT_ACK_NORMAL, seq);
    link->rx_seq = seq;
}

static void ppp_adapter_dispatch(struct ppp_adapter *adp)
{
    const union ppp_pkt *ppp = &adp->ppp_rx;
    struct ppp_link *link;

    if (adp->ops == RT_NULL || adp->ops->find_link == RT_NULL)
        return;

    link = adp->ops->find_link(ppp->pkt.link_id);

    if (ppp->pkt.ctrl.bits.is_ack)
        ppp_dispatch_ack(link, ppp);
    else
        ppp_dispatch_data(adp, link, ppp);
}

rt_bool_t ppp_adapter_recv(struct ppp_adapter *adp, uint8_t *buf, rt_size_t *buflen)
{
    if (!ppp_adapter_pull(adp, buf, buflen))
        return RT_FALSE;

    ppp_adapter_dispatch(adp);
    return RT_TRUE;
}

/* 测试示例（PPP 线路帧，定界符 0x7E，转义 0x7D）：
 * T0  基线明文 hello   逻辑 00 01 00 05 68 65 6C 6C 6F 5D 19   线路 7E 00 01 00 05 68 65 6C 6C 6F 5D 19 7E
 * T1  payload 含 0x7E   逻辑 00 01 00 01 7E 4C A4   线路 7E 00 01 00 01 7D 5E 4C A4 7E
 * T2  payload 含 0x7D   逻辑 00 01 00 01 7D CC 64   线路 7E 00 01 00 01 7D 5D CC 64 7E
 * T3  ACK_NORMAL seq=0 link_id=1   逻辑 80 01 00 00 24 78   线路 7E 80 01 00 00 24 78 7E
 * T4  ACK_RETX seq=0 link_id=1      逻辑 90 01 00 00 E4 7C   线路 7E 90 01 00 00 E4 7C 7E
 * T5  帧前垃圾   FF FE 7D 5E ... 7E [T0线路帧] 7E 7E ...   跳过假 7E/垃圾，正确解析 T0
 * T6  粘包   连续两帧 T0 线路帧无间隔   解析出 2 帧
 * T7  CRC错误   篡改线路帧任一数据字节   仅回 ACK_RETX（T4）
 * T8  未知uart_id 逻辑 00 02 00 03 ...   回 ACK_SKIP 逻辑 80 02 00 00 E4 83
 * T9  分包   总长>1023，residue 从 N-1 递减到 0，收齐后再 on_data 一次完整包
 */
