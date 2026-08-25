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

#define STORE_COPY_CHUNK            512

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
    char tmp[RT_NAME_MAX];
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
    static char buf[STORE_COPY_CHUNK];
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
        char src[RT_NAME_MAX];
        char dst[RT_NAME_MAX];
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

#define STORE_EVENT_SIZE            sizeof(struct event_num_pack)
#define STORE_DATE_ALL              0x00000000u
#define STORE_DATE_LATEST           0xFFFFFFFFu
#define STORE_TYPE_ALL              0x0Fu
#define STORE_DATE_CEIL             100000000u

static const enum event_file_type store_history_types[] =
{
    FILE_TYPE_RESET, FILE_TYPE_FIRE, FILE_TYPE_MONITOR, FILE_TYPE_FAULT,
    FILE_TYPE_ACTION, FILE_TYPE_LINKAGE, FILE_TYPE_MASK, FILE_TYPE_OTHER,
    FILE_TYPE_CTRL,
};
#define STORE_HISTORY_TYPE_CNT     (sizeof(store_history_types) / sizeof(store_history_types[0]))

/* 在 ramfs_path / diskfs_path 下找有 fname 的日期，开区间 (lower, ceiling)。pick_max 取最大，否则取最小。没有则 0 */
static rt_uint32_t store_find_date(upload_store_obj_t obj, const char *fname,
                                   rt_uint32_t lower, rt_uint32_t ceiling, rt_bool_t pick_max)
{
    DIR *dir;
    struct dirent *dent;
    rt_uint32_t best = 0;
    const char *roots[2];
    rt_size_t r;

    roots[0] = obj->ramfs_path;
    roots[1] = obj->diskfs_path;
    for (r = 0; r < 2; r++)
    {
        if (roots[r] == RT_NULL || roots[r][0] == '\0')
            continue;
        dir = opendir(roots[r]);
        if (dir == RT_NULL)
            continue;

        while ((dent = readdir(dir)) != RT_NULL)
        {
            char path[RT_NAME_MAX];
            struct stat st;
            rt_uint32_t date = 0;
            rt_size_t i;

            if (rt_strlen(dent->d_name) != 8)
                continue;
            for (i = 0; i < 8; i++)
            {
                if (dent->d_name[i] < '0' || dent->d_name[i] > '9')
                    break;
                date = date * 10 + (rt_uint32_t)(dent->d_name[i] - '0');
            }
            if (i != 8 || date <= lower || date >= ceiling)
                continue;
            if (rt_snprintf(path, sizeof(path), "%s/%s/%s",
                            roots[r], dent->d_name, fname) >= (int)sizeof(path))
                continue;
            if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
                continue;

            if (pick_max)
            {
                if (date > best)
                    best = date;
            }
            else if (best == 0 || date < best)
            {
                best = date;
            }
        }
        closedir(dir);
    }

    return best;
}

/* 拷贝指定日期的一种类型：同日 diskfs（旧）再 ramfs（新），number >= cursor，正向读 */
static void store_copy_day(upload_store_obj_t obj, rt_uint32_t date, const char *fname,
                           rt_uint32_t cursor, rt_size_t *remain,
                           rt_uint8_t *buffer, rt_size_t size, rt_size_t *offset)
{
    const char *roots[2];
    rt_size_t r;
    char path[RT_NAME_MAX];
    rt_uint8_t ev[STORE_EVENT_SIZE];

    roots[0] = obj->diskfs_path;
    roots[1] = obj->ramfs_path;

    for (r = 0; r < 2 && *remain > 0; r++)
    {
        int fd;
        rt_uint32_t number;

        if (roots[r] == RT_NULL || roots[r][0] == '\0')
            continue;
        /* date 为十进制 YYYYMMDD，如 20260825 → .../20260825/fname */
        if (rt_snprintf(path, sizeof(path), "%s/%08u/%s",
                        roots[r], (unsigned)date, fname) >= (int)sizeof(path))
            continue;

        fd = open(path, O_RDONLY | O_BINARY, 0);
        if (fd < 0)
            continue;

        if (read(fd, ev, STORE_EVENT_SIZE) != (ssize_t)STORE_EVENT_SIZE)
        {
            close(fd);
            continue;
        }
        rt_memcpy(&number, ev, sizeof(number));

        /* number 连续递增：按 (cursor - 首条 number) 直接定位，不必逐条扫 */
        if (cursor > number)
        {
            if (lseek(fd, (off_t)(cursor - number) * (off_t)STORE_EVENT_SIZE, SEEK_SET) < 0)
            {
                close(fd);
                continue;
            }
        }
        else
        {
            if (*offset + STORE_EVENT_SIZE > size)
            {
                *remain = 0;
                close(fd);
                break;
            }
            rt_memcpy(buffer + *offset, ev, STORE_EVENT_SIZE);
            *offset += STORE_EVENT_SIZE;
            (*remain)--;
        }

        while (*remain > 0 && read(fd, ev, STORE_EVENT_SIZE) == (ssize_t)STORE_EVENT_SIZE)
        {
            if (*offset + STORE_EVENT_SIZE > size)
            {
                *remain = 0;
                break;
            }
            rt_memcpy(buffer + *offset, ev, STORE_EVENT_SIZE);
            *offset += STORE_EVENT_SIZE;
            (*remain)--;
        }
        close(fd);
    }
}

