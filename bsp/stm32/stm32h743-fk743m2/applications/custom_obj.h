#ifndef __CUSTOM_OBJ_H__
#define __CUSTOM_OBJ_H__

#include <rtthread.h>

#define CUSTOM_PER_LAYER_OBJ_MAX  32
#define CUSTOM_THREAD_PRIORITY  16

enum custom_object_class_type
{
    Custom_Object_Class_Null          = 0x00,      
    Custom_Object_Class_Interface     = 0x01,      
    Custom_Object_Class_Protocol      = 0x02,
    Custom_Object_Class_Upload        = 0x03,
};

typedef struct
{
    int succ;
    int fail;
} double_ret;

struct bind_node
{
    rt_mutex_t bind_mutex;
    struct custom_obj **pre_obj_list;
    struct custom_obj **next_obj_list;
    rt_uint8_t *buffer;
    rt_size_t len;
    rt_uint32_t pre_event_mask;
    rt_uint32_t next_event_mask;
    rt_event_t sub_event;
    rt_mailbox_t sub_mb;
    rt_event_t pub_event;
    rt_event_t pub_succ_event;

    rt_atomic_t ext_recv_bytes;
    rt_atomic_t ext_send_bytes;
    rt_atomic_t link_recv_bytes[CUSTOM_PER_LAYER_OBJ_MAX];
    rt_atomic_t link_send_bytes[CUSTOM_PER_LAYER_OBJ_MAX];
};

struct custom_obj
{
    struct rt_object    parent;

    enum custom_object_class_type type;
    rt_uint32_t event_flag;
    rt_uint32_t *event_mask;
    struct custom_obj **obj_list;

    struct bind_node rxb;
    struct bind_node txb;

    rt_atomic_t gc_take;
    rt_atomic_t gc_flag;

    const struct custom_object_ops *ops;
};

typedef struct custom_obj *custom_obj_t;

struct custom_object_ops
{
    rt_err_t  (*gc) (custom_obj_t self);
};

rt_err_t apply_for_event_flag(custom_obj_t self);
rt_err_t unapply_for_event_flag(custom_obj_t self);
custom_obj_t get_obj_from_event_flag(custom_obj_t self, struct custom_obj **obj_list, rt_uint32_t *event_flag, rt_bool_t clear);

const char *custom_obj_msh_get_opt(int argc, char *argv[], const char *opt);
rt_bool_t custom_obj_msh_has_opt(int argc, char *argv[], const char *opt);
const char *custom_obj_msh_get_pos(int argc, char *argv[], int index, const char *flag_only_opts);
rt_err_t custom_obj_msh_check_create_name(const char *name, const char *type_prefix);
rt_err_t custom_obj_msh_check_delete_name(const char *name);

void custom_obj_bind_ext_add_recv(custom_obj_t obj, rt_size_t len, rt_bool_t is_rx);
void custom_obj_bind_ext_add_send(custom_obj_t obj, rt_size_t len, rt_bool_t is_rx);
void custom_obj_bind_link_xfer(custom_obj_t pre, custom_obj_t next, rt_size_t len, rt_bool_t is_rx);
rt_size_t custom_obj_bind_path_recv_bytes(custom_obj_t obj, custom_obj_t parent,
                                          custom_obj_t branch_child, rt_bool_t is_rx);
rt_size_t custom_obj_bind_path_send_bytes(custom_obj_t obj, custom_obj_t parent, rt_bool_t is_rx);
rt_size_t custom_obj_bind_format_path_label(char *buf, rt_size_t size, custom_obj_t obj,
                                            custom_obj_t parent, custom_obj_t branch_child,
                                            rt_bool_t is_rx);

#endif /* __CUSTOM_OBJ_H__ */
