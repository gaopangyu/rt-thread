#include "ppp.h"
#include <string.h>

void muart_pkt_info_set(union muart_pkt *p, rt_uint8_t residue, rt_uint16_t data_len)
{
    rt_uint16_t word;

    if (p == RT_NULL)
        return;

    word = (rt_uint16_t)((((rt_uint16_t)residue & MUART_PKT_RESIDUE_MAX) << MUART_PKT_INFO_RESIDUE_SHIFT)
                       | (data_len & MUART_PKT_INFO_LEN_MASK));
    p->pkt.info.bytes.hi = (rt_uint8_t)(word >> 8);
    p->pkt.info.bytes.lo = (rt_uint8_t)(word & 0xFF);
}

rt_uint16_t muart_pkt_payload_len(const uint8_t *pkt)
{
    rt_uint16_t info = (rt_uint16_t)(((rt_uint16_t)pkt[2] << 8) | pkt[3]);

    return info & MUART_PKT_INFO_LEN_MASK;
}

rt_bool_t muart_pkt_is_ack_byte(rt_uint8_t ctrl)
{
    return (ctrl & MUART_PKT_CTRL_ACK_MASK) != 0;
}

rt_uint8_t muart_pkt_ack_status(rt_uint8_t ctrl)
{
    return (ctrl & MUART_PKT_CTRL_STATUS_MASK) >> MUART_PKT_CTRL_STATUS_SHIFT;
}

rt_uint8_t muart_pkt_seq(rt_uint8_t ctrl)
{
    return ctrl & MUART_PKT_CTRL_SEQ_MASK;
}

rt_uint8_t muart_pkt_seq_next(rt_uint8_t seq)
{
    return (rt_uint8_t)((seq + 1) & MUART_PKT_CTRL_SEQ_MASK);
}

rt_uint8_t muart_pkt_uart_id(const uint8_t *pkt)
{
    return pkt[1];
}

const uint8_t *muart_pkt_payload_ptr(const uint8_t *pkt)
{
    return pkt + MUART_PKT_BODY_HDR_SIZE;
}

rt_size_t muart_pkt_wire_size_by_len(rt_uint16_t data_len)
{
    return MUART_PKT_MIN + (data_len & MUART_PKT_INFO_LEN_MASK);
}

static rt_bool_t ppp_need_escape(uint8_t byte)
{
    return (byte == PPP_FLAG || byte == PPP_ESC);
}

static rt_ssize_t ppp_encode_byte(uint8_t *dst, rt_size_t cap, rt_size_t off, uint8_t byte)
{
    if (off >= cap)
        return -1;

    if (!ppp_need_escape(byte))
    {
        dst[off] = byte;
        return (rt_ssize_t)(off + 1);
    }

    if (off + 2 > cap)
        return -1;

    dst[off] = PPP_ESC;
    dst[off + 1] = (uint8_t)(byte ^ PPP_XOR);
    return (rt_ssize_t)(off + 2);
}

static rt_ssize_t ppp_decode_byte(uint8_t code, uint8_t *byte)
{
    if (code == PPP_ESC_FLAG)
    {
        *byte = PPP_FLAG;
        return 1;
    }

    if (code == PPP_ESC_ESC)
    {
        *byte = PPP_ESC;
        return 1;
    }

    return 0;
}

rt_ssize_t ppp_to_wire(const uint8_t *raw, rt_size_t raw_len, uint8_t *wire, rt_size_t wire_cap)
{
    rt_size_t i, off;
    rt_ssize_t next;

    if (raw == RT_NULL || wire == RT_NULL || raw_len < MUART_PKT_MIN || raw_len > MUART_PKT_MAX)
        return -1;

    if (wire_cap < 3)
        return -1;

    wire[0] = PPP_FLAG;
    off = 1;

    for (i = 0; i < raw_len; i++)
    {
        next = ppp_encode_byte(wire, wire_cap - 1, off, raw[i]);
        if (next < 0)
            return -1;
        off = (rt_size_t)next;
    }

    if (off + 1 > wire_cap)
        return -1;

    wire[off] = PPP_FLAG;
    return (rt_ssize_t)(off + 1);
}

rt_ssize_t ppp_from_wire(const uint8_t *wire, rt_size_t wire_len, rt_size_t wire_start,
                         uint8_t *raw, rt_size_t raw_cap, rt_size_t *raw_len)
{
    rt_size_t i, o;
    uint8_t byte;

    if (wire == RT_NULL || raw == RT_NULL || raw_len == RT_NULL || wire_len < wire_start + 2)
        return 0;

    if (wire[wire_start] != PPP_FLAG)
        return -1;

    if (wire_start + 1 >= wire_len)
        return 0;

    o = 0;
    i = wire_start + 1;

    while (i < wire_len)
    {
        if (wire[i] == PPP_FLAG)
        {
            if (o < MUART_PKT_MIN)
                return -1;

            *raw_len = o;
            return (rt_ssize_t)(i + 1 - wire_start);
        }

        if (wire[i] == PPP_ESC)
        {
            if (i + 1 >= wire_len)
                return 0;

            if (!ppp_decode_byte(wire[i + 1], &byte))
                return -1;

            if (o >= raw_cap)
                return -1;

            raw[o++] = byte;
            i += 2;
            continue;
        }

        if (o >= raw_cap)
            return -1;

        raw[o++] = wire[i++];
    }

    return 0;
}

rt_ssize_t ppp_find_flag(const uint8_t *buf, rt_size_t len, rt_size_t pos)
{
    for (; pos < len; pos++)
    {
        if (buf[pos] == PPP_FLAG)
            return (rt_ssize_t)pos;
    }

    return -1;
}

