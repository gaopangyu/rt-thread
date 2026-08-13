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
 * /ramfs/log/site1/20260714/fire.log
 *   -> /diskfs/log/site1/20260714/fire.log
 * 相对路径相同，只换挂载根。
 */
static int ramfs_path_to_diskfs(const char *src, char *dst, rt_size_t dst_sz)
{
    const char *rel;

    if (src == RT_NULL || dst == RT_NULL || dst_sz < 2)
        return -1;

    if (rt_strncmp(src, RAMFS_ROOT, sizeof(RAMFS_ROOT) - 1) != 0)
        return -1;

    rel = src + (sizeof(RAMFS_ROOT) - 1);
    if (*rel == '/')
        rel++;
    if (*rel == '\0')
        return -1;

    if (rt_snprintf(dst, dst_sz, "%s/%s", DISKFS_ROOT, rel) >= (int)dst_sz)
        return -1;

    return 0;
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
    char prefix[RT_NAME_MAX];
    rt_size_t prefix_len;
    const char *ramfs_path;

    if (obj == RT_NULL || obj->ramfs_fd_mutex == RT_NULL || obj->diskfs_fd_mutex == RT_NULL)
        return;

    ramfs_path = obj->ramfs_path;
    if (ramfs_path[0] == '\0')
        return;

    if (rt_strncmp(ramfs_path, RAMFS_ROOT, sizeof(RAMFS_ROOT) - 1) != 0)
        return;

    {
        const char *rest = ramfs_path + (sizeof(RAMFS_ROOT) - 1);
        if (*rest == '/')
            rest++;
        rt_strncpy(prefix, rest, sizeof(prefix) - 1);
        prefix[sizeof(prefix) - 1] = '\0';
    }
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

    /* 每次只处理一个匹配项并重新 opendir，避免：
     * 1) 边 readdir 边 unlink 跳过文件
     * 2) 栈上缓存大量文件名导致溢出 HardFault
     */
    for (;;)
    {
        DIR *dir;
        struct dirent *dent;
        char name[RT_NAME_MAX];
        char src[STORE_PATH_MAX];
        char dst[STORE_PATH_MAX];
        int found = 0;

        dir = opendir(RAMFS_ROOT);
        if (dir == RT_NULL)
        {
            LOG_E("opendir %s failed", RAMFS_ROOT);
            break;
        }

        while ((dent = readdir(dir)) != RT_NULL)
        {
            rt_size_t name_len;

            if (dent->d_name[0] == '\0')
                continue;

            name_len = rt_strlen(dent->d_name);
            if (name_len < prefix_len || name_len >= RT_NAME_MAX)
                continue;
            if (rt_strncmp(dent->d_name, prefix, prefix_len) != 0)
                continue;
            if (dent->d_name[prefix_len] != '\0' && dent->d_name[prefix_len] != '/')
                continue;

            rt_strncpy(name, dent->d_name, RT_NAME_MAX - 1);
            name[RT_NAME_MAX - 1] = '\0';
            found = 1;
            break;
        }
        closedir(dir);

        if (!found)
            break;

        if (rt_snprintf(src, sizeof(src), "%s/%s", RAMFS_ROOT, name) >= (int)sizeof(src))
        {
            LOG_E("src path too long: %s", name);
            break;
        }

        if (ramfs_path_to_diskfs(src, dst, sizeof(dst)) != 0)
        {
            LOG_E("bad ramfs path: %s", src);
            break;
        }

        LOG_I("migrate %s -> %s", src, dst);
        if (store_copy_file(src, dst) != 0)
            break;
        if (unlink(src) != 0)
        {
            LOG_E("unlink %s failed", src);
            break;
        }
    }

    rt_mutex_release(obj->diskfs_fd_mutex);
    rt_mutex_release(obj->ramfs_fd_mutex);
}

