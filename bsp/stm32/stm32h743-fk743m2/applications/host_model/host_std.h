#ifndef __HOST_STD_H__
#define __HOST_STD_H__

#include "host.h"

struct host_std_config
{
    rt_uint8_t host_id;
    enum host_type host_type;
    
    rt_uint8_t crt_com_id;
    void *crt_interface_cfg;
    rt_uint8_t bus_com_id;
    void *bus_interface_cfg;
    void *crt_protocol_cfg;
    void *bus_protocol_cfg;
    void *store_upload_cfg;
    void *muart_upload_cfg[PLATFORM_MAX];
};

typedef struct host_std_config *host_std_config_t;

struct host_std
{
    struct host parent;

    interface_obj_t crt_interface;
    interface_obj_t bus_interface;
    protocol_obj_t crt_protocol;
    protocol_obj_t bus_protocol;
    upload_obj_t store_upload;
    upload_obj_t muart_upload[PLATFORM_MAX];
};

typedef struct host_std *host_std_t;

rt_err_t host_std_create(host_std_config_t config);

#endif /* __HOST_STD_H__ */
