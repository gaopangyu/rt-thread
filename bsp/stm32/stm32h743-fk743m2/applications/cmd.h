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

rt_size_t cmd_parse(enum cmd_obj_type obj_type, rt_uint8_t *buffer, rt_size_t len,
                    rt_uint8_t *reply, rt_size_t reply_size);

#endif /* __CMD_H__ */
