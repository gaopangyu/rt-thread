#include "store.h"
#include "upload_store.h"
#include "upload_obj.h"
#include "msh_util.h"

#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#define DBG_TAG "store"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define STORE_OBJ_MAX   CUSTOM_PER_LAYER_OBJ_MAX
struct store_collect_ctx
{
    upload_obj_t parent;
    upload_store_obj_t objs[STORE_OBJ_MAX];
    int count;
};

static struct rt_semaphore store_run_sem;
static struct rt_semaphore store_run_now_sem;

/////////////////////////////////////////////prune diskfs/////////////////////////////////////////////
/*
 * 注意：当多个版本的事件时需要更大的disk，
 * 因为相同主机可能在/diskfs/ev1/ /diskfs/ev2/ /diskfs/ev3/... 下存储STORE_HOST_KEEP_BYTES字节的文件
*/
#define STORE_DISKFS_WATER_PCT      80U
#define STORE_HOST_KEEP_BYTES       (2U * 1024U * 1024U)
#define STORE_PRUNE_MAX_ROUNDS      64

static int store_name_is_dot(const char *name)
{
    return name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

/* ev 后跟纯数字，如 ev1 / ev12 */
static int store_is_ev_dir(const char *name)
{
    const char *p;

    if (name == RT_NULL || name[0] != 'e' || name[1] != 'v' || name[2] == '\0')
        return 0;
    p = name + 2;
    if (*p < '0' || *p > '9')
        return 0;
    for (; *p != '\0'; p++)
    {
        if (*p < '0' || *p > '9')
            return 0;
    }
    return 1;
}

static int store_ev_num(const char *name)
{
    int n = 0;
    const char *p = name + 2;

    while (*p >= '0' && *p <= '9')
    {
        n = n * 10 + (*p - '0');
        p++;
    }
    return n;
}

static int store_is_date_dir(const char *name)
{
    rt_size_t i;

    if (rt_strlen(name) != 8)
        return 0;
    for (i = 0; i < 8; i++)
    {
        if (name[i] < '0' || name[i] > '9')
            return 0;
    }
    return 1;
}

static rt_int32_t store_diskfs_used_pct(void)
{
    struct statfs st;

    if (dfs_statfs(DISKFS_ROOT, &st) != 0 || st.f_blocks == 0)
        return -1;
    if (st.f_bfree >= st.f_blocks)
        return 0;
    return (rt_int32_t)(((rt_uint64_t)(st.f_blocks - st.f_bfree) * 100U) / st.f_blocks);
}

/* 目录树下普通文件总大小；边 readdir 边 stat，不缓存文件名 */
static rt_uint32_t store_dir_size(const char *path)
{
    DIR *dir;
    struct dirent *dent;
    rt_uint32_t total = 0;

    dir = opendir(path);
    if (dir == RT_NULL)
        return 0;

    while ((dent = readdir(dir)) != RT_NULL)
    {
        char child[RT_NAME_MAX];
        struct stat st;

        if (store_name_is_dot(dent->d_name))
            continue;
        if (rt_snprintf(child, sizeof(child), "%s/%s", path, dent->d_name) >= (int)sizeof(child))
            continue;
        if (stat(child, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            total += store_dir_size(child);
        else
            total += (rt_uint32_t)st.st_size;
    }
    closedir(dir);
    return total;
}

/* 每次只处理一个子项再重新 opendir，避免边 readdir 边 unlink 跳过 */
static int store_rm_tree(const char *path)
{
    for (;;)
    {
        DIR *dir;
        struct dirent *dent;
        char name[RT_NAME_MAX];
        char child[RT_NAME_MAX];
        struct stat st;
        int found = 0;

        dir = opendir(path);
        if (dir == RT_NULL)
            return -1;

        while ((dent = readdir(dir)) != RT_NULL)
        {
            if (store_name_is_dot(dent->d_name))
                continue;
            rt_strncpy(name, dent->d_name, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            found = 1;
            break;
        }
        closedir(dir);

        if (!found)
            break;

        if (rt_snprintf(child, sizeof(child), "%s/%s", path, name) >= (int)sizeof(child))
            return -1;

        if (stat(child, &st) == 0 && S_ISDIR(st.st_mode))
        {
            if (store_rm_tree(child) != 0)
                return -1;
        }
        else if (unlink(child) != 0)
        {
            LOG_E("unlink %s failed", child);
            return -1;
        }
    }

    if (rmdir(path) != 0)
    {
        LOG_E("rmdir %s failed", path);
        return -1;
    }
    return 0;
}

static int store_find_oldest_date(const char *host_path, char *date_name, rt_size_t name_sz)
{
    DIR *dir;
    struct dirent *dent;
    char oldest[9];
    int found = 0;

    dir = opendir(host_path);
    if (dir == RT_NULL)
        return -1;

    oldest[0] = '\0';
    while ((dent = readdir(dir)) != RT_NULL)
    {
        char child[RT_NAME_MAX];
        struct stat st;

        if (store_name_is_dot(dent->d_name) || !store_is_date_dir(dent->d_name))
            continue;
        if (rt_snprintf(child, sizeof(child), "%s/%s", host_path, dent->d_name) >= (int)sizeof(child))
            continue;
        if (stat(child, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;
        if (!found || rt_strcmp(dent->d_name, oldest) < 0)
        {
            rt_memcpy(oldest, dent->d_name, 8);
            oldest[8] = '\0';
            found = 1;
        }
    }
    closedir(dir);

    if (!found || name_sz < 9)
        return -1;
    rt_memcpy(date_name, oldest, 9);
    return 0;
}

/* 先 ev 编号小的（ev1），同 ev 下取容量最大的主机目录，且必须 >= 2MB */
static int store_pick_largest_host(char *out_path, rt_size_t out_sz)
{
    DIR *ev_root;
    struct dirent *ev_dent;
    char best_path[RT_NAME_MAX];
    rt_uint32_t best_size = 0;
    int best_ev = 0;
    int found = 0;

    ev_root = opendir(DISKFS_ROOT);
    if (ev_root == RT_NULL)
        return -1;

    best_path[0] = '\0';
    while ((ev_dent = readdir(ev_root)) != RT_NULL)
    {
        DIR *host_dir;
        struct dirent *host_dent;
        char ev_path[RT_NAME_MAX];
        int ev_num;

        if (store_name_is_dot(ev_dent->d_name) || !store_is_ev_dir(ev_dent->d_name))
            continue;

        ev_num = store_ev_num(ev_dent->d_name);
        if (rt_snprintf(ev_path, sizeof(ev_path), "%s/%s", DISKFS_ROOT, ev_dent->d_name) >= (int)sizeof(ev_path))
            continue;

        host_dir = opendir(ev_path);
        if (host_dir == RT_NULL)
            continue;

        while ((host_dent = readdir(host_dir)) != RT_NULL)
        {
            char host_path[RT_NAME_MAX];
            struct stat st;
            rt_uint32_t sz;

            if (store_name_is_dot(host_dent->d_name))
                continue;
            if (rt_snprintf(host_path, sizeof(host_path), "%s/%s", ev_path, host_dent->d_name) >= (int)sizeof(host_path))
                continue;
            if (stat(host_path, &st) != 0 || !S_ISDIR(st.st_mode))
                continue;

            sz = store_dir_size(host_path);
            if (sz < STORE_HOST_KEEP_BYTES)
                continue;

            if (!found || ev_num < best_ev || (ev_num == best_ev && sz > best_size))
            {
                rt_strncpy(best_path, host_path, sizeof(best_path) - 1);
                best_path[sizeof(best_path) - 1] = '\0';
                best_size = sz;
                best_ev = ev_num;
                found = 1;
            }
        }
        closedir(host_dir);
    }
    closedir(ev_root);

    if (!found)
        return -1;
    if (rt_snprintf(out_path, out_sz, "%s", best_path) >= (int)out_sz)
        return -1;
    return 0;
}

static rt_mutex_t store_find_host_mutex(struct store_collect_ctx *ctx, const char *host_path)
{
    int i;

    if (ctx == RT_NULL || host_path == RT_NULL)
        return RT_NULL;

    for (i = 0; i < ctx->count; i++)
    {
        if (ctx->objs[i] == RT_NULL || ctx->objs[i]->diskfs_fd_mutex == RT_NULL)
            continue;
        if (rt_strcmp(ctx->objs[i]->diskfs_path, host_path) == 0)
            return ctx->objs[i]->diskfs_fd_mutex;
    }
    return RT_NULL;
}

static int store_shrink_host(const char *host_path)
{
    while (store_dir_size(host_path) >= STORE_HOST_KEEP_BYTES)
    {
        char date[9];
        char date_path[RT_NAME_MAX];

        if (store_find_oldest_date(host_path, date, sizeof(date)) != 0)
        {
            LOG_W("no date dir under %s but size still >= 3MB", host_path);
            return -1;
        }
        if (rt_snprintf(date_path, sizeof(date_path), "%s/%s", host_path, date) >= (int)sizeof(date_path))
            return -1;

        LOG_W("prune oldest %s", date_path);
        if (store_rm_tree(date_path) != 0)
            return -1;
    }
    return 0;
}

static void store_prune_diskfs(struct store_collect_ctx *ctx)
{
    int round;

    for (round = 0; round < STORE_PRUNE_MAX_ROUNDS; round++)
    {
        rt_int32_t used_pct = store_diskfs_used_pct();
        char host_path[RT_NAME_MAX];
        rt_mutex_t mtx;
        int ret;

        if (used_pct < 0)
        {
            LOG_E("statfs %s failed", DISKFS_ROOT);
            return;
        }
        if (used_pct <= (rt_int32_t)STORE_DISKFS_WATER_PCT)
            return;

        if (store_pick_largest_host(host_path, sizeof(host_path)) != 0)
        {
            LOG_W("diskfs used %d%%, all /diskfs/ev*/*/ < 2MB, stop prune", used_pct);
            return;
        }

        LOG_W("diskfs used %d%%, shrink %s", used_pct, host_path);

        mtx = store_find_host_mutex(ctx, host_path);
        if (mtx != RT_NULL && rt_mutex_take(mtx, RT_WAITING_FOREVER) != RT_EOK)
            mtx = RT_NULL;

        ret = store_shrink_host(host_path);

        if (mtx != RT_NULL)
            rt_mutex_release(mtx);

        if (ret != 0)
            return;
    }
}

/////////////////////////////////////////////store migrate/////////////////////////////////////////////
#define STORE_COPY_CHUNK            512

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

    store_prune_diskfs(&ctx);

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

void store_check_ramfs(void)
{
    struct statfs st;
    static rt_uint8_t ramfs_water; /* 0:<25%  1:25%~75%  2:>75% */
    rt_uint32_t used_pct = 0;

    if (dfs_statfs(RAMFS_ROOT, &st) != 0 || st.f_blocks == 0)
        return;

    if (st.f_bfree < st.f_blocks)
        used_pct = (rt_uint32_t)(((rt_uint64_t)(st.f_blocks - st.f_bfree) * 100U) / st.f_blocks);

    if (used_pct > 75U)
    {
        if (ramfs_water < 2U)
        {
            ramfs_water = 2U;
            LOG_W("ramfs used %u%%, store_run_now", used_pct);
            store_run_now();
        }
    }
    else if (used_pct > 25U)
    {
        if (ramfs_water < 1U)
        {
            ramfs_water = 1U;
            LOG_I("ramfs used %u%%, store_run", used_pct);
            store_run();
        }
    }
    else
    {
        ramfs_water = 0U;
    }
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
#define STORE_INDEX_LAST            0xFFFFFFFFu
#define STORE_TYPE_ALL              0x0Fu
#define STORE_DATE_CEIL             100000000u

static const enum event_file_type store_history_types[] =
{
    FILE_TYPE_RESET, FILE_TYPE_FIRE, FILE_TYPE_MONITOR, FILE_TYPE_FAULT,
    FILE_TYPE_ACTION, FILE_TYPE_LINKAGE, FILE_TYPE_MASK, FILE_TYPE_OTHER,
    FILE_TYPE_CTRL,
};
#define STORE_HISTORY_TYPE_CNT     (sizeof(store_history_types) / sizeof(store_history_types[0]))

static void store_date_pick(rt_uint32_t date, rt_uint32_t lower, rt_uint32_t ceiling,
                            rt_bool_t pick_max, rt_uint32_t *best)
{
    if (date == 0 || date <= lower || date >= ceiling || best == RT_NULL)
        return;

    if (pick_max)
    {
        if (date > *best)
            *best = date;
    }
    else if (*best == 0 || date < *best)
    {
        *best = date;
    }
}

/* ramfs 是扁平文件：dirent 名为 ev1/tx3016a/YYYYMMDD/fname，只能 opendir("/ramfs") */
static void store_find_date_ramfs(const char *ramfs_path, const char *fname,
                                  rt_uint32_t lower, rt_uint32_t ceiling, rt_bool_t pick_max,
                                  rt_uint32_t *best)
{
    DIR *dir;
    struct dirent *dent;
    const char *prefix;
    rt_size_t prefix_len;

    if (ramfs_path == RT_NULL || ramfs_path[0] == '\0' || fname == RT_NULL || best == RT_NULL)
        return;
    if (rt_strncmp(ramfs_path, RAMFS_ROOT, sizeof(RAMFS_ROOT) - 1) != 0)
        return;

    prefix = ramfs_path + (sizeof(RAMFS_ROOT) - 1);
    if (*prefix == '/')
        prefix++;
    prefix_len = rt_strlen(prefix);
    if (prefix_len == 0)
        return;

    dir = opendir(RAMFS_ROOT);
    if (dir == RT_NULL)
        return;

    while ((dent = readdir(dir)) != RT_NULL)
    {
        const char *name = dent->d_name;
        const char *p;
        rt_uint32_t date = 0;
        rt_size_t i;

        if (rt_strncmp(name, prefix, prefix_len) != 0 || name[prefix_len] != '/')
            continue;

        p = name + prefix_len + 1;
        for (i = 0; i < 8; i++)
        {
            if (p[i] < '0' || p[i] > '9')
                break;
            date = date * 10 + (rt_uint32_t)(p[i] - '0');
        }
        if (i != 8 || p[8] != '/' || rt_strcmp(p + 9, fname) != 0)
            continue;

        store_date_pick(date, lower, ceiling, pick_max, best);
    }
    closedir(dir);
}

/* diskfs 是真实目录：/diskfs/ev1/tx3016a/YYYYMMDD/fname */
static void store_find_date_diskfs(const char *diskfs_path, const char *fname,
                                   rt_uint32_t lower, rt_uint32_t ceiling, rt_bool_t pick_max,
                                   rt_uint32_t *best)
{
    DIR *dir;
    struct dirent *dent;

    if (diskfs_path == RT_NULL || diskfs_path[0] == '\0' || fname == RT_NULL || best == RT_NULL)
        return;

    dir = opendir(diskfs_path);
    if (dir == RT_NULL)
        return;

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
        if (i != 8)
            continue;
        if (rt_snprintf(path, sizeof(path), "%s/%s/%s",
                        diskfs_path, dent->d_name, fname) >= (int)sizeof(path))
            continue;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        store_date_pick(date, lower, ceiling, pick_max, best);
    }
    closedir(dir);
}

/* 在 ramfs_path / diskfs_path 下找有 fname 的日期，开区间 (lower, ceiling)。pick_max 取最大，否则取最小。没有则 0 */
static rt_uint32_t store_find_date(const char *ramfs_path, const char *diskfs_path, const char *fname,
                                   rt_uint32_t lower, rt_uint32_t ceiling, rt_bool_t pick_max)
{
    rt_uint32_t best = 0;

    store_find_date_ramfs(ramfs_path, fname, lower, ceiling, pick_max, &best);
    store_find_date_diskfs(diskfs_path, fname, lower, ceiling, pick_max, &best);
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
    rt_size_t copied;

    roots[0] = obj->diskfs_path;
    roots[1] = obj->ramfs_path;
    if (cursor == STORE_INDEX_LAST)
    {
        /* 最后一条在 ramfs（新）里，没有再落到 diskfs */
        roots[0] = obj->ramfs_path;
        roots[1] = obj->diskfs_path;
    }

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

        copied = *offset;

        if (cursor == STORE_INDEX_LAST)
        {
            off_t end = lseek(fd, 0, SEEK_END);
            off_t nrec;

            if (end < (off_t)STORE_EVENT_SIZE)
            {
                close(fd);
                continue;
            }
            nrec = end / (off_t)STORE_EVENT_SIZE;
            if (lseek(fd, (nrec - 1) * (off_t)STORE_EVENT_SIZE, SEEK_SET) < 0)
            {
                close(fd);
                continue;
            }
        }
        else
        {
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

        /* last 只取 ramfs（或 ramfs 没有时的 diskfs）最新一条，不要两个盘各读一条 */
        if (cursor == STORE_INDEX_LAST && *offset > copied)
            break;
    }
}

/*
 * event_type 低4位：FILE_TYPE_*，0x0F 按 RESET→CTRL 读全部。
 * date：十进制整数，如 20260825 只读该日；0xFFFFFFFF 最近一天；0 不分日期。
 * *_index：该类型从 number=index 起（含）从小到大读；0xFFFFFFFF 读最后一条。
 * 凑不满 count 时末尾补一条全 0xFF，*is_end 为 RT_TRUE。
 */
rt_size_t store_get_history(upload_store_obj_t obj, rt_uint8_t *buffer, rt_size_t size, event_history_t event_history, rt_bool_t *is_end)
{
    if (is_end != RT_NULL)
        *is_end = RT_FALSE;

    if (buffer == RT_NULL || size < STORE_EVENT_SIZE)
        return 0;
    if (obj == RT_NULL || event_history == RT_NULL || event_history->count == 0)
        return event_put_end(buffer, size, is_end);

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
        return event_put_end(buffer, size, is_end);
    if (rt_mutex_take(obj->ramfs_fd_mutex, RT_WAITING_FOREVER) != RT_EOK)
        return event_put_end(buffer, size, is_end);
    if (rt_mutex_take(obj->diskfs_fd_mutex, RT_WAITING_FOREVER) != RT_EOK)
    {
        rt_mutex_release(obj->ramfs_fd_mutex);
        return event_put_end(buffer, size, is_end);
    }

    rt_size_t offset = 0;
    for (i = 0; i < ntype && remain > 0; i++)
    {
        const char *fname = event_get_file_name(types[i]);
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
            day = store_find_date(obj->ramfs_path, obj->diskfs_path, fname, 0, STORE_DATE_CEIL, RT_TRUE);
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
                day = store_find_date(obj->ramfs_path, obj->diskfs_path, fname, day, STORE_DATE_CEIL, RT_FALSE);
                if (day == 0)
                    break;
                store_copy_day(obj, day, fname, cursor, &remain, buffer, size, &offset);
            }
        }
    }

    if (remain > 0)
        offset += event_put_end(buffer + offset, size - offset, is_end);
    else if (is_end != RT_NULL)
        *is_end = RT_FALSE;

    rt_mutex_release(obj->diskfs_fd_mutex);
    rt_mutex_release(obj->ramfs_fd_mutex);
    return offset;
}

static int store_read_last_number(const char *ramfs_path, const char *diskfs_path, rt_uint32_t date,
                                  const char *fname, rt_uint32_t *number)
{
    const char *roots[2];
    rt_size_t r;

    if (fname == RT_NULL || number == RT_NULL)
        return -1;

    /* ramfs 更新，没有再落到 diskfs */
    roots[0] = ramfs_path;
    roots[1] = diskfs_path;
    for (r = 0; r < 2; r++)
    {
        char path[RT_NAME_MAX];
        int fd;
        off_t end;
        off_t nrec;
        struct event_num_pack ev;

        if (roots[r] == RT_NULL || roots[r][0] == '\0')
            continue;
        if (rt_snprintf(path, sizeof(path), "%s/%08u/%s",
                        roots[r], (unsigned)date, fname) >= (int)sizeof(path))
            continue;

        fd = open(path, O_RDONLY | O_BINARY, 0);
        if (fd < 0)
            continue;

        end = lseek(fd, 0, SEEK_END);
        if (end < (off_t)STORE_EVENT_SIZE)
        {
            close(fd);
            continue;
        }
        nrec = end / (off_t)STORE_EVENT_SIZE;
        if (lseek(fd, (nrec - 1) * (off_t)STORE_EVENT_SIZE, SEEK_SET) < 0)
        {
            close(fd);
            continue;
        }
        if (read(fd, &ev, STORE_EVENT_SIZE) != (ssize_t)STORE_EVENT_SIZE)
        {
            close(fd);
            continue;
        }
        close(fd);
        *number = ev.number;
        return 0;
    }
    return -1;
}

rt_err_t store_get_event_last_num(const char *dir, const char *path, event_last_num_t out)
{
    char ramfs_path[RT_NAME_MAX];
    char diskfs_path[RT_NAME_MAX];
    rt_size_t i;

    if (dir == RT_NULL || path == RT_NULL || out == RT_NULL)
        return -RT_EINVAL;
    if (dir[0] == '\0' || path[0] == '\0')
        return -RT_EINVAL;

    if (rt_snprintf(ramfs_path, sizeof(ramfs_path), "%s/%s/%s",
                    RAMFS_ROOT, dir, path) >= (int)sizeof(ramfs_path))
        return -RT_ERROR;
    if (rt_snprintf(diskfs_path, sizeof(diskfs_path), "%s/%s/%s",
                    DISKFS_ROOT, dir, path) >= (int)sizeof(diskfs_path))
        return -RT_ERROR;

    for (i = 0; i < STORE_HISTORY_TYPE_CNT; i++)
    {
        const char *fname = event_get_file_name(store_history_types[i]);
        rt_uint32_t date;
        rt_uint32_t number;

        date = store_find_date(ramfs_path, diskfs_path, fname, 0, STORE_DATE_CEIL, RT_TRUE);
        if (date == 0)
            continue;
        if (store_read_last_number(ramfs_path, diskfs_path, date, fname, &number) != 0)
            continue;

        funnel_set_event_last_num(out, store_history_types[i], number + 1);
    }

    LOG_I("next event num: reset=%u fire=%u monitor=%u fault=%u action=%u linkage=%u mask=%u other=%u ctrl=%u",
          out->reset, out->fire, out->monitor, out->fault, out->action,
          out->linkage, out->mask, out->other, out->ctrl);

    return RT_EOK;
}

///////////////////////////////////////////// msh cmd /////////////////////////////////////////////
#define STORE_MSH_HISTORY_MAX   16

static int store_msh_parse_type(const char *s, rt_uint8_t *out)
{
    static const struct
    {
        const char *name;
        rt_uint8_t type;
    } map[] =
    {
        { "all",     STORE_TYPE_ALL },
        { "reset",   FILE_TYPE_RESET },
        { "fire",    FILE_TYPE_FIRE },
        { "monitor", FILE_TYPE_MONITOR },
        { "fault",   FILE_TYPE_FAULT },
        { "action",  FILE_TYPE_ACTION },
        { "linkage", FILE_TYPE_LINKAGE },
        { "mask",    FILE_TYPE_MASK },
        { "other",   FILE_TYPE_OTHER },
        { "ctrl",    FILE_TYPE_CTRL },
    };
    rt_uint32_t v;
    rt_size_t i;

    if (s == RT_NULL || s[0] == '\0')
    {
        *out = STORE_TYPE_ALL;
        return 0;
    }

    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
    {
        if (rt_strcmp(s, map[i].name) == 0)
        {
            *out = map[i].type;
            return 0;
        }
    }

    if (msh_parse_u32(s, &v) != 0 || v > 0x0Fu)
        return -1;
    *out = (rt_uint8_t)v;
    return 0;
}

static upload_store_obj_t store_msh_find_obj(const char *name)
{
    struct store_collect_ctx ctx;
    upload_obj_t obj;

    if (name != RT_NULL && name[0] != '\0')
    {
        obj = (upload_obj_t)rt_object_find(name, RT_Object_Class_Custom);
        if (obj == RT_NULL)
            return RT_NULL;
        if (((custom_obj_t)obj)->type != Custom_Object_Class_Upload)
            return RT_NULL;
        return (upload_store_obj_t)obj;
    }

    ctx.parent = (upload_obj_t)rt_object_find(UPLOAD_STORE_PARENT_NAME, RT_Object_Class_Custom);
    ctx.count = 0;
    if (ctx.parent == RT_NULL || rt_object_get_information(RT_Object_Class_Custom) == RT_NULL)
        return RT_NULL;

    rt_object_for_each(RT_Object_Class_Custom, store_upload_store_collect, &ctx);
    if (ctx.count == 0)
        return RT_NULL;
    return ctx.objs[0];
}

static void store_msh_print_history_header(void)
{
    rt_kprintf("%-4s%-14s%-11s%-14s%-12s%-12s%-14s%s\n",
               "", "number", "version", "timestamp", "sys_type", "sys_addr",
               "event_type", "event_description");
}

static void store_msh_print_event(rt_size_t idx, const rt_uint8_t *raw)
{
    event_num_pack_t pack = (event_num_pack_t)raw;
    char idx_s[8];
    char num_s[16];
    char ver_s[8];
    char ts_s[16];
    char sys_type_s[8];
    char sys_addr_s[8];
    char event_type_s[8];
    char desc_s[40];
    rt_size_t i;

    rt_snprintf(idx_s, sizeof(idx_s), "[%d]", (int)idx);
    rt_snprintf(num_s, sizeof(num_s), "%010u", (unsigned)pack->number);
    rt_snprintf(ver_s, sizeof(ver_s), "0x%02X", pack->event.version);
    rt_snprintf(ts_s, sizeof(ts_s), "%02X%02X%02X%02X%02X%02X",
                pack->event.timestamp[0], pack->event.timestamp[1],
                pack->event.timestamp[2], pack->event.timestamp[3],
                pack->event.timestamp[4], pack->event.timestamp[5]);
    rt_snprintf(sys_type_s, sizeof(sys_type_s), "0x%02X", pack->event.sys_type);
    rt_snprintf(sys_addr_s, sizeof(sys_addr_s), "0x%02X", pack->event.sys_addr);
    rt_snprintf(event_type_s, sizeof(event_type_s), "0x%02X", pack->event.event_type);
    for (i = 0; i < sizeof(pack->event.event_description); i++)
        rt_snprintf(desc_s + i * 2, 3, "%02X", pack->event.event_description[i]);

    rt_kprintf("%-4s%-14s%-11s%-14s%-12s%-12s%-14s0x%s\n",
               idx_s, num_s, ver_s, ts_s, sys_type_s, sys_addr_s, event_type_s, desc_s);
}

static int cmd_store_get_history(int argc, char **argv)
{
    const char *name;
    const char *type_s;
    const char *count_s;
    const char *date_s;
    const char *index_s;
    upload_store_obj_t obj;
    struct event_history hist;
    rt_uint8_t buf[STORE_MSH_HISTORY_MAX * STORE_EVENT_SIZE];
    rt_uint8_t type = STORE_TYPE_ALL;
    rt_uint32_t count = 8;
    rt_uint32_t date = STORE_DATE_LATEST;
    rt_uint32_t index = STORE_INDEX_LAST;
    rt_size_t n;
    rt_size_t i;

    if (msh_has_opt(argc, argv, "h"))
    {
        rt_kprintf("Usage: store_get_history [-n <obj>] [-t <type>] [-c <count>] [-d <date>] [-i <index>]\n");
        rt_kprintf("  -n  upload_store name (omit: first upload_store)\n");
        rt_kprintf("  -t  all|reset|fire|monitor|fault|action|linkage|mask|other|ctrl|0x0F (default all)\n");
        rt_kprintf("  -c  max records 1..%d (default 8)\n", STORE_MSH_HISTORY_MAX);
        rt_kprintf("  -d  YYYYMMDD | latest | all (default latest)\n");
        rt_kprintf("  -i  start number | last (default last)\n");
        rt_kprintf("Example: store_get_history -t fire -c 4 -d latest -i last\n");
        return RT_EOK;
    }

    name = msh_get_opt(argc, argv, "n");
    type_s = msh_get_opt(argc, argv, "t");
    count_s = msh_get_opt(argc, argv, "c");
    date_s = msh_get_opt(argc, argv, "d");
    index_s = msh_get_opt(argc, argv, "i");

    if (store_msh_parse_type(type_s, &type) != 0)
    {
        rt_kprintf("Invalid type: %s\n", type_s);
        return -RT_ERROR;
    }
    if (count_s != RT_NULL && (msh_parse_u32(count_s, &count) != 0 || count == 0))
    {
        rt_kprintf("Invalid count: %s\n", count_s);
        return -RT_ERROR;
    }
    if (count > STORE_MSH_HISTORY_MAX)
        count = STORE_MSH_HISTORY_MAX;

    if (date_s != RT_NULL)
    {
        if (rt_strcmp(date_s, "latest") == 0)
            date = STORE_DATE_LATEST;
        else if (rt_strcmp(date_s, "all") == 0)
            date = STORE_DATE_ALL;
        else if (msh_parse_u32(date_s, &date) != 0)
        {
            rt_kprintf("Invalid date: %s\n", date_s);
            return -RT_ERROR;
        }
    }

    if (index_s != RT_NULL)
    {
        if (rt_strcmp(index_s, "last") == 0)
            index = STORE_INDEX_LAST;
        else if (msh_parse_u32(index_s, &index) != 0)
        {
            rt_kprintf("Invalid index: %s\n", index_s);
            return -RT_ERROR;
        }
    }

    obj = store_msh_find_obj(name);
    if (obj == RT_NULL)
    {
        rt_kprintf("upload_store not found%s%s\n", name ? ": " : "", name ? name : "");
        return -RT_ERROR;
    }

    rt_memset(&hist, 0, sizeof(hist));
    hist.event_type = type;
    hist.count = (rt_uint16_t)count;
    hist.date = date;
    hist.reset_index = index;
    hist.fire_index = index;
    hist.monitor_index = index;
    hist.fault_index = index;
    hist.action_index = index;
    hist.linkage_index = index;
    hist.mask_index = index;
    hist.other_index = index;
    hist.ctrl_index = index;

    n = store_get_history(obj, buf, sizeof(buf), &hist, RT_NULL);

    store_msh_print_history_header();
    for (i = 0; i + STORE_EVENT_SIZE <= n; i += STORE_EVENT_SIZE)
        store_msh_print_event(i / STORE_EVENT_SIZE, buf + i);

    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_store_get_history, store_get_history, Read stored events: store_get_history -h);
