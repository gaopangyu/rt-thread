#ifndef __PPP_H__
#define __PPP_H__

#include <rtthread.h>
#include <stdint.h>
#include <rtdevice.h>

/*
 * 逻辑帧: [ctrl 1B][uart_id 1B][info 2B BE][data n B][crc 2B]
 * 线路帧: [0x7E][转义后的逻辑帧][0x7E]   转义: 0x7D，再异或 0x20
 *
 * 使用：实现 ops → ppp_adapter_init → 每通道 ppp_link_init
 *       发 ppp_adapter_send / 收 ppp_adapter_input
 */

#define PPP_PKT_PAYLOAD_MAX         1023    /* info bit9~0：本包 payload */
#define PPP_PKT_RESIDUE_MAX         63      /* info bit15~10：本包之后剩余子包数 */
#define PPP_PKT_FRAG_MAX            (PPP_PKT_RESIDUE_MAX + 1)
#define PPP_PKT_ASM_MAX             (PPP_PKT_FRAG_MAX * PPP_PKT_PAYLOAD_MAX)
#define PPP_PKT_HDR_SIZE            4       /* ctrl + uart_id + info */
#define PPP_PKT_CRC_SIZE            2
#define PPP_PKT_MIN                 (PPP_PKT_HDR_SIZE + PPP_PKT_CRC_SIZE)
#define PPP_PKT_MAX                 (PPP_PKT_MIN + PPP_PKT_PAYLOAD_MAX)

#define PPP_FLAG                    0x7E
#define PPP_ESC                     0x7D
#define PPP_XOR                     0x20
#define PPP_ESC_FLAG                (PPP_FLAG ^ PPP_XOR)  /* 0x5E */
#define PPP_ESC_ESC                 (PPP_ESC ^ PPP_XOR)   /* 0x5D */

#define PPP_PKT_WIRE_MAX            (2 + PPP_PKT_MAX * 2 + 1)

#define PPP_PKT_SEQ_MASK            0x0F
#define PPP_PKT_INFO_LEN_MASK       0x03FF
#define PPP_PKT_INFO_RESIDUE_SHIFT  10

#define PPP_PKT_SEND_NORMAL         0
#define PPP_PKT_ACK_NORMAL          0   /* 接收正确 */
#define PPP_PKT_ACK_RETX            1   /* 包错误，立即重发 */
#define PPP_PKT_ACK_SKIP            2   /* 无此通道，立即跳过 */

#define PPP_PKT_RX_SEQ_NONE         0xFF
#define PPP_ASM_IDLE                0xFF

#define PPP_SEND_ACK_TIMEOUT_MS     2000
#define PPP_SEND_RETRY              3

/*
 * info 只走 bytes.hi/lo（线路大端）。
 * 访问：ppp->pkt.ctrl.bits.* / uart_id / info.hi|lo / data
 */
union ppp_pkt
{
    rt_uint8_t raw[PPP_PKT_MAX];
    struct
    {
        union
        {
            rt_uint8_t byte;
            struct
            {
                rt_uint8_t seq    : 4;  /* bit3~0 */
                rt_uint8_t status : 3;  /* bit6~4  SEND命令 / ACK状态 */
                rt_uint8_t is_ack : 1;  /* bit7    0=DATA 1=ACK */
            } bits;
        } ctrl;
        rt_uint8_t uart_id;
        struct
        {
            rt_uint8_t hi;
            rt_uint8_t lo;
        } info;
        rt_uint8_t data[PPP_PKT_PAYLOAD_MAX];
    } __attribute__((packed)) pkt;
};

/* 一条 PPP 通道（每个 uart_id 一个） */
struct ppp_link
{
    rt_uint8_t id;
    rt_sem_t   ack_sem;
    rt_uint8_t ack_status;          /* 最近一次对端 ACK：PPP_PKT_ACK_* */
    rt_uint8_t seq;                 /* 当前发送流水号 */
    rt_uint8_t rx_seq;              /* 最近已处理的接收流水号 */
    uint8_t   *asm_buf;             /* 多分片组包，residue==0 时回调 */
    rt_size_t  asm_cap;
    rt_size_t  asm_len;
    rt_uint8_t asm_next_residue;    /* 下一包期望 residue，PPP_ASM_IDLE=空闲 */
};

struct ppp_adapter_ops
{
    struct ppp_link *(*find_link)(void *user, rt_uint8_t id);
    /* payload 仅在回调期间有效 */
    void (*on_data)(void *user, struct ppp_link *link, const uint8_t *payload, rt_size_t len);
};

struct ppp_adapter
{
    rt_device_t dev;                /* 物理口，成帧后直接 rt_device_write */
    const struct ppp_adapter_ops *ops;
    struct rt_mutex lock;
    rt_int32_t ack_timeout_ms;
    rt_uint8_t send_retry;
    union ppp_pkt tx;
    union ppp_pkt rx;
    uint8_t wire[PPP_PKT_WIRE_MAX];
};

rt_err_t ppp_adapter_init(struct ppp_adapter *adp, const char *name,
                          rt_device_t dev, const struct ppp_adapter_ops *ops);
rt_err_t ppp_adapter_deinit(struct ppp_adapter *adp);

void ppp_link_init(struct ppp_link *link, rt_uint8_t id, rt_sem_t ack_sem);
void ppp_link_deinit(struct ppp_link *link);

/* >1023 自动分包，每包等 ACK；residue 从 N-1 递减到 0 */
rt_size_t ppp_adapter_send(struct ppp_adapter *adp, struct ppp_link *link,
                           const uint8_t *data, rt_size_t size);

/* 抽出 1 帧并分发。抽出一帧返回 RT_TRUE，可循环直到 RT_FALSE */
rt_bool_t ppp_adapter_input(struct ppp_adapter *adp, uint8_t *buf, rt_size_t *buflen);

#endif /* __PPP_H__ */