/* 从 path 读入 buffer[offset..)，返回本次写入字节；文件不存在返回 0 */
static rt_size_t store_append_file(const char *path, rt_uint8_t *buffer, rt_size_t size, rt_size_t offset)
{
    int fd;
    rt_size_t written = 0;

    if (path == RT_NULL || buffer == RT_NULL || offset >= size)
        return 0;

    fd = open(path, O_RDONLY | O_BINARY, 0);
    if (fd < 0)
        return 0;

    while (offset + written < size)
    {
        ssize_t n = read(fd, buffer + offset + written, size - offset - written);
        if (n <= 0)
            break;
        written += (rt_size_t)n;
    }

    close(fd);
    return written;
}

/*
 * 优先读 ramfs（未迁移），不存在再读 diskfs（已落盘）。
 * 成功打开并读过（含空文件）返回 1，两侧都不存在返回 0。
 */
static int store_read_one_log(upload_store_obj_t obj, const char *date_str, const char *type,
                              rt_uint8_t *buffer, rt_size_t size, rt_size_t *offset)
{
    char path[STORE_PATH_MAX];
    rt_size_t n;

    if (obj->ramfs_path[0] != '\0' &&
        rt_snprintf(path, sizeof(path), "%s/%s/%s", obj->ramfs_path, date_str, type) < (int)sizeof(path))
    {
        int fd = open(path, O_RDONLY | O_BINARY, 0);
        if (fd >= 0)
        {
            close(fd);
            n = store_append_file(path, buffer, size, *offset);
            *offset += n;
            return 1;
        }
    }

    if (obj->diskfs_path[0] != '\0' &&
        rt_snprintf(path, sizeof(path), "%s/%s/%s", obj->diskfs_path, date_str, type) < (int)sizeof(path))
    {
        int fd = open(path, O_RDONLY | O_BINARY, 0);
        if (fd >= 0)
        {
            close(fd);
            n = store_append_file(path, buffer, size, *offset);
            *offset += n;
            return 1;
        }
    }

    return 0;
}

rt_size_t store_get_history(upload_store_obj_t obj, rt_uint8_t *buffer, rt_size_t size, sub_history_t sub_history)
{
    static const char * const log_types[] = { "fire.log", "fault.log", "other.log" };
    const char *only_type = RT_NULL;
    rt_size_t offset = 0;
    rt_size_t files = 0;
    struct tm tm_day;
    time_t start_day;
    time_t end_day;
    time_t start_time = sub_history->start_time;
    time_t end_time = sub_history->end_time;
    rt_size_t count = sub_history->count;
    enum event_type event_type = sub_history->event_type;

    if (obj == RT_NULL || buffer == RT_NULL || size == 0 || start_time > end_time || count == 0)
        return 0;

    if (obj->ramfs_fd_mutex == RT_NULL || obj->diskfs_fd_mutex == RT_NULL)
        return 0;

    if (event_type == 1 || event_type == '0')
        only_type = log_types[0];
    else if (event_type == 2 || event_type == '1')
        only_type = log_types[1];
    else if (event_type != 0)
        only_type = log_types[2];

    time_t t = start_time;
    localtime_r(&t, &tm_day);
    tm_day.tm_hour = 0;
    tm_day.tm_min = 0;
    tm_day.tm_sec = 0;
    tm_day.tm_isdst = -1;
    start_day = mktime(&tm_day);

    t = end_time;
    localtime_r(&t, &tm_day);
    tm_day.tm_hour = 0;
    tm_day.tm_min = 0;
    tm_day.tm_sec = 0;
    tm_day.tm_isdst = -1;
    end_day = mktime(&tm_day);

    if (start_day == (time_t)-1 || end_day == (time_t)-1)
        return 0;

    if (rt_mutex_take(obj->ramfs_fd_mutex, RT_WAITING_FOREVER) != RT_EOK)
        return 0;
    if (rt_mutex_take(obj->diskfs_fd_mutex, RT_WAITING_FOREVER) != RT_EOK)
    {
        rt_mutex_release(obj->ramfs_fd_mutex);
        return 0;
    }

    while (start_day <= end_day && files < count && offset < size)
    {
        char date_str[9];
        int i;

        localtime_r(&start_day, &tm_day);
        rt_snprintf(date_str, sizeof(date_str), "%04d%02d%02d",
                    tm_day.tm_year + 1900, tm_day.tm_mon + 1, tm_day.tm_mday);

        if (only_type != RT_NULL)
        {
            if (store_read_one_log(obj, date_str, only_type, buffer, size, &offset))
                files++;
        }
        else
        {
            for (i = 0; i < (int)(sizeof(log_types) / sizeof(log_types[0])); i++)
            {
                if (files >= count || offset >= size)
                    break;

                if (store_read_one_log(obj, date_str, log_types[i], buffer, size, &offset))
                    files++;
            }
        }

        tm_day.tm_mday += 1;
        tm_day.tm_isdst = -1;
        start_day = mktime(&tm_day);
        if (start_day == (time_t)-1)
            break;
    }

out:
    rt_mutex_release(obj->diskfs_fd_mutex);
    rt_mutex_release(obj->ramfs_fd_mutex);
    return offset;
}

