#ifndef __LIB_H__
#define __LIB_H__

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <rtthread.h>

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
	uint8_t eventype;
	uint8_t logver;
	uint8_t timestamp[6];
	uint8_t systype;
	uint8_t sysaddr;
	uint8_t event_len;
	uint8_t ack_len;
	const uint8_t* event_detail;
	const uint8_t* ack;

}__attribute__((packed)) EventEntry;

typedef struct {
	uint16_t event_size;
	EventEntry* events;
}Event;

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

typedef Event* (*process_data_func_t)(const uint8_t* data, const size_t len,
		const uint8_t* timestamp, const size_t timestamp_len);
typedef void (*destroy_event_func_t)(Event* event);

typedef CtrlResp * (*remote_control_func_t)(CtrlReq *);
typedef void (*destroy_ctrl_resp_func_t )(CtrlResp*);

typedef ProtocalPackage* (*package_protocal_func_t)(Event* event);
typedef void (*destroy_protocal_package_func_t)(ProtocalPackage*);

#endif /* __LIB_H__ */