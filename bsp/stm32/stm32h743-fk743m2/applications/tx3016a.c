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

}EventEntry;

typedef struct {
	uint16_t event_size;
	const EventEntry* events;
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

void tx3016a_test_entry(void *parameter) {
	dlerror();
	void *handle_tx3016a= dlopen("/sdcard/libtx3016a.so", RTLD_LAZY);
	if (!handle_tx3016a) {
		rt_kprintf("Error: %s\n", dlerror());
		return;
	}

	init_func_t init= (init_func_t)dlsym(handle_tx3016a, "init");
	destroy_initentry_func_t destroy_initentry = (destroy_initentry_func_t)dlsym(handle_tx3016a, "destroy_initentry");
	process_data_func_t process_data = (process_data_func_t)dlsym(handle_tx3016a, "process_data");
	destroy_event_func_t destroy_event = (destroy_event_func_t)dlsym(handle_tx3016a, "destroy_event");
	remote_control_func_t remote_ctrl = (remote_control_func_t)dlsym(handle_tx3016a, "remote_control");

	destroy_ctrl_resp_func_t destroy_ctrl_resp  = (destroy_ctrl_resp_func_t)dlsym(handle_tx3016a, "destroy_ctrl_resp");

	if (NULL == init || NULL == destroy_initentry
			|| NULL == process_data|| NULL == destroy_event
			|| NULL == remote_ctrl|| NULL == destroy_ctrl_resp) {
		rt_kprintf("--------------\n");
		dlclose(handle_tx3016a);
		return;
	}
	dlerror();
	void *handle_protocal = dlopen("/sdcard/libprotocal_gb.so", RTLD_LAZY);
	if (!handle_protocal) {
		rt_kprintf("Error: %s\n", dlerror());
		return;
	}
	InitEntry *entry = init(128);
	if (NULL != entry) {
		rt_kprintf("band %d -- timeout %d -- ",
			entry->baud, entry->timeout);
		int idx = 0;
		rt_kprintf(" protocal are ");
		for (;idx<2;idx++) {
			rt_kprintf("%d ", entry->protocal[idx]);
		}
		rt_kprintf("\ninterval are ");
		for (idx=0;idx<entry->interval_len;idx++) {
			rt_kprintf("%d ", entry->interval[idx]);
		}
		rt_kprintf("\nand the reqcode are ");

		for (idx=0;idx<entry->reqcode_len;idx++) {
			rt_kprintf("%d ", entry->reqcode[idx]);
		}
		rt_kprintf("\n");
	}

	const uint8_t input[] = {
		0x40,0x40,0x02,0x00,0x02,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x00,0x02,0x04,0x01,0x01,0x01,0x01,0x00,0x3A,0x03,0x0E,0x1D,0x0B,0x0D,0x9C,0x23,0x23,

		0x40,0x40,0x04,0x00,0x02,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x34,0x00,0x02,0x02,0x01,0x01,0x01,0x4E,0x30,0x30,0x31,
		0x30,0x31,0x30,0x30,0x31,0x01,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x35,0x04,0x0E,0x1D,0x0B,0x0D,0xAB,0x23,0x23,
	};
	const uint8_t timestamp[] = { 1, 2, 3, 4, 5, 6 };
	Event * event = process_data(input, sizeof(input)/sizeof(uint8_t), timestamp, sizeof(timestamp)/sizeof(uint8_t));
	if (NULL != event) {
		rt_kprintf("%d --- %p\n", event->event_size, event->events);
		for (int i=0;i< event->event_size;i++) {
			rt_kprintf("---- eventype %02X ---- logver %02X ---- systype %02X ---- sysaddr %02X ---- event_len %02X ---- ack_len %02X\n",
					event->events[i].eventype, event->events[i].logver,
					event->events[i].systype, event->events[i].sysaddr,
					event->events[i].event_len, event->events[i].ack_len);
			rt_kprintf("event detail package are ");
			for(int j=0;j<event->events[i].event_len;j++) {
				rt_kprintf("%02X ", event->events[i].event_detail[j]);
			}
			rt_kprintf("\nack package are ");
			for(int j=0;j<event->events[i].ack_len;j++) {
				rt_kprintf("%02X ", event->events[i].ack[j]);
			}
			rt_kprintf("\n");
		}
	}

	CtrlReq * req = NULL;
	CtrlResp *data = remote_ctrl(req);
	if (NULL != data) {
		rt_kprintf("Genearal package are ");
		for(int i=0; i< data->general_len; i++) {
			rt_kprintf("%02X ", data->general_body[i]);
		}
		rt_kprintf("\n");
	}

	package_protocal_func_t package_protocal = (package_protocal_func_t)dlsym(handle_protocal, "package_protocal");
	destroy_protocal_package_func_t destroy_protocal_package = (destroy_protocal_package_func_t)dlsym(handle_protocal, "destroy_protocal_package");
	if (NULL == package_protocal || NULL == destroy_protocal_package) {
		dlclose(handle_protocal);
		return;
	}
	ProtocalPackage * packages = package_protocal(event);
	rt_kprintf("===========================================\n");
	if (NULL != packages) {
		rt_kprintf("The packages are: \n");
		for(int i=0; i<packages->package_size; i++) {
			rt_kprintf("                  ");
			for(int j=0; j<packages->package[i].general_len;j++) {
				rt_kprintf("%02X ", packages->package[i].general_body[j]);
			}
			rt_kprintf("\n");
		}
	}
	destroy_protocal_package(packages);
	dlclose(handle_protocal);
	destroy_ctrl_resp(data);
	destroy_event(event);
	destroy_initentry(entry);
	dlclose(handle_tx3016a);
	return;
}


