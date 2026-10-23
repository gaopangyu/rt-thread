#ifndef __CMD_H__
#define __CMD_H__

#include <rtthread.h>

rt_size_t cmd_parse(rt_uint8_t *buffer, rt_size_t len, rt_uint8_t *reply, rt_size_t reply_size);

#endif /* __CMD_H__ */