#define STORE_OBJ_MAX   CUSTOM_PER_LAYER_OBJ_MAX

struct store_collect_ctx
{
    upload_obj_t parent;
    upload_store_obj_t objs[STORE_OBJ_MAX];
    int count;
};

static rt_err_t store_upload_store_collect(struct rt_object *object, void *data)
{
    struct store_collect_ctx *ctx = (struct store_collect_ctx *)data;
    upload_obj_t upload_obj;
    custom_obj_t custom_obj;
    rt_size_t name_len;

    if (object == RT_NULL || ctx == RT_NULL || ctx->parent == RT_NULL)
        return RT_EOK;

    custom_obj = (custom_obj_t)object;
    upload_obj = (upload_obj_t)object;

    if (custom_obj->type != Custom_Object_Class_Upload)
        return RT_EOK;
    if (upload_obj->ops != ctx->parent->ops)
        return RT_EOK;
    if (object == (rt_object_t)ctx->parent)
        return RT_EOK;

    name_len = rt_strnlen(object->name, RT_NAME_MAX);
    if (name_len >= 7 && rt_strncmp(object->name + name_len - 7, "_parent", 7) == 0)
        return RT_EOK;

    if (rt_strncmp(object->name, "upload_store", 12) != 0)
        return RT_EOK;

    /* for_each 持有 spinlock/critical，这里只能收集指针，禁止 take mutex / 调 DFS */
    if (ctx->count < STORE_OBJ_MAX)
        ctx->objs[ctx->count++] = (upload_store_obj_t)object;
    else
        LOG_W("upload_store count > %d, skip %s", STORE_OBJ_MAX, object->name);

    return RT_EOK;
}

static void store_ramfs_to_diskfs(void)
{
    struct store_collect_ctx ctx;
    int i;

    ctx.parent = (upload_obj_t)rt_object_find(UPLOAD_STORE_PARENT_NAME, RT_Object_Class_Custom);
    ctx.count = 0;
    if (ctx.parent == RT_NULL || ctx.parent->ops == RT_NULL)
    {
        LOG_W("upload_store parent not ready");
        return;
    }

    if (rt_object_get_information(RT_Object_Class_Custom) == RT_NULL)
        return;

    rt_object_for_each(RT_Object_Class_Custom, store_upload_store_collect, &ctx);

    for (i = 0; i < ctx.count; i++)
        store_migrate_one(ctx.objs[i]);
}

static void store_run_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        rt_err_t err = rt_sem_take(&store_run_sem, 60 *60 * 1000);
        if (err != RT_EOK && err != -RT_ETIMEOUT)
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
                                      6144,
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
                                      6144,
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
INIT_ENV_EXPORT(store_init);

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


