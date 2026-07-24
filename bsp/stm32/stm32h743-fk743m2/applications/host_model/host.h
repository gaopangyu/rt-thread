#ifndef __HOST_H__
#define __HOST_H__

#include "class/interface_obj.h"
#include "class/protocol_obj.h"
#include "class/upload_obj.h"

#define COM_MAX         7
#define PLATFORM_MAX    6
#define HOST_MAX        6

enum host_type
{
    HOST_TYPE_NONE = 0,
    HOST_TYPE_STD,
    HOST_TYPE_MAX,
};

struct host
{
    rt_uint8_t id;
    enum host_type type;

    struct host_ops *ops;
};

typedef struct host *host_t;

struct host_ops
{
    rt_err_t    (*create_muart_rx_bind)   (host_t self, rt_uint8_t platform_id);
    rt_err_t    (*delete_muart_rx_bind)   (host_t self, rt_uint8_t platform_id);
    rt_err_t    (*delete_host)            (host_t self);
};

extern host_t g_host[HOST_MAX];
extern char  *g_com[COM_MAX];

rt_err_t generate_name_from_parent(char *out, rt_size_t out_size, const char *parent_name, rt_uint8_t host_id, rt_uint8_t sub_id);

#endif /* __HOST_H__ */
