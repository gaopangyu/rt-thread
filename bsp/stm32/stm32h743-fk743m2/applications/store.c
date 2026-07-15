#include "store.h"
#include "upload_store.h"
#include "upload_obj.h"

#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

#define DBG_TAG "store"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define RAMFS_MOUNT             "/ramfs"
#define DISKFS_ROOT             "/diskfs"
#define STORE_PATH_MAX          256
#define STORE_COPY_CHUNK        512

static struct rt_semaphore store_run_sem;
static struct rt_semaphore store_run_now_sem;

int ensure_dir(const char *path)
{
    struct stat st;

    if (path == RT_NULL || path[0] == '\0')
        return -1;

    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode) ? 0 : -1;

    return mkdir(path, 0x777);
}

/* 创建 filepath 的各级父目录（不含文件名本身） */
static int ensure_parent_dirs(const char *filepath)
{
    char tmp[STORE_PATH_MAX];
    rt_size_t len;
    char *p;

    if (filepath == RT_NULL)
        return -1;

    len = rt_strnlen(filepath, sizeof(tmp) - 1);
    if (len == 0 || len >= sizeof(tmp))
        return -1;

    rt_memcpy(tmp, filepath, len);
    tmp[len] = '\0';

    p = strrchr(tmp, '/');
    if (p == RT_NULL || p == tmp)
        return 0;
    *p = '\0';

    for (p = tmp + 1; *p != '\0'; p++)
    {
        if (*p != '/')
            continue;
        *p = '\0';
        if (ensure_dir(tmp) != 0)
            return -1;
        *p = '/';
    }

    return ensure_dir(tmp);
}

/*
 * /ramfs<log><site1><20260714><fire.log>
 *   -> /diskfs/log/site1/20260714/fire.log
 */
static int angle_path_to_diskfs(const char *src, char *dst, rt_size_t dst_sz)
{
    const char *p;
    rt_size_t n;

    if (src == RT_NULL || dst == RT_NULL || dst_sz < 2)
        return -1;

    p = src;
    if (rt_strncmp(p, RAMFS_MOUNT, sizeof(RAMFS_MOUNT) - 1) == 0)
        p += sizeof(RAMFS_MOUNT) - 1;

    n = rt_snprintf(dst, dst_sz, "%s", DISKFS_ROOT);
    if (n >= dst_sz)
        return -1;

    while (*p == '<')
    {
        const char *end;
        rt_size_t seg_len;

        p++;
        end = strchr(p, '>');
        if (end == RT_NULL || end == p)
            return -1;

        seg_len = (rt_size_t)(end - p);
        if (n + 1 + seg_len >= dst_sz)
            return -1;

        dst[n++] = '/';
        rt_memcpy(dst + n, p, seg_len);
        n += seg_len;
        dst[n] = '\0';
        p = end + 1;
    }

    return (*p == '\0' && n > sizeof(DISKFS_ROOT) - 1) ? 0 : -1;
}

static int store_copy_file(const char *src, const char *dst)
{
    int sfd = -1;
    int dfd = -1;
    int ret = -1;
    char buf[STORE_COPY_CHUNK];
    ssize_t nr;

    if (ensure_parent_dirs(dst) != 0)
    {
        LOG_E("ensure parent dirs for %s failed", dst);
        return -1;
    }

    sfd = open(src, O_RDONLY, 0);
    if (sfd < 0)
    {
        LOG_E("open src %s failed", src);
        return -1;
    }

    dfd = open(dst, O_WRONLY | O_CREAT | O_APPEND, 0);
    if (dfd < 0)
    {
        LOG_E("open dst %s failed", dst);
        close(sfd);
        return -1;
    }

    while ((nr = read(sfd, buf, sizeof(buf))) > 0)
    {
        if (write(dfd, buf, (size_t)nr) != nr)
        {
            LOG_E("write %s failed", dst);
            goto out;
        }
    }
    if (nr < 0)
    {
        LOG_E("read %s failed", src);
        goto out;
    }

    ret = 0;

out:
    close(dfd);
    close(sfd);
    return ret;
}

