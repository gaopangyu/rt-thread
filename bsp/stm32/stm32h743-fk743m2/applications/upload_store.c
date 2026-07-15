#include <fal.h>
#include <dfs_fs.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "upload_store.h"
#include "bind.h"
#include "store.h"

#define DBG_TAG "upload_store"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define RAM_STORE_BASE          "/ramfs/log"   /* ramfs 仍扁平：相对路径里的 '/' 是文件名的一部分，勿 mkdir */

static void upload_store_recv_sub_event(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    upload_store_obj_t obj = (upload_store_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;

    if(custom_obj->rxb.sub_mb == RT_NULL)
        return;

    while (1)
    {
        while(custom_obj->rxb.sub_event == RT_NULL || custom_obj->rxb.pre_event_mask == 0)
            rt_thread_mdelay(100);

        rt_uint32_t recv_event_flag = 0;
        if(rt_event_recv(custom_obj->rxb.sub_event, custom_obj->rxb.pre_event_mask, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR , RT_WAITING_FOREVER, &recv_event_flag) != RT_EOK)
        {
            if (custom_obj->rxb.sub_event == RT_NULL || custom_obj->rxb.pre_event_mask == 0 || recv_event_flag == 0)
                continue;
            LOG_E("%s recv tx_sub_event error: %d/%d\r\n", rt_obj->name, recv_event_flag, custom_obj->rxb.pre_event_mask);
            break;
        }

        if (rt_mb_send(custom_obj->rxb.sub_mb, recv_event_flag) != RT_EOK)
        {
            LOG_E("%s send tx_sub_mb error: %d\r\n", rt_obj->name, recv_event_flag);
            break;
        }
    }
}

static double_ret upload_store_set_param(upload_obj_t self, void *config)
{
    double_ret result = {0, 0};
    if (self == RT_NULL || config == RT_NULL) 
    {
        result.fail++;
        return result;
    }

    upload_store_config_t cfg = (upload_store_config_t)config;
    upload_store_obj_t obj = (upload_store_obj_t)self;
    upload_store_config_t obj_cfg = &(obj->config);

    char path[RT_NAME_MAX] = {0};
    rt_snprintf(path, sizeof(path), "%s/%s", RAM_STORE_BASE, cfg->path);

    if(rt_mutex_take(obj->ramfs_fd_mutex, RT_WAITING_FOREVER) != RT_EOK)
    {
        result.fail++;
        return result;
    }

    rt_strncpy(obj->config.path, cfg->path, RT_NAME_MAX - 1);
    obj->config.path[RT_NAME_MAX - 1] = 0;
    rt_strncpy(obj->ramfs_path, path, RT_NAME_MAX - 1);
    obj->ramfs_path[RT_NAME_MAX - 1] = 0;

    rt_mutex_release(obj->ramfs_fd_mutex);

    result.succ++;

    return result;
}

static void upload_store_handle_bind_pre(upload_store_obj_t obj, rt_object_t rt_obj, rt_uint8_t *buffer, rt_size_t len)
{
    if (buffer == RT_NULL || len == 0)
        return;

    char path[RT_NAME_MAX] = {0};
    rt_snprintf(path, sizeof(path), "%s/20260714/fire.log", obj->ramfs_path);

    char path1[RT_NAME_MAX] = {0};
    rt_snprintf(path1, sizeof(path), "%s/20260715/fire.log", obj->ramfs_path);

    char path2[RT_NAME_MAX] = {0};
    rt_snprintf(path2, sizeof(path), "%s/20260715/fault.log", obj->ramfs_path);

    if(rt_mutex_take(obj->ramfs_fd_mutex, RT_WAITING_FOREVER) != RT_EOK)
        return;

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_BINARY, 0);
    if (fd < 0)
    {
        LOG_E("%s open %s failed", rt_obj->name, path);
        rt_mutex_release(obj->ramfs_fd_mutex);
        return;
    }

    if (write(fd, buffer, len) != (int)len)
        LOG_E("%s write %s failed, len=%d", rt_obj->name, path, (int)len);

    close(fd);

    fd = open(path1, O_WRONLY | O_CREAT | O_APPEND | O_BINARY, 0);
    if (fd < 0)
    {
        LOG_E("%s open %s failed", rt_obj->name, path1);
        rt_mutex_release(obj->ramfs_fd_mutex);
        return;
    }

    if (write(fd, buffer, len) != (int)len)
        LOG_E("%s write %s failed, len=%d", rt_obj->name, path1, (int)len);

    close(fd);

    fd = open(path2, O_WRONLY | O_CREAT | O_APPEND | O_BINARY, 0);
    if (fd < 0)
    {
        LOG_E("%s open %s failed", rt_obj->name, path2);
        rt_mutex_release(obj->ramfs_fd_mutex);
        return;
    }

    if (write(fd, buffer, len) != (int)len)
        LOG_E("%s write %s failed, len=%d", rt_obj->name, path2, (int)len);

    close(fd);

    rt_mutex_release(obj->ramfs_fd_mutex);

    return;
}

