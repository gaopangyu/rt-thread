#ifndef __HOST_H__
#define __HOST_H__

#include "class/interface_obj.h"
#include "class/protocol_obj.h"
#include "class/upload_obj.h"

#define COM_MAX         7
#define HOST_MAX        4
#define PLATFORM_MAX    4

enum host_type
{
    HOST_TYPE_NONE = 0,
    HOST_TYPE_STD = 1,
    HOST_TYPE_MAX,
};

struct host
{
    enum host_type type;
    rt_uint8_t id;

    struct host_ops *ops;
};
typedef struct host *host_t;

struct host_ops
{
    rt_bool_t   (*muart_rx_bind_status)   (host_t self);
    rt_err_t    (*muart_rx_bind_create)   (host_t self);
    rt_err_t    (*muart_rx_bind_delete)   (host_t self);
    rt_size_t   (*get_history)            (host_t self, rt_uint8_t *buffer, rt_size_t size, event_history_t event_history, rt_bool_t *is_end);
    rt_err_t    (*host_delete)            (host_t self);
};

extern host_t g_host[HOST_MAX];
extern char  *g_com[COM_MAX];

rt_err_t generate_name_from_parent(char *out, rt_size_t out_size, const char *parent_name, rt_uint8_t host_id, rt_uint8_t sub_id);
const char *host_protocol_parent_name(const char *protocol_ver);

#endif /* __HOST_H__ */