/*
 * event_type 低4位：FILE_TYPE_*，0x0F 按 RESET→CTRL 读全部。
 * date：十进制整数，如 20260825 只读该日；0xFFFFFFFF 最近一天；0 不分日期。
 * *_index：该类型从 number=index 起（含）从小到大读。
 * 凑不满 count 时末尾补一条全 0xFF。
 */
rt_size_t store_get_history(upload_store_obj_t obj, rt_uint8_t *buffer, rt_size_t size, event_history_t event_history)
{
    if (obj == RT_NULL || buffer == RT_NULL || size < STORE_EVENT_SIZE || event_history == RT_NULL)
        return 0;
    if (event_history->count == 0)
        return 0;

    enum event_file_type types[STORE_HISTORY_TYPE_CNT];
    rt_size_t ntype;
    rt_uint8_t want = (rt_uint8_t)(event_history->event_type & 0x0F);
    rt_size_t i;
    if (want == STORE_TYPE_ALL)
    {
        for (i = 0; i < STORE_HISTORY_TYPE_CNT; i++)
            types[i] = store_history_types[i];
        ntype = STORE_HISTORY_TYPE_CNT;
    }
    else
    {
        types[0] = FILE_TYPE_OTHER;
        for (i = 0; i < STORE_HISTORY_TYPE_CNT; i++)
        {
            if ((rt_uint8_t)store_history_types[i] == want)
            {
                types[0] = store_history_types[i];
                break;
            }
        }
        ntype = 1;
    }

    rt_size_t remain = event_history->count;
    if (obj->ramfs_fd_mutex == RT_NULL || obj->diskfs_fd_mutex == RT_NULL)
        return 0;
    if (rt_mutex_take(obj->ramfs_fd_mutex, RT_WAITING_FOREVER) != RT_EOK)
        return 0;
    if (rt_mutex_take(obj->diskfs_fd_mutex, RT_WAITING_FOREVER) != RT_EOK)
    {
        rt_mutex_release(obj->ramfs_fd_mutex);
        return 0;
    }

    rt_size_t offset = 0;
    for (i = 0; i < ntype && remain > 0; i++)
    {
        const char *fname = get_file_name(types[i]);
        rt_uint32_t cursor;
        rt_uint32_t date = event_history->date;
        rt_uint32_t day;

        switch (types[i] & 0x0F)
        {
        case FILE_TYPE_RESET:   cursor = event_history->reset_index;   break;
        case FILE_TYPE_FIRE:    cursor = event_history->fire_index;    break;
        case FILE_TYPE_MONITOR: cursor = event_history->monitor_index; break;
        case FILE_TYPE_FAULT:   cursor = event_history->fault_index;   break;
        case FILE_TYPE_ACTION:  cursor = event_history->action_index;  break;
        case FILE_TYPE_LINKAGE: cursor = event_history->linkage_index; break;
        case FILE_TYPE_MASK:    cursor = event_history->mask_index;    break;
        case FILE_TYPE_OTHER:   cursor = event_history->other_index;   break;
        case FILE_TYPE_CTRL:    cursor = event_history->ctrl_index;    break;
        default:                cursor = 0;                            break;
        }

        if (date == STORE_DATE_LATEST)
        {
            day = store_find_date(obj, fname, 0, STORE_DATE_CEIL, RT_TRUE);
            if (day != 0)
                store_copy_day(obj, day, fname, cursor, &remain, buffer, size, &offset);
        }
        else if (date != STORE_DATE_ALL)
        {
            store_copy_day(obj, date, fname, cursor, &remain, buffer, size, &offset);
        }
        else
        {
            day = 0;
            while (remain > 0)
            {
                day = store_find_date(obj, fname, day, STORE_DATE_CEIL, RT_FALSE);
                if (day == 0)
                    break;
                store_copy_day(obj, day, fname, cursor, &remain, buffer, size, &offset);
            }
        }
    }

    if (remain > 0 && offset + STORE_EVENT_SIZE <= size)
    {
        rt_memset(buffer + offset, 0xFF, STORE_EVENT_SIZE);
        offset += STORE_EVENT_SIZE;
    }

    rt_mutex_release(obj->diskfs_fd_mutex);
    rt_mutex_release(obj->ramfs_fd_mutex);
    return offset;
}
