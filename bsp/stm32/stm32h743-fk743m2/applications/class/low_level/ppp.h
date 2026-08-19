#ifndef __PPP_H__
#define __PPP_H__

#include <rtthread.h>
#include <stdint.h>

/* 逻辑包限值（与协议图一致） */
#define MUART_PKT_PAYLOAD_MAX       1023    /* info bit9~0：数据长度 n */
#define MUART_PKT_RESIDUE_MAX       63      /* info bit15~10：剩余子包数 */
#define MUART_PKT_BODY_HDR_SIZE     4       /* ctrl(1) + uart_id(1) + info(2) */
#define MUART_PKT_CRC_SIZE          2
#define MUART_PKT_MIN               (MUART_PKT_BODY_HDR_SIZE + MUART_PKT_CRC_SIZE) /* 6 */
#define MUART_PKT_MAX               (MUART_PKT_MIN + MUART_PKT_PAYLOAD_MAX)         /* 1029 */

/* PPP 数据链路层：帧定界符 0x7E，转义字符 0x7D（后跟 byte^0x20） */
#define PPP_FLAG                    0x7E
#define PPP_ESC                     0x7D
#define PPP_XOR                     0x20
#define PPP_ESC_FLAG                (PPP_FLAG ^ PPP_XOR)  /* 0x5E */
#define PPP_ESC_ESC                 (PPP_ESC ^ PPP_XOR)   /* 0x5D */

#define MUART_PKT_FLAG              PPP_FLAG
#define MUART_PKT_ESC               PPP_ESC

#define MUART_PKT_WIRE_BODY_MAX     (MUART_PKT_MAX * 2)
#define MUART_PKT_WIRE_MAX          (2 + MUART_PKT_WIRE_BODY_MAX + 1) /* flag + body + flag */

#define MUART_PKT_CTRL_STATUS_MASK  0x70
#define MUART_PKT_CTRL_STATUS_SHIFT 4
#define MUART_PKT_CTRL_SEQ_MASK     0x0F
#define MUART_PKT_CTRL_ACK_MASK     0x80

#define MUART_PKT_INFO_LEN_MASK     0x03FF
#define MUART_PKT_INFO_RESIDUE_SHIFT 10

/* SEND: ctrl bit6~4 命令 */
#define MUART_PKT_SEND_NORMAL       0

/* ACK: ctrl bit6~4 状态 */
#define MUART_PKT_ACK_NORMAL        0
#define MUART_PKT_ACK_RETX          1   /* 立即重发：接收包错误 */
#define MUART_PKT_ACK_SKIP          2   /* 立即跳过：无 uart 编号 */

#define MUART_PKT_RX_SEQ_NONE       0xFF /* 尚未收到过数据包 */

/*
 * muart 多串口通信包（单 union）
 * 逻辑帧: [ctrl 1B][uart_id 1B][info 2B][data n B][crc 2B]
 * 线路帧: [0x7E][PPP_escape(逻辑帧)][0x7E]
 */
union muart_pkt
{
    rt_uint8_t raw[MUART_PKT_MAX];
    struct
    {
        union
        {
            rt_uint8_t byte;
            struct
            {
                rt_uint8_t seq    : 4;  /* bit3~0   流水号 */
                rt_uint8_t status : 3;  /* bit6~4   SEND命令/ACK状态 */
                rt_uint8_t is_ack : 1;  /* bit7     0=SEND 1=ACK */
            } bits;
        } ctrl;
        rt_uint8_t uart_id;
        union
        {
            rt_uint16_t word;           /* 勿直接赋值，见 muart_pkt_info_set() */
            struct
            {
                rt_uint16_t data_len : 10;  /* bit9~0   数据长度 n */
                rt_uint16_t residue  : 6;   /* bit15~10 剩余子包数 */
            } bits;
            struct
            {
                rt_uint8_t hi;          /* info 高字节，先发 */
                rt_uint8_t lo;          /* info 低字节，后发 */
            } bytes;
        } info;
        rt_uint8_t data[MUART_PKT_PAYLOAD_MAX];
    } __attribute__((packed)) pkt;
};

void muart_pkt_info_set(union muart_pkt *p, rt_uint8_t residue, rt_uint16_t data_len);
rt_uint16_t muart_pkt_payload_len(const uint8_t *pkt);
rt_bool_t muart_pkt_is_ack_byte(rt_uint8_t ctrl);
rt_uint8_t muart_pkt_ack_status(rt_uint8_t ctrl);
rt_uint8_t muart_pkt_seq(rt_uint8_t ctrl);
rt_uint8_t muart_pkt_seq_next(rt_uint8_t seq);
rt_uint8_t muart_pkt_uart_id(const uint8_t *pkt);
const uint8_t *muart_pkt_payload_ptr(const uint8_t *pkt);
rt_size_t muart_pkt_wire_size_by_len(rt_uint16_t data_len);

rt_uint16_t crc16_modbus(const uint8_t *buf, rt_size_t len);
rt_uint16_t muart_pkt_calc_crc(const uint8_t *pkt, rt_uint16_t payload_len);
void muart_pkt_set_crc(union muart_pkt *pkt, rt_uint16_t payload_len, rt_uint16_t crc);
void muart_pkt_build_hdr(union muart_pkt *pkt, rt_uint8_t uart_id,
                         rt_uint8_t residue, rt_uint16_t data_len,
                         rt_bool_t is_ack, rt_uint8_t ack_status, rt_uint8_t seq);
rt_bool_t muart_pkt_build(union muart_pkt *pkt, rt_uint8_t uart_id,
                          const uint8_t *payload, rt_uint16_t data_len,
                          rt_uint8_t residue, rt_bool_t is_ack, rt_uint8_t ack_status,
                          rt_uint8_t seq);
rt_bool_t muart_pkt_shape_ok(const uint8_t *pkt, rt_size_t pkt_len);
rt_bool_t muart_pkt_crc_ok(const uint8_t *pkt, rt_size_t pkt_len);
rt_bool_t muart_pkt_validate(const uint8_t *pkt, rt_size_t pkt_len);

rt_ssize_t ppp_to_wire(const uint8_t *raw, rt_size_t raw_len, uint8_t *wire, rt_size_t wire_cap);
rt_ssize_t ppp_from_wire(const uint8_t *wire, rt_size_t wire_len, rt_size_t wire_start,
                         uint8_t *raw, rt_size_t raw_cap, rt_size_t *raw_len);
rt_ssize_t ppp_find_flag(const uint8_t *buf, rt_size_t len, rt_size_t pos);

#endif /* __PPP_H__ */
