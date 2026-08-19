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

#define STORE_PATH_MAX              256
#define STORE_COPY_CHUNK            512
#define STORE_EVENT_SIZE            32

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
 * /ramfs/event/site1/20260714/fire.event
 *   -> /diskfs/event/site1/20260714/fire.event
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
    //存事件
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

    //存错误日志log
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
MSH_CMD_EXPORT_ALIAS(cmd_store_run, store_run, migrate ramfs events to diskfs);

static int cmd_store_run_now(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    store_run_now();
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_store_run_now, store_run_now, migrate ramfs events to diskfs now);

/////////////////////////////////////////////get history/////////////////////////////////////////////

static time_t store_day_floor(time_t t)
{
    struct tm tm_day;

    localtime_r(&t, &tm_day);
    tm_day.tm_hour = 0;
    tm_day.tm_min = 0;
    tm_day.tm_sec = 0;
    tm_day.tm_isdst = -1;
    return mktime(&tm_day);
}

static void store_date_str(time_t day, char date_str[9])
{
    struct tm tm_day;

    localtime_r(&day, &tm_day);
    rt_snprintf(date_str, 9, "%04d%02d%02d",
                tm_day.tm_year + 1900, tm_day.tm_mon + 1, tm_day.tm_mday);
}

static int store_parse_event_date(const char *name, rt_uint32_t *date)
{
    rt_size_t i;
    rt_uint32_t value = 0;

    if (name == RT_NULL || date == RT_NULL)
        return -1;
    if (rt_strlen(name) != 14 || rt_strcmp(name + 8, ".event") != 0)
        return -1;

    for (i = 0; i < 8; i++)
    {
        if (name[i] < '0' || name[i] > '9')
            return -1;
        value = value * 10 + (rt_uint32_t)(name[i] - '0');
    }

    *date = value;
    return 0;
}

static rt_uint32_t store_scan_dir_prev_date(const char *dir_path, rt_uint32_t upper)
{
    DIR *dir;
    struct dirent *dent;
    rt_uint32_t best = 0;

    if (dir_path == RT_NULL || dir_path[0] == '\0')
        return 0;

    dir = opendir(dir_path);
    if (dir == RT_NULL)
        return 0;

    while ((dent = readdir(dir)) != RT_NULL)
    {
        rt_uint32_t date;

        if (store_parse_event_date(dent->d_name, &date) != 0)
            continue;
        if (date < upper && date > best)
            best = date;
    }

    closedir(dir);
    return best;
}

static rt_uint32_t store_scan_ramfs_prev_date(upload_store_obj_t obj, rt_uint32_t upper)
{
    DIR *dir;
    struct dirent *dent;
    char prefix[RT_NAME_MAX];
    rt_size_t prefix_len;
    const char *rest;
    rt_uint32_t best = 0;

    if (obj->ramfs_path[0] == '\0' ||
        rt_strncmp(obj->ramfs_path, RAMFS_ROOT, sizeof(RAMFS_ROOT) - 1) != 0)
        return 0;

    rest = obj->ramfs_path + (sizeof(RAMFS_ROOT) - 1);
    if (*rest == '/')
        rest++;
    rt_strncpy(prefix, rest, sizeof(prefix) - 1);
    prefix[sizeof(prefix) - 1] = '\0';
    prefix_len = rt_strlen(prefix);
    if (prefix_len == 0)
        return 0;

    dir = opendir(RAMFS_ROOT);
    if (dir == RT_NULL)
        return 0;

    while ((dent = readdir(dir)) != RT_NULL)
    {
        rt_size_t name_len;
        const char *fname;
        rt_uint32_t date;

        name_len = rt_strlen(dent->d_name);
        if (name_len < prefix_len + 1 + 14)
            continue;
        if (rt_strncmp(dent->d_name, prefix, prefix_len) != 0)
            continue;
        if (dent->d_name[prefix_len] != '/')
            continue;

        fname = dent->d_name + prefix_len + 1;
        if (store_parse_event_date(fname, &date) != 0)
            continue;
        if (date < upper && date > best)
            best = date;
    }

    closedir(dir);
    return best;
}

