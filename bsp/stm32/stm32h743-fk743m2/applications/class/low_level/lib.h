#ifndef __LIB_H__
#define __LIB_H__

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <rtthread.h>

#define LIB_PATH "/diskfs/lib/"

enum event_file_type
{
    FILE_TYPE_RESET = 0x01, //复位
    FILE_TYPE_FIRE = 0x02, //火警
    FILE_TYPE_MONITOR = 0x03, //监管
    FILE_TYPE_FAULT = 0x04, //故障/恢复
    FILE_TYPE_ACTION = 0x05, //动作(启停)
    FILE_TYPE_LINKAGE = 0x06, //联动(反馈/取消)
    FILE_TYPE_MASK = 0x07, //屏蔽/取消
    FILE_TYPE_OTHER = 0x0C, //其他
    FILE_TYPE_CTRL = 0x0D, //控制
};

struct event_pack
{
    uint8_t version;
    uint8_t timestamp[6];
    uint8_t sys_type;
    uint8_t sys_addr;
    uint8_t event_type;
    uint8_t event_description[18];
}__attribute__((packed));
typedef struct event_pack *event_pack_t;

struct event_ack_pack 
{
    struct event_pack event;
	uint8_t ack_len;
	const uint8_t* ack;
}__attribute__((packed));
typedef struct event_ack_pack *event_ack_pack_t;

struct event_ack_pack_list{
	uint16_t count;
	event_ack_pack_t event_ack;
}__attribute__((packed));
typedef struct event_ack_pack_list *event_ack_pack_list_t;

typedef struct {
	uint8_t interval_len;
	uint8_t statucode_len;
	uint8_t reqcode_len;
	uint32_t baud;
	uint32_t timeout;
	uint8_t protocal[2];
	const uint8_t* interval;
	const uint8_t* statucode;
	const uint8_t* reqcode;
} InitEntry;

typedef struct {
}CtrlReq;

typedef struct {
    uint8_t general_len;
    const uint8_t* general_body;
} GeneralPackage;
typedef struct {
    uint16_t package_size;
    GeneralPackage* package;
}ProtocalPackage;

typedef GeneralPackage CtrlResp;
// 函数指针类型定义
typedef InitEntry* (*init_func_t)(uint8_t system_addr);
typedef void (*destroy_initentry_func_t)(InitEntry* entry);

typedef event_ack_pack_list_t (*process_data_func_t)(const uint8_t* data, const size_t len,
		const uint8_t* timestamp, const size_t timestamp_len);
typedef void (*destroy_event_func_t)(event_ack_pack_list_t event);

typedef CtrlResp * (*remote_control_func_t)(CtrlReq *);
typedef void (*destroy_ctrl_resp_func_t )(CtrlResp*);

typedef ProtocalPackage* (*package_protocal_func_t)(event_ack_pack_list_t event);
typedef void (*destroy_protocal_package_func_t)(ProtocalPackage*);

#endif /* __LIB_H__ */