#ifndef __HOST_H__
#define __HOST_H__

#include "interface_obj.h"
#include "protocol_obj.h"
#include "upload_obj.h"

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

    const struct host_ops *ops;
};

typedef struct host *host_t;

struct host_ops
{
    rt_err_t    (*create_muart_rx_bind)   (rt_uint8_t platform_id);
    rt_err_t    (*delete_muart_rx_bind)   (rt_uint8_t platform_id);
};

extern host_t g_host[HOST_MAX];

#endif /* __HOST_H__ */