static void upload_store_recv(void *parameter)
{
    if (parameter == RT_NULL)
        return;

    upload_store_obj_t obj = (upload_store_obj_t)parameter;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    upload_obj_t upload_obj = (upload_obj_t)obj;

    if(custom_obj->rxb.sub_mb == RT_NULL)
        return;

    while (1)
    {
        rt_uint32_t recv_event_flag = 0;
        if(rt_mb_recv(custom_obj->rxb.sub_mb, (rt_ubase_t *)&recv_event_flag, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s recv rxb.sub_mb error", rt_obj->name);
            break;
        }

        rt_thread_yield();

        if (rt_mutex_take(custom_obj->rxb.bind_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            LOG_E("%s take rx_bind_mutex error", rt_obj->name);
            break;
        }

        if(custom_obj->event_flag == 0)
        {
            LOG_E("%s event_flag is 0, skip", rt_obj->name);
            rt_mutex_release(custom_obj->rxb.bind_mutex);
            continue;
        }

        while(recv_event_flag != 0)
        {
            custom_obj_t rx_custom_obj = get_obj_from_event_flag(custom_obj, custom_obj->rxb.pre_obj_list, &recv_event_flag, RT_TRUE);
            if(rx_custom_obj == RT_NULL)
                break;

            LOG_D("%s recv from %s: %d\r\n", rt_obj->name, rx_custom_obj->parent.name, rx_custom_obj->rxb.len);
            custom_obj_bind_link_xfer(rx_custom_obj, custom_obj, rx_custom_obj->rxb.len, RT_TRUE);

            if(rx_custom_obj->rxb.pub_succ_event == RT_NULL)
                continue;

            upload_store_handle_bind_pre(obj, rt_obj, rx_custom_obj->rxb.buffer, rx_custom_obj->rxb.len);

            LOG_I("%s ---> %s send rxb.pub_succ_event: 0x%X\r\n", rt_obj->name, rx_custom_obj->parent.name, custom_obj->event_flag);
            if( rt_event_send(rx_custom_obj->rxb.pub_succ_event, custom_obj->event_flag) != RT_EOK )
                LOG_E("%s send rx_pub_succ_event error: %d/%d\r\n", rt_obj->name, custom_obj->event_flag, rx_custom_obj->rxb.next_event_mask);
        }

        rt_mutex_release(custom_obj->rxb.bind_mutex);
    }
}

static rt_err_t upload_store_delete(upload_obj_t self)
{
    if (self == RT_NULL) {
        return -RT_EINVAL;
    }

    upload_store_obj_t obj = (upload_store_obj_t)self;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;

    if(rt_object_find(rt_obj->name, RT_Object_Class_Custom) != RT_NULL)
        rt_object_detach(rt_obj);

    bind_delete_all_for_obj(custom_obj);
    
    if (obj->rx_sub_event_tid != RT_NULL)
    {
        rt_thread_delete(obj->rx_sub_event_tid);
        obj->rx_sub_event_tid = RT_NULL;
    }

    if (obj->rx_tid != RT_NULL)
    {
        rt_thread_delete(obj->rx_tid);
        obj->rx_tid = RT_NULL;
    }

    if(custom_obj->rxb.sub_mb != RT_NULL)
    {
        rt_mb_delete(custom_obj->rxb.sub_mb);
        custom_obj->rxb.sub_mb = RT_NULL;
    }

    if(custom_obj->txb.sub_mb != RT_NULL)
    {
        rt_mb_delete(custom_obj->txb.sub_mb);
        custom_obj->txb.sub_mb = RT_NULL;
    }

    if(custom_obj->rxb.bind_mutex != RT_NULL)
    {
        rt_mutex_delete(custom_obj->rxb.bind_mutex);
        custom_obj->rxb.bind_mutex = RT_NULL;
    }

    if(custom_obj->txb.bind_mutex != RT_NULL)
    {
        rt_mutex_delete(custom_obj->txb.bind_mutex);
        custom_obj->txb.bind_mutex = RT_NULL;
    }

    if(custom_obj->rxb.pub_succ_event  != RT_NULL)
    {
        rt_event_delete(custom_obj->rxb.pub_succ_event);
        custom_obj->rxb.pub_succ_event = RT_NULL;
    }

    if(custom_obj->txb.pub_succ_event != RT_NULL)
    {
        rt_event_delete(custom_obj->txb.pub_succ_event);
        custom_obj->txb.pub_succ_event = RT_NULL;
    }

    if(obj->ramfs_fd_mutex != RT_NULL)
    {
        rt_mutex_delete(obj->ramfs_fd_mutex);
        obj->ramfs_fd_mutex = RT_NULL;
    }

    if(obj->diskfs_fd_mutex != RT_NULL)
    {
        rt_mutex_delete(obj->diskfs_fd_mutex);
        obj->diskfs_fd_mutex = RT_NULL;
    }

    rt_free(obj);
    obj = RT_NULL;

    return RT_EOK;
}

static upload_obj_t  upload_store_create(upload_obj_t self, char *name, void *config)
{
    if (self == RT_NULL || name == RT_NULL) {
        LOG_E("create error: param");
        return RT_NULL;
    }

    if (rt_strncmp(self->parent.parent.name, UPLOAD_STORE_PARENT_NAME, RT_NAME_MAX) != 0) {
        LOG_E("create error: invalid parent");
        return RT_NULL;
    }

    if (rt_object_find(name, RT_Object_Class_Custom) != RT_NULL)
    {
        LOG_E("create error: %s exist", name);
        return RT_NULL;
    }

    upload_store_obj_t obj = rt_malloc(sizeof(struct upload_store_obj));
    if (obj == RT_NULL) {
        LOG_E("create error: memory allocation");
        return RT_NULL;
    }

    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    upload_obj_t upload_obj = (upload_obj_t)obj;

    rt_memcpy(obj, self, sizeof(struct upload_store_obj));

    char name_buf[RT_NAME_MAX] = {0};
    
    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_ramfs_fd_mutex");
    if(((obj->ramfs_fd_mutex = rt_mutex_create(name_buf, RT_IPC_FLAG_FIFO)) == RT_NULL)) {
        LOG_E("create error: ramfs_fd_mutex create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_diskfs_fd_mutex");
    if(((obj->diskfs_fd_mutex = rt_mutex_create(name_buf, RT_IPC_FLAG_FIFO)) == RT_NULL)) {
        LOG_E("create error: diskfs_fd_mutex create");
        goto exit;
    }

    if(config == RT_NULL)
        config = &(obj->config);

    double_ret result = upload_store_set_param(upload_obj, config);
    if (result.fail != 0)
    {
        LOG_E("create error: set param");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx");
    obj->rx_tid = rt_thread_create(name_buf, upload_store_recv, obj, 1024, CUSTOM_THREAD_PRIORITY, 20);
    if (obj->rx_tid == RT_NULL) {
        LOG_E("create error: rx thread create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_sub_event");
    if((obj->rx_sub_event_tid = rt_thread_create(name_buf, upload_store_recv_sub_event, obj, 1024, CUSTOM_THREAD_PRIORITY - 1, 20)) == RT_NULL) {
        LOG_E("create error: rx sub event thread create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_bind_mutex");
    if(((custom_obj->rxb.bind_mutex = rt_mutex_create(name_buf, RT_IPC_FLAG_FIFO)) == RT_NULL)) {
        LOG_E("create error: rx mutex create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_tx_bind_mutex");
    if(((custom_obj->txb.bind_mutex = rt_mutex_create(name_buf, RT_IPC_FLAG_FIFO)) == RT_NULL)) {
        LOG_E("create error: tx mutex create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_sub_mb");
    if ((custom_obj->rxb.sub_mb = rt_mb_create(name_buf, CUSTOM_PER_LAYER_OBJ_MAX, RT_IPC_FLAG_FIFO)) == RT_NULL) {
        LOG_E("create error: rx mailbox create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_tx_sub_mb");
    if ((custom_obj->txb.sub_mb = rt_mb_create(name_buf, CUSTOM_PER_LAYER_OBJ_MAX, RT_IPC_FLAG_FIFO)) == RT_NULL) {
        LOG_E("create error: tx mailbox create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_rx_pub_succ");
    if((custom_obj->rxb.pub_succ_event = rt_event_create(name_buf, RT_IPC_FLAG_PRIO)) == RT_NULL) {
        LOG_E("create error: rx pub succ event create");
        goto exit;
    }

    rt_snprintf(name_buf, sizeof(name_buf) - 1, "%s%s", name, "_tx_pub_succ");
    if((custom_obj->txb.pub_succ_event = rt_event_create(name_buf, RT_IPC_FLAG_PRIO)) == RT_NULL) {
        LOG_E("create error: tx pub succ event create");
        goto exit;
    }

    rt_thread_startup(obj->rx_tid);
    rt_thread_startup(obj->rx_sub_event_tid);

    rt_object_init(rt_obj, RT_Object_Class_Custom, name);
    LOG_I("create success: %s", name);
    return upload_obj;

exit:
    upload_store_delete(upload_obj);
    return RT_NULL;
}

static const struct upload_obj_ops upload_store_ops =
{
    .create = upload_store_create,
    .delete = upload_store_delete,
    .set_param = upload_store_set_param,
};

static rt_err_t  upload_store_gc(custom_obj_t self)
{
    if (self == RT_NULL)
        return -RT_EINVAL;

    upload_obj_t upload_obj = (upload_obj_t)self;
    return upload_store_delete(upload_obj);
}

static const struct custom_object_ops custom_obj_ops =
{
    .gc = upload_store_gc,
};

static int upload_store_init(void)
{
    static struct upload_store_obj object = {0};
    upload_store_obj_t obj = &object;
    rt_object_t rt_obj = (rt_object_t)obj;
    custom_obj_t custom_obj = (custom_obj_t)obj;
    upload_obj_t upload_obj = (upload_obj_t)obj;

    if (rt_object_find(UPLOAD_STORE_PARENT_NAME, RT_Object_Class_Custom) != RT_NULL)
        return -RT_ERROR;

    custom_obj->type = Custom_Object_Class_Upload;

    upload_obj_init(custom_obj);

    custom_obj->ops = &custom_obj_ops;
    upload_obj->ops = &upload_store_ops;

    rt_strncpy(obj->config.path, "temp", rt_strnlen("temp", RT_NAME_MAX - 1));

    rt_object_init(rt_obj, RT_Object_Class_Custom, UPLOAD_STORE_PARENT_NAME);

    return RT_EOK;
}

INIT_APP_EXPORT(upload_store_init);

static int cmd_create_upload_store(int argc, char *argv[])
{
    if (argc < 2)
    {
        rt_kprintf("Usage: create_upload_store <name> -p <path>\n");
        rt_kprintf("Example: create_upload_store upload_store_1 -p site1\n");
        return -RT_ERROR;
    }

    const char *name = argv[1];
    rt_err_t err = custom_obj_msh_check_create_name(name, "upload_store");

    if (err == -RT_EFULL)
    {
        rt_kprintf("Object already exists: %s\n", name);
        return -RT_ERROR;
    }
    if (err != RT_EOK)
    {
        rt_kprintf("Invalid name: %s\n", name);
        return -RT_ERROR;
    }

    upload_obj_t parent = (upload_obj_t)rt_object_find(UPLOAD_STORE_PARENT_NAME, RT_Object_Class_Custom);
    if (parent == RT_NULL || parent->ops == RT_NULL || parent->ops->create == RT_NULL)
    {
        rt_kprintf("Parent not ready: %s\n", UPLOAD_STORE_PARENT_NAME);
        return -RT_ERROR;
    }

    const char *path = custom_obj_msh_get_opt(argc, argv, "p");
    if (path == RT_NULL || path[0] == '\0')
    {
        rt_kprintf("Usage: create_upload_store <name> -p <path>\n");
        rt_kprintf("Example: create_upload_store upload_store_1 -p site1\n");
        return -RT_ERROR;
    }

    struct upload_store_config config = {0};
    rt_strncpy(config.path, path, RT_NAME_MAX - 1);
    config.path[RT_NAME_MAX - 1] = '\0';

    if (parent->ops->create(parent, (char *)name, &config) == RT_NULL)
    {
        rt_kprintf("Failed to create upload_store: %s\n", name);
        return -RT_ERROR;
    }

    rt_kprintf("Created upload_store: %s path=%s\n", name, config.path);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_create_upload_store, create_upload_store, Create upload_store object);

static int cmd_delete_upload_store(int argc, char *argv[])
{
    const char *name = custom_obj_msh_get_opt(argc, argv, "n");

    if (name == RT_NULL)
    {
        rt_kprintf("Usage: delete_upload_store -n <name>\n");
        rt_kprintf("Example: delete_upload_store -n upload_store_1\n");
        return -RT_ERROR;
    }

    if (custom_obj_msh_check_delete_name(name) != RT_EOK)
    {
        rt_kprintf("Invalid name: %s\n", name);
        return -RT_ERROR;
    }

    upload_obj_t obj = (upload_obj_t)rt_object_find(name, RT_Object_Class_Custom);
    if (obj == RT_NULL)
    {
        rt_kprintf("Object not found: %s\n", name);
        return -RT_ERROR;
    }

    if (((custom_obj_t)obj)->type != Custom_Object_Class_Upload)
    {
        rt_kprintf("Not a upload_store object: %s\n", name);
        return -RT_ERROR;
    }

    if (obj->ops == RT_NULL || obj->ops->delete == RT_NULL)
    {
        rt_kprintf("Delete not supported: %s\n", name);
        return -RT_ERROR;
    }

    if (obj->ops->delete(obj) != RT_EOK)
    {
        rt_kprintf("Failed to delete upload_store: %s\n", name);
        return -RT_ERROR;
    }

    rt_kprintf("Deleted upload_store: %s\n", name);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_delete_upload_store, delete_upload_store, Delete upload_store object);