rt_uint16_t crc16_modbus(const uint8_t *buf, rt_size_t len)
{
    rt_uint16_t crc = 0xFFFF;
    rt_size_t i;
    int j;

    if (buf == RT_NULL || len == 0)
        return crc;

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

rt_uint16_t muart_pkt_calc_crc(const uint8_t *pkt, rt_uint16_t payload_len)
{
    return crc16_modbus(pkt, MUART_PKT_BODY_HDR_SIZE + payload_len);
}

void muart_pkt_set_crc(union muart_pkt *pkt, rt_uint16_t payload_len, rt_uint16_t crc)
{
    rt_uint8_t *crc_ptr = &pkt->raw[MUART_PKT_BODY_HDR_SIZE + payload_len];

    crc_ptr[0] = (rt_uint8_t)(crc >> 8);
    crc_ptr[1] = (rt_uint8_t)(crc & 0xFF);
}

void muart_pkt_build_hdr(union muart_pkt *pkt, rt_uint8_t uart_id,
                         rt_uint8_t residue, rt_uint16_t data_len,
                         rt_bool_t is_ack, rt_uint8_t ack_status, rt_uint8_t seq)
{
    pkt->pkt.ctrl.byte = 0;
    pkt->pkt.ctrl.bits.seq = seq & MUART_PKT_CTRL_SEQ_MASK;
    if (is_ack)
    {
        pkt->pkt.ctrl.bits.is_ack = 1;
        pkt->pkt.ctrl.bits.status = ack_status & 0x07;
    }
    else
    {
        pkt->pkt.ctrl.bits.is_ack = 0;
        pkt->pkt.ctrl.bits.status = MUART_PKT_SEND_NORMAL;
    }
    pkt->pkt.uart_id = uart_id;
    muart_pkt_info_set(pkt, residue, data_len);
}

rt_bool_t muart_pkt_build(union muart_pkt *pkt, rt_uint8_t uart_id,
                          const uint8_t *payload, rt_uint16_t data_len,
                          rt_uint8_t residue, rt_bool_t is_ack, rt_uint8_t ack_status,
                          rt_uint8_t seq)
{
    rt_uint16_t crc;

    if (pkt == RT_NULL || data_len > MUART_PKT_PAYLOAD_MAX)
        return RT_FALSE;

    if (data_len > 0 && payload == RT_NULL)
        return RT_FALSE;

    muart_pkt_build_hdr(pkt, uart_id, residue, data_len, is_ack, ack_status, seq);

    if (data_len > 0)
        rt_memcpy(pkt->pkt.data, payload, data_len);

    crc = muart_pkt_calc_crc(pkt->raw, data_len);
    muart_pkt_set_crc(pkt, data_len, crc);

    return RT_TRUE;
}

rt_bool_t muart_pkt_shape_ok(const uint8_t *pkt, rt_size_t pkt_len)
{
    rt_uint16_t payload_len;

    if (pkt == RT_NULL || pkt_len < MUART_PKT_MIN || pkt_len > MUART_PKT_MAX)
        return RT_FALSE;

    payload_len = muart_pkt_payload_len(pkt);
    if (pkt_len != muart_pkt_wire_size_by_len(payload_len))
        return RT_FALSE;

    return RT_TRUE;
}

rt_bool_t muart_pkt_crc_ok(const uint8_t *pkt, rt_size_t pkt_len)
{
    rt_uint16_t payload_len;
    rt_uint16_t crc_calc;
    rt_uint16_t crc_rx;

    payload_len = muart_pkt_payload_len(pkt);
    crc_calc = muart_pkt_calc_crc(pkt, payload_len);
    crc_rx = (rt_uint16_t)((pkt[pkt_len - 2] << 8) | pkt[pkt_len - 1]);

    return (crc_calc == crc_rx);
}

rt_bool_t muart_pkt_validate(const uint8_t *pkt, rt_size_t pkt_len)
{
    return muart_pkt_shape_ok(pkt, pkt_len) && muart_pkt_crc_ok(pkt, pkt_len);
}

// 测试示例（PPP 线路帧，定界符 0x7E，转义 0x7D）：
// T0  基线明文 hello   逻辑 00 01 00 05 68 65 6C 6C 6F 5D 19   线路 7E 00 01 00 05 68 65 6C 6C 6F 5D 19 7E
// T1  payload 含 0x7E   逻辑 00 01 00 01 7E 4C A4   线路 7E 00 01 00 01 7D 5E 4C A4 7E
// T2  payload 含 0x7D   逻辑 00 01 00 01 7D CC 64   线路 7E 00 01 00 01 7D 5D CC 64 7E
// T3  ACK_NORMAL seq=0 uart_id=1   逻辑 80 01 00 00 24 78   线路 7E 80 01 00 00 24 78 7E
// T4  ACK_RETX seq=0 uart_id=1      逻辑 90 01 00 00 E4 7C   线路 7E 90 01 00 00 E4 7C 7E
// T5  帧前垃圾   FF FE 7D 5E ... 7E [T0线路帧] 7E 7E ...   跳过假 7E/垃圾，正确解析 T0
// T6  粘包   连续两帧 T0 线路帧无间隔   解析出 2 帧
// T7  CRC错误   篡改线路帧任一数据字节   仅回 ACK_RETX（T4）
// T8  未知uart_id 逻辑 00 02 00 03 ...   回 ACK_SKIP 逻辑 80 02 00 00 E4 83