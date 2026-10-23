#ifndef __HOST_STD_H__
#define __HOST_STD_H__

#include "host.h"

struct host_std_config
{
    enum host_type host_type;
    rt_uint8_t host_id;
    char protocol_ver[RT_NAME_MAX];

    rt_uint8_t crt_com_id;
    void *interface_crt_cfg;
    void *protocol_crt_cfg;

    rt_uint8_t bus_com_id;
    void *interface_bus_cfg;
    void *protocol_bus_cfg;

    void *funnel_default_cfg;
    void *upload_store_cfg;
    void *upload_net_cfg;
};

typedef struct host_std_config *host_std_config_t;

struct host_std
{
    struct host parent;

    interface_obj_t interface_crt;
    protocol_obj_t protocol_crt;
    interface_obj_t interface_bus;
    protocol_obj_t protocol_bus;
    funnel_obj_t funnel_default;
    upload_obj_t upload_store;
    upload_obj_t upload_net;
};

typedef struct host_std *host_std_t;

rt_err_t host_std_create(host_std_config_t config);
rt_err_t host_std_create_from_json(cJSON *host_json);
rt_err_t host_std_set_param_from_json(cJSON *host_json);

#endif /* __HOST_STD_H__ */