static void store_migrate_one(upload_store_obj_t obj)
{
    DIR *dir;
    struct dirent *dent;
    char prefix[RT_NAME_MAX];
    rt_size_t prefix_len;
    const char *ramfs_path;

    if (obj == RT_NULL || obj->ramfs_fd_mutex == RT_NULL || obj->diskfs_fd_mutex == RT_NULL)
        return;

    ramfs_path = obj->ramfs_path;
    if (ramfs_path[0] == '\0')
        return;

    if (rt_strncmp(ramfs_path, RAMFS_MOUNT, sizeof(RAMFS_MOUNT) - 1) != 0)
        return;

    rt_strncpy(prefix, ramfs_path + (sizeof(RAMFS_MOUNT) - 1), sizeof(prefix) - 1);
    prefix[sizeof(prefix) - 1] = '\0';
    prefix_len = rt_strlen(prefix);
    if (prefix_len == 0)
        return;

    if (rt_mutex_take(obj->ramfs_fd_mutex, RT_WAITING_FOREVER) != RT_EOK)
        return;
    if (rt_mutex_take(obj->diskfs_fd_mutex, RT_WAITING_FOREVER) != RT_EOK)
    {
        rt_mutex_release(obj->ramfs_fd_mutex);
        return;
    }

    dir = opendir(RAMFS_MOUNT);
    if (dir == RT_NULL)
    {
        LOG_E("opendir %s failed", RAMFS_MOUNT);
        goto unlock;
    }

    while ((dent = readdir(dir)) != RT_NULL)
    {
        char src[STORE_PATH_MAX];
        char dst[STORE_PATH_MAX];
        rt_size_t name_len;

        if (dent->d_name[0] == '\0')
            continue;

        name_len = rt_strlen(dent->d_name);
        if (name_len < prefix_len)
            continue;
        if (rt_strncmp(dent->d_name, prefix, prefix_len) != 0)
            continue;
        /* 避免 site1 匹配到 site12：前缀后必须是下一段 <... > 或结束 */
        if (dent->d_name[prefix_len] != '\0' && dent->d_name[prefix_len] != '<')
            continue;

        /* 扁平名：/ramfs + <log><path>...（中间无额外 '/'） */
        if (rt_snprintf(src, sizeof(src), "%s%s", RAMFS_MOUNT, dent->d_name) >= (int)sizeof(src))
        {
            LOG_E("src path too long: %s", dent->d_name);
            continue;
        }

        if (angle_path_to_diskfs(src, dst, sizeof(dst)) != 0)
        {
            LOG_E("bad angle path: %s", src);
            continue;
        }

        LOG_I("migrate %s -> %s", src, dst);
        if (store_copy_file(src, dst) == 0)
        {
            if (unlink(src) != 0)
                LOG_E("unlink %s failed", src);
        }
    }

    closedir(dir);

unlock:
    rt_mutex_release(obj->diskfs_fd_mutex);
    rt_mutex_release(obj->ramfs_fd_mutex);
}

static rt_err_t store_upload_store_iter(struct rt_object *object, void *data)
{
    upload_obj_t parent = (upload_obj_t)data;
    upload_obj_t upload_obj;
    custom_obj_t custom_obj;
    rt_size_t name_len;

    if (object == RT_NULL || parent == RT_NULL)
        return RT_EOK;

    custom_obj = (custom_obj_t)object;
    upload_obj = (upload_obj_t)object;

    if (custom_obj->type != Custom_Object_Class_Upload)
        return RT_EOK;
    if (upload_obj->ops != parent->ops)
        return RT_EOK;
    if (object == (rt_object_t)parent)
        return RT_EOK;

    name_len = rt_strnlen(object->name, RT_NAME_MAX);
    if (name_len >= 7 && rt_strncmp(object->name + name_len - 7, "_parent", 7) == 0)
        return RT_EOK;

    store_migrate_one((upload_store_obj_t)object);
    return RT_EOK;
}

static void store_ramfs_to_diskfs(void)
{
    upload_obj_t parent;

    parent = (upload_obj_t)rt_object_find(UPLOAD_STORE_PARENT_NAME, RT_Object_Class_Custom);
    if (parent == RT_NULL || parent->ops == RT_NULL)
    {
        LOG_W("upload_store parent not ready");
        return;
    }

    if (rt_object_get_information(RT_Object_Class_Custom) == RT_NULL)
        return;

    rt_object_for_each(RT_Object_Class_Custom, store_upload_store_iter, parent);
}

static void store_run_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        if (rt_sem_take(&store_run_sem, RT_WAITING_FOREVER) != RT_EOK)
        {
            rt_thread_mdelay(5000);
            continue;
        }

        LOG_I("store run ! ! !\r\n");
        store_ramfs_to_diskfs();
    }
}

static void store_run_now_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        if (rt_sem_take(&store_run_now_sem, RT_WAITING_FOREVER) != RT_EOK)
        {
            rt_thread_mdelay(5000);
            continue;
        }

        LOG_I("store run now ! ! !\r\n");
        store_ramfs_to_diskfs();
    }
}

static int store_init(void)
{
    RT_ASSERT(rt_sem_init(&store_run_sem, "store_run_sem", 0, RT_IPC_FLAG_FIFO) == RT_EOK);
    RT_ASSERT(rt_sem_init(&store_run_now_sem, "store_run_now_sem", 0, RT_IPC_FLAG_FIFO) == RT_EOK);

    rt_thread_t run_tid = rt_thread_create("store_run_thread",
                                      store_run_entry,
                                      RT_NULL,
                                      4096,
                                      RT_THREAD_PRIORITY_MAX - 5,
                                      20);
    if (run_tid == RT_NULL)
    {
        LOG_E("store_run: thread create error");
        return -RT_ERROR;
    }
    rt_thread_startup(run_tid);

    rt_thread_t run_now_tid = rt_thread_create("store_run_now_thread",
                                      store_run_now_entry,
                                      RT_NULL,
                                      4096,
                                      5,
                                      20);
    if (run_now_tid == RT_NULL)
    {
        LOG_E("store_run_now: thread create error");
        return -RT_ERROR;
    }
    rt_thread_startup(run_now_tid);

    return RT_EOK;
}
INIT_APP_EXPORT(store_init);

void store_run(void)
{
    rt_sem_release(&store_run_sem);
}

void store_run_now(void)
{
    rt_sem_release(&store_run_now_sem);
}

static int cmd_store_run(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    store_run();
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_store_run, store_run, migrate ramfs logs to diskfs);

static int cmd_store_run_now(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    store_run_now();
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_store_run_now, store_run_now, migrate ramfs logs to diskfs now);
