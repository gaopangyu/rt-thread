#ifndef __HOST_H__
#define __HOST_H__

#include <cJSON.h>

#include "class/interface_obj.h"
#include "class/protocol_obj.h"
#include "class/funnel_obj.h"
#include "class/upload_obj.h"

#include "class/interface_rs232.h"
#include "class/protocol_ev1.h"
#include "class/funnel_default.h"
#include "class/upload_store.h"
#include "class/upload_net.h"

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
    rt_bool_t   (*net_rx_bind_status)   (host_t self);
    rt_err_t    (*net_rx_bind_create)   (host_t self);
    rt_err_t    (*net_rx_bind_delete)   (host_t self);
    rt_size_t   (*get_history)            (host_t self, rt_uint8_t *buffer, rt_size_t size, event_history_t event_history, rt_bool_t *is_end);
    rt_err_t    (*host_delete)            (host_t self);
};

extern host_t g_host[HOST_MAX];
extern char  *g_com[COM_MAX];

rt_err_t host_get_name_from_parent(char *out, rt_size_t out_size, const char *parent_name, rt_uint8_t host_id, rt_uint8_t sub_id);
const char *host_get_parent_name_from_protocol(const char *protocol_ver);

/* interface / protocol / funnel / upload 的对象布局相同，用同一套接口。 */
void *host_obj_create(const char *parent_name, rt_uint8_t host_id, rt_uint8_t sub_id, void *cfg);
void host_obj_delete(void *obj);
rt_err_t host_obj_set_param(void *obj, void *cfg, const char *name);

rt_size_t host_get_history(upload_obj_t obj, rt_uint8_t *buffer, rt_size_t size,
                                  event_history_t event_history, rt_bool_t *is_end);


void host_parse_cfg_rs232(struct cJSON *obj, struct interface_rs232_config *cfg);
void host_parse_cfg_protocol_ev1(struct cJSON *obj, struct protocol_ev1_config *cfg);
void host_parse_cfg_store(struct cJSON *obj, struct upload_store_config *cfg);

#endif /* __HOST_H__ */