/* 在 ramfs/diskfs 已有日期文件中，找小于 upper 的最大 YYYYMMDD；没有返回 0 */
static rt_uint32_t store_find_prev_event_date(upload_store_obj_t obj, rt_uint32_t upper)
{
    rt_uint32_t ramfs_date = store_scan_ramfs_prev_date(obj, upper);
    rt_uint32_t diskfs_date = store_scan_dir_prev_date(obj->diskfs_path, upper);

    return ramfs_date > diskfs_date ? ramfs_date : diskfs_date;
}

/* 优先 ramfs，没有再 diskfs。找到返回 0。 */
static int store_event_file_path(upload_store_obj_t obj, const char *date_str,
                                 char *path, rt_size_t path_sz)
{
    struct stat st;

    if (obj->ramfs_path[0] != '\0' &&
        rt_snprintf(path, path_sz, "%s/%s.event", obj->ramfs_path, date_str) < (int)path_sz &&
        stat(path, &st) == 0 && S_ISREG(st.st_mode))
        return 0;

    if (obj->diskfs_path[0] != '\0' &&
        rt_snprintf(path, path_sz, "%s/%s.event", obj->diskfs_path, date_str) < (int)path_sz &&
        stat(path, &st) == 0 && S_ISREG(st.st_mode))
        return 0;

    return -1;
}

static rt_size_t store_event_file_count(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0 || st.st_size <= 0)
        return 0;

    return (rt_size_t)st.st_size / STORE_EVENT_SIZE;
}

static int store_read_event_at(int fd, rt_size_t ev_idx, rt_uint8_t *ev)
{
    if (lseek(fd, (off_t)(ev_idx * STORE_EVENT_SIZE), SEEK_SET) < 0)
        return -1;
    if (read(fd, ev, STORE_EVENT_SIZE) != STORE_EVENT_SIZE)
        return -1;
    return 0;
}

static rt_uint32_t store_event_id(const rt_uint8_t *ev)
{
    rt_uint32_t id;

    rt_memcpy(&id, ev, sizeof(id));
    return id;
}

static rt_bool_t store_append_event(rt_uint8_t *buffer, rt_size_t size, rt_size_t *offset,
                                    const rt_uint8_t *ev)
{
    if (*offset + STORE_EVENT_SIZE > size)
        return RT_FALSE;

    rt_memcpy(buffer + *offset, ev, STORE_EVENT_SIZE);
    *offset += STORE_EVENT_SIZE;
    return RT_TRUE;
}

/* 从文件 ev_off 起按时间递增读到文件末尾，返回读到的事件数 */
static rt_size_t store_copy_events_forward(const char *path, rt_size_t ev_off, rt_size_t remain,
                                           rt_uint8_t *buffer, rt_size_t size, rt_size_t *offset)
{
    int fd;
    rt_size_t total;
    rt_size_t copied = 0;
    rt_uint8_t ev[STORE_EVENT_SIZE];

    total = store_event_file_count(path);
    if (total == 0 || ev_off >= total || remain == 0)
        return 0;

    fd = open(path, O_RDONLY | O_BINARY, 0);
    if (fd < 0)
        return 0;

    while (ev_off < total && copied < remain)
    {
        if (store_read_event_at(fd, ev_off, ev) != 0)
            break;
        if (!store_append_event(buffer, size, offset, ev))
            break;
        ev_off++;
        copied++;
    }

    close(fd);
    return copied;
}

/* 从文件 ev_off 起按时间递减读到文件头，返回读到的事件数 */
static rt_size_t store_copy_events_backward(const char *path, rt_size_t ev_off, rt_size_t remain,
                                            rt_uint8_t *buffer, rt_size_t size, rt_size_t *offset)
{
    int fd;
    rt_size_t total;
    rt_size_t copied = 0;
    rt_uint8_t ev[STORE_EVENT_SIZE];

    total = store_event_file_count(path);
    if (total == 0 || ev_off >= total || remain == 0)
        return 0;

    fd = open(path, O_RDONLY | O_BINARY, 0);
    if (fd < 0)
        return 0;

    while (copied < remain)
    {
        if (store_read_event_at(fd, ev_off, ev) != 0)
            break;
        if (!store_append_event(buffer, size, offset, ev))
            break;
        copied++;
        if (ev_off == 0)
            break;
        ev_off--;
    }

    close(fd);
    return copied;
}

