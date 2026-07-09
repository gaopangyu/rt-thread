#include "bind.h"

#define DBG_TAG "bind"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

rt_err_t bind_create(custom_obj_t pre_obj, custom_obj_t next_obj, rt_bool_t is_rx)
{
    if (pre_obj == RT_NULL || next_obj == RT_NULL || pre_obj == next_obj)
        return -RT_EINVAL;

    if (is_rx)
    {
        if(rt_mutex_take(pre_obj->rxb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("Failed to take rx_bind_mutex");
            return -RT_ERROR;
        }

        if(rt_mutex_take(next_obj->rxb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("Failed to take rx_bind_mutex");
            rt_mutex_release(pre_obj->rxb.bind_mutex);
            return -RT_ERROR;
        }

        if( pre_obj->event_flag == 0 )
            apply_for_event_flag(pre_obj);

        if( next_obj->event_flag == 0 )
            apply_for_event_flag(next_obj);

        if( pre_obj->event_flag != 0 && next_obj->event_flag != 0 )
        {
            pre_obj->rxb.next_event_mask |= next_obj->event_flag;
            pre_obj->rxb.next_obj_list = next_obj->obj_list;
            next_obj->rxb.pre_event_mask |= pre_obj->event_flag;
            next_obj->rxb.sub_event = pre_obj->rxb.pub_event;
            next_obj->rxb.pre_obj_list = pre_obj->obj_list;
            rt_event_control(next_obj->rxb.sub_event, RT_IPC_CMD_RESET, RT_NULL);
        }
        else
            LOG_E("One of the objects does not apply for event flag: %s %d, %s %d", pre_obj->parent.name, pre_obj->event_flag, next_obj->parent.name, next_obj->event_flag);

        rt_thread_mdelay(110); //等待接收事件rt_event_recv准备好

        rt_mutex_release(next_obj->rxb.bind_mutex);
        rt_mutex_release(pre_obj->rxb.bind_mutex);
        
        LOG_I("Bind rx created: %s ---> %s", ((rt_object_t)pre_obj)->name, ((rt_object_t)next_obj)->name);
    }
    else
    {
        if(rt_mutex_take(next_obj->txb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("Failed to take tx_bind_mutex");
            return -RT_ERROR;
        }

        if(rt_mutex_take(pre_obj->txb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("Failed to take tx_bind_mutex");
            rt_mutex_release(next_obj->txb.bind_mutex);
            return -RT_ERROR;
        }

        if( pre_obj->event_flag == 0 )
            apply_for_event_flag(pre_obj);

        if( next_obj->event_flag == 0 )
            apply_for_event_flag(next_obj);

        if( pre_obj->event_flag != 0 && next_obj->event_flag != 0 )
        {
            next_obj->txb.pre_event_mask |= pre_obj->event_flag;
            next_obj->txb.pre_obj_list = pre_obj->obj_list;
            pre_obj->txb.next_event_mask |= next_obj->event_flag;
            pre_obj->txb.sub_event = next_obj->txb.pub_event;
            pre_obj->txb.next_obj_list = next_obj->obj_list;
            rt_event_control(pre_obj->txb.sub_event, RT_IPC_CMD_RESET, RT_NULL);
        }
        else
            LOG_E("One of the objects does not apply for event flag: %s %d, %s %d", pre_obj->parent.name, pre_obj->event_flag, next_obj->parent.name, next_obj->event_flag);

        rt_thread_mdelay(110); //等待接收事件rt_event_recv准备好

        rt_mutex_release(pre_obj->txb.bind_mutex);
        rt_mutex_release(next_obj->txb.bind_mutex);
        
        LOG_I("Bind tx created: %s <--- %s", ((rt_object_t)pre_obj)->name, ((rt_object_t)next_obj)->name);
    }

    return RT_EOK;
}

static void bind_try_release_obj(custom_obj_t obj)
{
    if (obj == RT_NULL)
        return;

    if (obj->rxb.pre_event_mask != 0 || obj->rxb.next_event_mask != 0 ||
        obj->txb.pre_event_mask != 0 || obj->txb.next_event_mask != 0)
        return;

    obj->rxb.sub_event = RT_NULL;
    obj->txb.sub_event = RT_NULL;
    obj->rxb.pre_obj_list = RT_NULL;
    obj->rxb.next_obj_list = RT_NULL;
    obj->txb.pre_obj_list = RT_NULL;
    obj->txb.next_obj_list = RT_NULL;

    if (obj->event_flag != 0)
        unapply_for_event_flag(obj);
}

rt_err_t bind_delete(custom_obj_t pre_obj, custom_obj_t next_obj, rt_bool_t is_rx)
{
    if (pre_obj == RT_NULL || next_obj == RT_NULL || pre_obj == next_obj)
        return -RT_EINVAL;

    if (is_rx)
    {
        if(rt_mutex_take(pre_obj->rxb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("Failed to take rx_bind_mutex");
            return -RT_ERROR;
        }

        if(rt_mutex_take(next_obj->rxb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("Failed to take rx_bind_mutex");
            rt_mutex_release(pre_obj->rxb.bind_mutex);
            return -RT_ERROR;
        }

        if (pre_obj->event_flag != 0 && next_obj->event_flag != 0)
        {
            pre_obj->rxb.next_event_mask &= ~next_obj->event_flag;
            next_obj->rxb.pre_event_mask &= ~pre_obj->event_flag;
            if (next_obj->rxb.sub_event != RT_NULL)
                rt_event_control(next_obj->rxb.sub_event, RT_IPC_CMD_RESET, RT_NULL);
            if (pre_obj->rxb.pub_succ_event != RT_NULL)
                rt_event_control(pre_obj->rxb.pub_succ_event, RT_IPC_CMD_RESET, RT_NULL);
        }
        else if ((next_obj->event_flag != 0 && (pre_obj->rxb.next_event_mask & next_obj->event_flag) != 0)
              || (pre_obj->event_flag != 0 && (next_obj->rxb.pre_event_mask & pre_obj->event_flag) != 0))
        {
            LOG_E("One of the objects does not apply for event flag: %s %d, %s %d",
                  pre_obj->parent.name, pre_obj->event_flag,
                  next_obj->parent.name, next_obj->event_flag);
        }

        rt_thread_mdelay(110); //等待接收事件rt_event_recv准备好

        rt_mutex_release(next_obj->rxb.bind_mutex);
        rt_mutex_release(pre_obj->rxb.bind_mutex);

        LOG_I("Bind rx deleted: %s ---> %s", ((rt_object_t)pre_obj)->name, ((rt_object_t)next_obj)->name);
    }
    else
    {
        if(rt_mutex_take(next_obj->txb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("Failed to take tx_bind_mutex");
            return -RT_ERROR;
        }

        if(rt_mutex_take(pre_obj->txb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("Failed to take tx_bind_mutex");
            rt_mutex_release(next_obj->txb.bind_mutex);
            return -RT_ERROR;
        }

        if (pre_obj->event_flag != 0 && next_obj->event_flag != 0)
        {
            next_obj->txb.pre_event_mask &= ~pre_obj->event_flag;
            pre_obj->txb.next_event_mask &= ~next_obj->event_flag;
            if (pre_obj->txb.sub_event != RT_NULL)
                rt_event_control(pre_obj->txb.sub_event, RT_IPC_CMD_RESET, RT_NULL);
            if (next_obj->txb.pub_succ_event != RT_NULL)
                rt_event_control(next_obj->txb.pub_succ_event, RT_IPC_CMD_RESET, RT_NULL);
        }
        else if ((next_obj->event_flag != 0 && (pre_obj->txb.next_event_mask & next_obj->event_flag) != 0)
              || (pre_obj->event_flag != 0 && (next_obj->txb.pre_event_mask & pre_obj->event_flag) != 0))
        {
            LOG_E("One of the objects does not apply for event flag: %s %d, %s %d",
                  pre_obj->parent.name, pre_obj->event_flag,
                  next_obj->parent.name, next_obj->event_flag);
        }
        
        rt_thread_mdelay(110); //等待接收事件rt_event_recv准备好

        rt_mutex_release(pre_obj->txb.bind_mutex);
        rt_mutex_release(next_obj->txb.bind_mutex);

        LOG_I("Bind tx deleted: %s <--- %s", ((rt_object_t)pre_obj)->name, ((rt_object_t)next_obj)->name);
    }

    return RT_EOK;
}

static void bind_try_release_peers(custom_obj_t pre_obj, custom_obj_t next_obj)
{
    bind_try_release_obj(pre_obj);
    bind_try_release_obj(next_obj);
}

static void bind_delete_pair(custom_obj_t pre_obj, custom_obj_t next_obj)
{
    bind_delete(pre_obj, next_obj, RT_TRUE);
    bind_delete(pre_obj, next_obj, RT_FALSE);
    bind_try_release_peers(pre_obj, next_obj);
}

void bind_delete_all_for_obj(custom_obj_t self)
{
    if (self == RT_NULL)
        return;

    custom_obj_t peer;

    while (self->rxb.pre_event_mask)
    {
        peer = get_obj_from_event_flag(self, self->rxb.pre_obj_list, &self->rxb.pre_event_mask, RT_FALSE);
        if (peer == RT_NULL)
            break;

        bind_delete_pair(peer, self);
    }

    while (self->rxb.next_event_mask)
    {
        peer = get_obj_from_event_flag(self, self->rxb.next_obj_list, &self->rxb.next_event_mask, RT_FALSE);
        if (peer == RT_NULL)
            break;

        bind_delete_pair(self, peer);
    }

    while (self->txb.next_event_mask)
    {
        peer = get_obj_from_event_flag(self, self->txb.next_obj_list, &self->txb.next_event_mask, RT_FALSE);
        if (peer == RT_NULL)
            break;

        bind_delete_pair(self, peer);
    }

    while (self->txb.pre_event_mask)
    {
        peer = get_obj_from_event_flag(self, self->txb.pre_obj_list, &self->txb.pre_event_mask, RT_FALSE);
        if (peer == RT_NULL)
            break;

        bind_delete_pair(peer, self);
    }


}

static void bind_msh_parse_rt_flags(int argc, char *argv[], rt_bool_t *do_rx, rt_bool_t *do_tx)
{
    *do_rx = custom_obj_msh_has_opt(argc, argv, "r");
    *do_tx = custom_obj_msh_has_opt(argc, argv, "t");

    if (!*do_rx && !*do_tx)
    {
        *do_rx = RT_TRUE;
        *do_tx = RT_TRUE;
    }
}

static int cmd_create_bind(int argc, char *argv[])
{
    const char *pre_obj_name;
    const char *next_obj_name;
    rt_bool_t do_rx;
    rt_bool_t do_tx;
    rt_err_t err = RT_EOK;

    if (argc < 3)
    {
        rt_kprintf("Usage: create_bind <pre_obj_name> <next_obj_name> [-r] [-t]\n");
        rt_kprintf("  -r  create rx bind only (pre ---> next)\n");
        rt_kprintf("  -t  create tx bind only (pre <--- next)\n");
        rt_kprintf("  default: both rx and tx\n");
        return -RT_ERROR;
    }

    pre_obj_name = custom_obj_msh_get_pos(argc, argv, 0, "rt");
    next_obj_name = custom_obj_msh_get_pos(argc, argv, 1, "rt");
    if (pre_obj_name == RT_NULL || next_obj_name == RT_NULL)
    {
        rt_kprintf("Usage: create_bind <pre_obj_name> <next_obj_name> [-r] [-t]\n");
        return -RT_ERROR;
    }

    bind_msh_parse_rt_flags(argc, argv, &do_rx, &do_tx);

    custom_obj_t pre_obj = (custom_obj_t)rt_object_find(pre_obj_name, RT_Object_Class_Custom);
    if (pre_obj == RT_NULL)
    {
        rt_kprintf("Object not found: %s\n", pre_obj_name);
        return -RT_ERROR;
    }

    custom_obj_t next_obj = (custom_obj_t)rt_object_find(next_obj_name, RT_Object_Class_Custom);
    if (next_obj == RT_NULL)
    {
        rt_kprintf("Object not found: %s\n", next_obj_name);
        return -RT_ERROR;
    }

    if (do_rx)
    {
        rt_err_t err_rx = bind_create(pre_obj, next_obj, RT_TRUE);

        if (err_rx != RT_EOK)
        {
            rt_kprintf("Failed to create bind rx %s ---> %s\n", pre_obj_name, next_obj_name);
            err = err_rx;
        }
    }

    if (do_tx)
    {
        rt_err_t err_tx = bind_create(pre_obj, next_obj, RT_FALSE);

        if (err_tx != RT_EOK)
        {
            rt_kprintf("Failed to create bind tx %s <--- %s\n", pre_obj_name, next_obj_name);
            err = err_tx;
        }
    }

    return err;
}
MSH_CMD_EXPORT_ALIAS(cmd_create_bind, create_bind, Create bind: create_bind <pre> <next> [-r] [-t]);

static int cmd_delete_bind(int argc, char *argv[])
{
    const char *pre_obj_name;
    const char *next_obj_name;
    rt_bool_t do_rx;
    rt_bool_t do_tx;
    rt_err_t err = RT_EOK;

    if (argc < 3)
    {
        rt_kprintf("Usage: delete_bind <pre_obj_name> <next_obj_name> [-r] [-t]\n");
        rt_kprintf("  -r  delete rx bind only (pre ---> next)\n");
        rt_kprintf("  -t  delete tx bind only (pre <--- next)\n");
        rt_kprintf("  default: both rx and tx\n");
        return -RT_ERROR;
    }

    pre_obj_name = custom_obj_msh_get_pos(argc, argv, 0, "rt");
    next_obj_name = custom_obj_msh_get_pos(argc, argv, 1, "rt");
    if (pre_obj_name == RT_NULL || next_obj_name == RT_NULL)
    {
        rt_kprintf("Usage: delete_bind <pre_obj_name> <next_obj_name> [-r] [-t]\n");
        return -RT_ERROR;
    }

    bind_msh_parse_rt_flags(argc, argv, &do_rx, &do_tx);

    custom_obj_t pre_obj = (custom_obj_t)rt_object_find(pre_obj_name, RT_Object_Class_Custom);
    if (pre_obj == RT_NULL)
    {
        rt_kprintf("Object not found: %s\n", pre_obj_name);
        return -RT_ERROR;
    }

    custom_obj_t next_obj = (custom_obj_t)rt_object_find(next_obj_name, RT_Object_Class_Custom);
    if (next_obj == RT_NULL)
    {
        rt_kprintf("Object not found: %s\n", next_obj_name);
        return -RT_ERROR;
    }

    if (do_rx)
    {
        rt_err_t err_rx = bind_delete(pre_obj, next_obj, RT_TRUE);

        if (err_rx != RT_EOK)
        {
            rt_kprintf("Failed to delete bind rx %s ---> %s\n", pre_obj_name, next_obj_name);
            err = err_rx;
        }
    }

    if (do_tx)
    {
        rt_err_t err_tx = bind_delete(pre_obj, next_obj, RT_FALSE);

        if (err_tx != RT_EOK)
        {
            rt_kprintf("Failed to delete bind tx %s <--- %s\n", pre_obj_name, next_obj_name);
            err = err_tx;
        }
    }

    if (do_rx || do_tx)
        bind_try_release_peers(pre_obj, next_obj);

    return err;
}
MSH_CMD_EXPORT_ALIAS(cmd_delete_bind, delete_bind, Delete bind: delete_bind <pre> <next> [-r] [-t]);

#define BIND_PATH_MAX_DEPTH   16
#define BIND_PATH_PREFIX_MAX  128
#define BIND_PATH_LABEL_MAX   (RT_NAME_MAX + 32)

struct bind_path_tree_ctx
{
    char branch_prefix[BIND_PATH_PREFIX_MAX];
    int line_pos;
    rt_bool_t root_multi_branch;
    const char *root_arrow;
    const char *arrow;
    const char *branch_arrow;
};

static int bind_path_collect_children(custom_obj_t cur, rt_bool_t is_rx,
                                      custom_obj_t *children, int max)
{
    rt_uint32_t mask = is_rx ? cur->rxb.next_event_mask : cur->txb.next_event_mask;
    struct custom_obj **list = is_rx ? cur->rxb.next_obj_list : cur->txb.next_obj_list;
    int count = 0;

    if (cur == RT_NULL || children == RT_NULL || max <= 0 || mask == 0 || list == RT_NULL)
        return 0;

    for (int i = 0; i < CUSTOM_PER_LAYER_OBJ_MAX && count < max; i++)
    {
        if ((mask >> i) & 1 && list[i] != RT_NULL)
            children[count++] = list[i];
    }

    return count;
}

static void bind_path_extend_prefix_to_col(char *prefix, rt_size_t size, int col)
{
    rt_size_t len;
    int pad;

    if (prefix == RT_NULL || size == 0 || col < 0)
        return;

    len = rt_strnlen(prefix, size - 1);
    pad = col - (int)len - 1;
    if (pad < 0)
        pad = 0;

    if (len + 1 + (rt_size_t)pad + 1 >= size)
        return;

    prefix[len] = '|';
    if (pad > 0)
        rt_memset(prefix + len + 1, ' ', pad);
    prefix[len + 1 + pad] = '|';
    prefix[len + 1 + pad + 1] = '\0';
}

static void bind_path_set_prefix_col(char *prefix, rt_size_t size, int col)
{
    if (prefix == RT_NULL || col < 0 || (rt_size_t)col + 1 >= size)
        return;

    rt_memset(prefix, ' ', col);
    prefix[col] = '|';
    prefix[col + 1] = '\0';
}

static int bind_path_branch_line_pos(const char *prefix, const char *branch_arrow, const char *name)
{
    return (int)rt_strnlen(prefix, BIND_PATH_PREFIX_MAX) + 1
         + (int)rt_strlen(branch_arrow) + 1 + (int)rt_strlen(name);
}

static rt_bool_t bind_path_prefix_has_vert(const char *prefix)
{
    if (prefix == RT_NULL)
        return RT_FALSE;

    for (; *prefix != '\0'; prefix++)
    {
        if (*prefix == '|')
            return RT_TRUE;
    }

    return RT_FALSE;
}

static void bind_path_print_sibling(struct bind_path_tree_ctx *ctx, int depth,
                                    int parent_col, const char *name)
{
    int branch_col = parent_col + 1;
    rt_size_t prefix_len = rt_strnlen(ctx->branch_prefix, sizeof(ctx->branch_prefix));
    rt_bool_t use_root_vert = (depth == 0) || ctx->root_multi_branch
                           || bind_path_prefix_has_vert(ctx->branch_prefix);

    if (use_root_vert)
    {
        if (depth == 0)
        {
            rt_kprintf("\n%s|\n%s|%s %s", ctx->branch_prefix, ctx->branch_prefix,
                       ctx->branch_arrow, name);
        }
        else
        {
            int pad = branch_col - (int)prefix_len - 1;

            if (pad < 0)
                pad = 0;
            rt_kprintf("\n%s|%*s|\n", ctx->branch_prefix, pad, "");
            bind_path_extend_prefix_to_col(ctx->branch_prefix, sizeof(ctx->branch_prefix), branch_col);
            rt_kprintf("%s%s %s", ctx->branch_prefix, ctx->branch_arrow, name);
        }
        ctx->line_pos = bind_path_branch_line_pos(ctx->branch_prefix, ctx->branch_arrow, name);
    }
    else
    {
        rt_kprintf("\n%*s|\n%*s|%s %s", branch_col, "", branch_col, "", ctx->branch_arrow, name);
        ctx->line_pos = branch_col + 1 + (int)rt_strlen(ctx->branch_arrow) + 1 + (int)rt_strlen(name);
        bind_path_set_prefix_col(ctx->branch_prefix, sizeof(ctx->branch_prefix), branch_col);
    }
}

static custom_obj_t bind_path_tx_branch_child(custom_obj_t obj)
{
    custom_obj_t children[BIND_PATH_MAX_DEPTH];

    if (bind_path_collect_children(obj, RT_FALSE, children, BIND_PATH_MAX_DEPTH) == 1)
        return children[0];

    return RT_NULL;
}

static void bind_path_print_tree_recur(custom_obj_t node, struct bind_path_tree_ctx *ctx,
                                       int depth, rt_bool_t is_rx)
{
    custom_obj_t children[BIND_PATH_MAX_DEPTH];
    char label[BIND_PATH_LABEL_MAX];
    int n;

    if (node == RT_NULL || ctx == RT_NULL)
        return;

    if (depth >= BIND_PATH_MAX_DEPTH)
    {
        rt_kprintf(" ... (max depth)");
        return;
    }

    n = bind_path_collect_children(node, is_rx, children, BIND_PATH_MAX_DEPTH);
    if (n == 0)
        return;

    for (int i = 0; i < n; i++)
    {
        char saved_prefix[BIND_PATH_PREFIX_MAX];
        int parent_col = ctx->line_pos;
        int saved_line_pos = ctx->line_pos;

        custom_obj_t branch_child = is_rx ? RT_NULL : bind_path_tx_branch_child(children[i]);

        custom_obj_bind_format_path_label(label, sizeof(label), children[i], node, branch_child, is_rx);

        rt_strncpy(saved_prefix, ctx->branch_prefix, sizeof(saved_prefix));
        saved_prefix[sizeof(saved_prefix) - 1] = '\0';

        if (i == 0)
        {
            const char *arrow = (depth == 0) ? ctx->root_arrow : ctx->arrow;

            rt_kprintf("%s%s", arrow, label);
            ctx->line_pos += (int)rt_strlen(arrow) + (int)rt_strlen(label);
        }
        else
        {
            bind_path_print_sibling(ctx, depth, parent_col, label);
        }

        bind_path_print_tree_recur(children[i], ctx, depth + 1, is_rx);
        rt_strncpy(ctx->branch_prefix, saved_prefix, sizeof(ctx->branch_prefix));
        ctx->branch_prefix[sizeof(ctx->branch_prefix) - 1] = '\0';
        ctx->line_pos = saved_line_pos;
    }
}

static void bind_path_init_tree_ctx(struct bind_path_tree_ctx *ctx, custom_obj_t root, rt_bool_t is_rx)
{
    custom_obj_t root_children[BIND_PATH_MAX_DEPTH];
    char root_label[BIND_PATH_LABEL_MAX];
    rt_size_t root_len;

    rt_memset(ctx, 0, sizeof(*ctx));
    ctx->root_arrow = is_rx ? " ----> " : " <---- ";
    ctx->arrow = ctx->root_arrow;
    ctx->branch_arrow = is_rx ? "--->" : "<---";
    ctx->root_multi_branch = bind_path_collect_children(root, is_rx, root_children, BIND_PATH_MAX_DEPTH) > 1;

    custom_obj_bind_format_path_label(root_label, sizeof(root_label), root, RT_NULL, RT_NULL, is_rx);
    root_len = rt_strnlen(root_label, sizeof(root_label));
    if (root_len + 1 < sizeof(ctx->branch_prefix))
    {
        rt_memset(ctx->branch_prefix, ' ', root_len + 1);
        ctx->branch_prefix[root_len + 1] = '\0';
    }

    rt_kprintf("%s", root_label);
    ctx->line_pos = (int)root_len;
}

static void bind_path_print_tree(custom_obj_t root, rt_bool_t is_rx)
{
    struct bind_path_tree_ctx ctx;

    bind_path_init_tree_ctx(&ctx, root, is_rx);
    bind_path_print_tree_recur(root, &ctx, 0, is_rx);
    rt_kprintf("\n");
}

static void bind_path_print_one(custom_obj_t obj)
{
    rt_kprintf("RX:\n");
    bind_path_print_tree(obj, RT_TRUE);
    rt_kprintf("TX:\n");
    bind_path_print_tree(obj, RT_FALSE);
}

static rt_bool_t bind_path_is_parent_obj(const char *name)
{
    rt_size_t len = rt_strnlen(name, RT_NAME_MAX);

    return len >= 7 && rt_strncmp(name + len - 7, "_parent", 7) == 0;
}

static rt_err_t bind_path_parse_type_keyword(const char *name,
                                             enum custom_object_class_type *type_out)
{
    static const struct { const char *keyword; enum custom_object_class_type type; } map[] =
    {
        { "interface", Custom_Object_Class_Interface },
        { "protocol",  Custom_Object_Class_Protocol },
        { "upload",    Custom_Object_Class_Upload },
    };

    if (name == RT_NULL || type_out == RT_NULL)
        return -RT_EINVAL;

    for (int i = 0; i < (int)(sizeof(map) / sizeof(map[0])); i++)
    {
        if (rt_strcmp(name, map[i].keyword) == 0)
        {
            *type_out = map[i].type;
            return RT_EOK;
        }
    }

    return -RT_EINVAL;
}

struct bind_path_list_ctx
{
    enum custom_object_class_type type;
    const char *type_name;
    int count;
    rt_bool_t first;
};

static rt_err_t bind_path_list_iter(struct rt_object *object, void *data)
{
    struct bind_path_list_ctx *ctx = data;
    custom_obj_t obj = (custom_obj_t)object;

    if (ctx == RT_NULL || object == RT_NULL || obj->type != ctx->type
        || bind_path_is_parent_obj(object->name))
        return RT_EOK;

    if (!ctx->first)
        rt_kprintf("\n");

    ctx->first = RT_FALSE;
    ctx->count++;
    bind_path_print_one(obj);

    return RT_EOK;
}

static int bind_path_list_by_type(enum custom_object_class_type type, const char *type_name)
{
    struct bind_path_list_ctx ctx =
    {
        .type = type,
        .type_name = type_name,
        .first = RT_TRUE,
    };

    if (rt_object_get_information(RT_Object_Class_Custom) == RT_NULL)
    {
        rt_kprintf("No custom object registered\n");
        return -RT_ERROR;
    }

    rt_object_for_each(RT_Object_Class_Custom, bind_path_list_iter, &ctx);

    if (ctx.count == 0)
    {
        rt_kprintf("No %s object found\n", type_name);
        return -RT_ERROR;
    }

    return RT_EOK;
}

static int cmd_list_bind_path(int argc, char *argv[])
{
    enum custom_object_class_type type;
    custom_obj_t obj;

    if (argc < 2)
        return bind_path_list_by_type(Custom_Object_Class_Interface, "interface");

    if (bind_path_parse_type_keyword(argv[1], &type) == RT_EOK)
        return bind_path_list_by_type(type, argv[1]);

    obj = (custom_obj_t)rt_object_find(argv[1], RT_Object_Class_Custom);
    if (obj == RT_NULL)
    {
        rt_kprintf("Object not found: %s\n", argv[1]);
        return -RT_ERROR;
    }

    bind_path_print_one(obj);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_list_bind_path, list_bind_path, List bind paths: list_bind_path [obj|interface|protocol|upload]);