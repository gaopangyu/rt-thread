#ifndef __HOST_H__
#define __HOST_H__

#include "class/interface_obj.h"
#include "class/protocol_obj.h"
#include "class/funnel_obj.h"
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

rt_err_t host_set_interface_param(interface_obj_t obj, void *cfg, const char *name);
rt_err_t host_set_protocol_param(protocol_obj_t obj, void *cfg, const char *name);
rt_err_t host_set_funnel_param(funnel_obj_t obj, void *cfg, const char *name);
rt_err_t host_set_upload_param(upload_obj_t obj, void *cfg, const char *name);

void host_delete_interface(interface_obj_t *obj);
void host_delete_protocol(protocol_obj_t *obj);
void host_delete_funnel(funnel_obj_t *obj);
void host_delete_upload(upload_obj_t *obj);

interface_obj_t host_create_interface(const char *parent_name, rt_uint8_t host_id, rt_uint8_t sub_id, void *cfg);
protocol_obj_t host_create_protocol(const char *parent_name, rt_uint8_t host_id, rt_uint8_t sub_id, void *cfg);
funnel_obj_t host_create_funnel(const char *parent_name, rt_uint8_t host_id, rt_uint8_t sub_id, void *cfg);
upload_obj_t host_create_upload(const char *parent_name, rt_uint8_t host_id, rt_uint8_t sub_id, void *cfg);

rt_size_t host_upload_get_history(upload_obj_t obj, rt_uint8_t *buffer, rt_size_t size,
                                  event_history_t event_history, rt_bool_t *is_end);

struct cJSON;
struct interface_rs232_config;
struct protocol_ev1_config;
struct upload_store_config;

void host_parse_rs232(struct cJSON *obj, struct interface_rs232_config *cfg);
void host_parse_protocol_ev1(struct cJSON *obj, struct protocol_ev1_config *cfg);
void host_parse_store(struct cJSON *obj, struct upload_store_config *cfg);

#endif /* __HOST_H__ */