static int store_find_event_index(const char *path, rt_size_t event_index, rt_size_t *ev_off)
{
    int fd;
    rt_size_t total;
    rt_size_t i;
    rt_uint8_t ev[STORE_EVENT_SIZE];

    total = store_event_file_count(path);
    if (total == 0)
        return -1;

    fd = open(path, O_RDONLY | O_BINARY, 0);
    if (fd < 0)
        return -1;

    for (i = total; i > 0; i--)
    {
        if (store_read_event_at(fd, i - 1, ev) != 0)
            continue;
        if (store_event_id(ev) == (rt_uint32_t)event_index)
        {
            *ev_off = i - 1;
            close(fd);
            return 0;
        }
    }

    close(fd);
    return -1;
}

static rt_err_t store_history_lock(upload_store_obj_t obj)
{
    if (obj->ramfs_fd_mutex == RT_NULL || obj->diskfs_fd_mutex == RT_NULL)
        return -RT_ERROR;

    if (rt_mutex_take(obj->ramfs_fd_mutex, RT_WAITING_FOREVER) != RT_EOK)
        return -RT_ERROR;
    if (rt_mutex_take(obj->diskfs_fd_mutex, RT_WAITING_FOREVER) != RT_EOK)
    {
        rt_mutex_release(obj->ramfs_fd_mutex);
        return -RT_ERROR;
    }

    return RT_EOK;
}

static void store_history_unlock(upload_store_obj_t obj)
{
    rt_mutex_release(obj->diskfs_fd_mutex);
    rt_mutex_release(obj->ramfs_fd_mutex);
}

/*
 * 扫描文件夹中已有的 YYYYMMDD.event，从日期最大的开始递减查找 event_index
 * （每条 32B，前 4B 为编号），找到后按时间递减继续读，直到 count 条。
 */
static rt_size_t store_get_history_by_index(upload_store_obj_t obj, rt_uint8_t *buffer,
                                            rt_size_t size, event_history_t event_history)
{
    rt_size_t offset = 0;
    rt_size_t remain = event_history->count;
    rt_uint32_t last_date = 100000000; /* 大于任意 YYYYMMDD */
    rt_bool_t found = RT_FALSE;

    if (remain == 0)
        return 0;

    if (store_history_lock(obj) != RT_EOK)
        return 0;

    while (remain > 0)
    {
        char date_str[9];
        char path[STORE_PATH_MAX];
        rt_uint32_t date;
        rt_size_t ev_off = 0;
        rt_size_t n;

        date = store_find_prev_event_date(obj, last_date);
        if (date == 0)
            break;
        last_date = date;

        rt_snprintf(date_str, sizeof(date_str), "%08u", date);
        if (store_event_file_path(obj, date_str, path, sizeof(path)) != 0)
            continue;

        if (!found)
        {
            if (store_find_event_index(path, event_history->fire_index, &ev_off) != 0)
                continue;
            found = RT_TRUE;
        }
        else
        {
            n = store_event_file_count(path);
            if (n == 0)
                continue;
            ev_off = n - 1;
        }

        remain -= store_copy_events_backward(path, ev_off, remain, buffer, size, &offset);
    }

    store_history_unlock(obj);
    return offset;
}

/*
 * 只读 start_time 当天文件，从 start_time_offset 条事件起往后读，
 * 直到文件结尾或 count 条。
 */
static rt_size_t store_get_history_by_date(upload_store_obj_t obj, rt_uint8_t *buffer,
                                           rt_size_t size, event_history_t event_history)
{
    rt_size_t offset = 0;
    time_t start_day;
    char date_str[9];
    char path[STORE_PATH_MAX];

    return offset;
}

rt_size_t store_get_history(upload_store_obj_t obj, rt_uint8_t *buffer, rt_size_t size, event_history_t event_history)
{
    if (obj == RT_NULL || buffer == RT_NULL || size == 0 || event_history == RT_NULL)
        return 0;

    if (event_history->date != 0)
        return store_get_history_by_index(obj, buffer, size, event_history);

    return store_get_history_by_date(obj, buffer, size, event_history);
}

void get_last_date_file(char *dir, char *date, rt_size_t len)
{
    rt_memcpy(date, "20260819", len);
}