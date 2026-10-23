#ifndef __CMD_H__
#define __CMD_H__

#include <rtthread.h>

enum cmd_obj_type
{
    CMD_OBJ_NET = 0,
    CMD_OBJ_UI,
    CMD_OBJ_MAX,
};
typedef enum cmd_obj_type cmd_obj_type_t;

enum cmd_type
{
    CMD_GET_PARAM      = 0x01, /* 获取参数。请求：hosts[id]|platforms[id]|eth|all，net 只有 all */
    CMD_SET_PARAM      = 0x02, /* 设置参数。回复：成功条数 1 字节 + 失败条数 1 字节 */
    CMD_GET_HISTORY    = 0x03, /* 获取历史报文。请求：struct event_history；回复：32 字节事件数组 */
    CMD_DELETE_BIND    = 0x04, /* 删除绑定。请求：host_id；回复：1 成功 / 0 失败 */
    CMD_CREATE_BIND    = 0x05, /* 创建绑定。请求：host_id；回复：1 成功 / 0 失败 */
    CMD_GET_BIND       = 0x06, /* 获取绑定状态。请求：link_id，49 控制链路，255 为全部；回复：JSON 数组 */
    CMD_SWITCH_CONSOLE = 0x07, /* 切换控制台。请求：usb|uart|net；回复：1 成功 / 0 失败 */
};
typedef enum cmd_type cmd_type_t;

rt_size_t cmd_parse(enum cmd_obj_type obj_type, rt_uint8_t *buffer, rt_size_t len,
                    rt_uint8_t *reply, rt_size_t reply_size);

#endif /* __CMD_H__ */
