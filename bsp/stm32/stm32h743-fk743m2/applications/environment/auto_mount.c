#include <fal.h>
#include <dfs_fs.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#define DBG_TAG "auto_mount"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define INIT_PARTITION_NAME "init"
#define INIT_MOUNT_POINT "/"
#define INIT_DIR_PATH       "/init"
#define INIT_PART_LIST_PATH "/init/part_table.txt"
#define INIT_TXT_LINE_MAX   256

#define DISKFS_PARTITION_NAME "diskfs"

/* part_table.txt 行末 reserved：0=littlefs，1=FAT32（DFS 类型名 elm），其它值暂不支持 */
#define PART_FS_RESERVED_LITTLEFS 0u
#define PART_FS_RESERVED_FAT32    1u

static int ensure_dir(const char *path)
{
    struct stat st;

    if (path == RT_NULL || path[0] == '\0')
        return -1;

    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode) ? 0 : -1;

    return mkdir(path, 0x777);
}

static void part_str_trim(char *s)
{
    char *p = s;

    while (*p == ' ' || *p == '\t')
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);

    p = s + strlen(s);
    while (p > s && (p[-1] == ' ' || p[-1] == '\t'))
        *--p = '\0';
}

/**
 * 从 fd 读一行到 line，单行最多 cap 字节（含结尾 '\\0'，即最多 cap - 1 个字符）。
 * @return  1 读到一行（可为空行）；0 文件结束且无残留字符；-1 读错误
 */
static int read_one_line(int fd, char *line, size_t cap)
{
    size_t i = 0;
    char ch;
    ssize_t nr;

    if (cap < 2)
        return -1;

    for (;;)
    {
        nr = read(fd, &ch, 1);
        if (nr < 0)
            return -1;
        if (nr == 0)
        {
            line[i] = '\0';
            return (i > 0) ? 1 : 0;
        }
        if (ch == '\n')
        {
            line[i] = '\0';
            return 1;
        }
        if (ch == '\r')
            continue;
        if (i < cap - 1)
        {
            line[i++] = ch;
        }
        else
        {
            while ((nr = read(fd, &ch, 1)) > 0 && ch != '\n')
                ;
            line[cap - 1] = '\0';
            LOG_W("line longer than %u bytes, truncated", (unsigned)(cap - 1));
            return 1;
        }
    }
}

/**
 * 从 /init/init.txt 读取分区描述，写入 part_table。
 * 每行：magic_word, name, flash_name, offset, len, reserved（逗号分隔，# 开头为注释）。
 * reserved：0=littlefs，1=FAT32(elm)，其它暂不支持。
 *
 * @return 分区总数（不含 part_table[0]），无有效行或读失败时返回 0，由调用方使用默认表。
 */
static rt_size_t load_partitions_from_init_txt(struct fal_partition *part_table, rt_size_t table_max)
{
    int fd;
    char line[INIT_TXT_LINE_MAX];
    rt_size_t idx = 0;

    if (table_max < 1)
        return 0;

    fd = open(INIT_PART_LIST_PATH, O_RDONLY);
    if (fd < 0)
    {
        LOG_I("%s not found", INIT_PART_LIST_PATH);
        ensure_dir(INIT_DIR_PATH);
        return 0;
    }

    while (idx < table_max)
    {
        int rl = read_one_line(fd, line, sizeof(line));

        if (rl < 0)
        {
            LOG_W("read %s error", INIT_PART_LIST_PATH);
            close(fd);
            return 0;
        }
        if (rl == 0)
            break;

        part_str_trim(line);
        if (line[0] == '\0' || line[0] == '#')
            continue;

        {
            int magic_i;
            unsigned long len_ul;
            unsigned int res;
            char name[FAL_DEV_NAME_MAX];
            char flash[FAL_DEV_NAME_MAX];
            long off;
            int n;
            struct fal_partition *part = &part_table[idx];

            n = sscanf(line, "%i,%23[^,],%23[^,],%ld,%lu,%u",
                       &magic_i, name, flash, &off, &len_ul, &res);
            if (n != 6)
            {
                LOG_W("bad line (need 6 fields): %s", line);
                continue;
            }
            if ((rt_uint32_t)magic_i != FAL_PART_MAGIC_WORD)
            {
                LOG_W("bad magic 0x%x: %s", (unsigned int)magic_i, line);
                continue;
            }

            part_str_trim(name);
            part_str_trim(flash);
            if (name[0] == '\0' || flash[0] == '\0')
            {
                LOG_W("empty name or flash_name: %s", line);
                continue;
            }

            part->magic_word = (rt_uint32_t)magic_i;
            strncpy(part->name, name, FAL_DEV_NAME_MAX - 1);
            part->name[FAL_DEV_NAME_MAX - 1] = '\0';
            strncpy(part->flash_name, flash, FAL_DEV_NAME_MAX - 1);
            part->flash_name[FAL_DEV_NAME_MAX - 1] = '\0';
            part->offset = off;
            part->len = (rt_size_t)len_ul;
            part->reserved = res;
            idx++;
        }
    }

    close(fd);

    return idx;
}

/**
 * 把 init 之后直到 Flash 末尾的空间做成 diskfs 分区（littlefs）。
 * @return 写入的分区数（0 或 1）
 */
static rt_size_t add_default_diskfs_partition(struct fal_partition *part,
                                              const struct fal_partition *init_part)
{
    const struct fal_flash_dev *flash;
    rt_uint32_t disk_off;
    rt_size_t disk_len;

    if (part == RT_NULL || init_part == RT_NULL)
        return 0;

    flash = fal_flash_device_find(init_part->flash_name);
    if (flash == RT_NULL)
    {
        LOG_E("flash '%s' not found, cannot create diskfs", init_part->flash_name);
        return 0;
    }

    if (init_part->offset < 0)
        return 0;

    disk_off = (rt_uint32_t)init_part->offset + init_part->len;
    if (disk_off >= flash->len)
    {
        LOG_E("no leftover flash after init (off=0x%x flash_len=0x%x)",
              (unsigned)disk_off, (unsigned)flash->len);
        return 0;
    }

    disk_len = flash->len - disk_off;
    if (flash->blk_size && (disk_len % flash->blk_size) != 0)
        disk_len -= disk_len % flash->blk_size;
    if (disk_len == 0)
        return 0;

    if (init_part->offset != 0)
        LOG_W("init offset=0x%lx, space before init is unused", (long)init_part->offset);

    rt_memset(part, 0, sizeof(*part));
    part->magic_word = FAL_PART_MAGIC_WORD;
    strncpy(part->name, DISKFS_PARTITION_NAME, FAL_DEV_NAME_MAX - 1);
    strncpy(part->flash_name, init_part->flash_name, FAL_DEV_NAME_MAX - 1);
    part->offset = (long)disk_off;
    part->len = disk_len;
    part->reserved = PART_FS_RESERVED_LITTLEFS;

    LOG_I("default diskfs: flash='%s' offset=0x%x len=0x%x",
          part->flash_name, (unsigned)disk_off, (unsigned)disk_len);

    return 1;
}

static const char *part_reserved_to_fs_type(rt_uint32_t reserved)
{
    switch (reserved)
    {
    case PART_FS_RESERVED_LITTLEFS:
        return "lfs";
    case PART_FS_RESERVED_FAT32:
        return RT_NULL;
    default:
        return RT_NULL;
    }
}

/**
 * 挂载 part_table[1 .. part_count-1]（跳过 init）。
 * 挂载点：/{part->name}；reserved 决定 FS（见 PART_FS_RESERVED_*）。
 * 挂载失败则对该分区 dfs_mkfs 同类型后重试挂载。
 */
static void mount_extra_partitions(struct fal_partition *part_table, rt_size_t part_count)
{
    rt_size_t i;
    char mnt_path[FAL_DEV_NAME_MAX + 4];

    for (i = 1; i < part_count; i++)
    {
        struct fal_partition *part = &part_table[i];
        const char *fs_type = part_reserved_to_fs_type(part->reserved);

        if (fs_type == RT_NULL)
        {
            LOG_W("partition '%s': unsupported reserved=%u", part->name, part->reserved);
            continue;
        }

        if (part->name[0] == '\0')
            continue;

        rt_snprintf(mnt_path, sizeof(mnt_path), "/%s", part->name);

        if (ensure_dir(mnt_path) != 0)
            continue;

        if (fal_mtd_nor_device_create(part->name) == RT_NULL)
        {
            LOG_E("Can't create mtd on partition '%s'.", part->name);
            continue;
        }

        if (dfs_mount(part->name, mnt_path, fs_type, 0, 0) != 0)
        {
            int mkfs_ret;

            LOG_W("mount '%s' -> '%s' (%s) failed, try mkfs.", part->name, mnt_path, fs_type);
            mkfs_ret = dfs_mkfs(fs_type, part->name);
            if (mkfs_ret != 0)
                LOG_E("mkfs (%s) on '%s' failed (%d), skip remount.", fs_type, part->name, mkfs_ret);
            else if (dfs_mount(part->name, mnt_path, fs_type, 0, 0) != 0)
                LOG_E("mount '%s' -> '%s' (%s) still failed after mkfs (flash dirty or geometry mismatch?).",
                      part->name, mnt_path, fs_type);
            else
                LOG_I("mounted '%s' on '%s' (%s) after mkfs.", part->name, mnt_path, fs_type);
        }
        else
            LOG_I("mounted '%s' on '%s' (%s).", part->name, mnt_path, fs_type);
    }
}

static int mount_init_fs(void)
{
    struct rt_device *mtd_dev = fal_mtd_nor_device_create(INIT_PARTITION_NAME);

    if (mtd_dev == RT_NULL)
    {
        LOG_E("Can't create a mtd device on '%s' partition.", INIT_PARTITION_NAME);
        return -RT_ERROR;
    }

    if (dfs_mount(INIT_PARTITION_NAME, INIT_MOUNT_POINT, "lfs", 0, 0) == 0)
    {
        LOG_I("init_fs mounted on %s (%s).", INIT_MOUNT_POINT, INIT_PARTITION_NAME);
        return RT_EOK;
    }

    dfs_mkfs("lfs", INIT_PARTITION_NAME);
    if (dfs_mount(INIT_PARTITION_NAME, INIT_MOUNT_POINT, "lfs", 0, 0) == 0)
    {
        LOG_W("init_fs mkfs then mounted on %s (%s).", INIT_MOUNT_POINT, INIT_PARTITION_NAME);
        return RT_EOK;
    }

    LOG_E("Failed to mount init_fs on '%s'.", INIT_PARTITION_NAME);

    return -RT_ERROR;
}

static int auto_mount(void)
{
    if(rt_device_find(FAL_USING_NOR_FLASH_DEV_NAME) == NULL)
    {
        LOG_E("Flash device %s not found!", FAL_USING_NOR_FLASH_DEV_NAME);
        return -RT_ERROR;
    }

    if(fal_init() <= 0)
    {
        LOG_E("FAL initialization failed.");
        return -RT_ERROR;
    }

    if (mount_init_fs() != RT_EOK)
    {
        return -RT_ERROR;
    }

    static struct fal_partition part_table[10] = FAL_PART_TABLE;
    rt_size_t part_count = 1;
    rt_size_t extra;

    extra = load_partitions_from_init_txt(part_table + 1, sizeof(part_table) / sizeof(part_table[0]) - 1);
    if (extra == 0)
    {
        struct stat st;

        if (stat(INIT_PART_LIST_PATH, &st) != 0)
            extra = add_default_diskfs_partition(&part_table[1], &part_table[0]);
    }
    part_count += extra;

    /* 卸载并销毁 MTD，再切换临时分区表，避免 MTD 内缓存的 fal_partition 指针与全局表不一致 */
    if (dfs_unmount(INIT_MOUNT_POINT) != 0)
    {
        LOG_E("dfs_unmount(%s) failed before temp partition table.", INIT_MOUNT_POINT);
        return -RT_ERROR;
    }

    if (fal_mtd_nor_device_destroy(INIT_PARTITION_NAME) != RT_EOK)
    {
        LOG_E("fal_mtd_nor_device_destroy(%s) failed.", INIT_PARTITION_NAME);
        return -RT_ERROR;
    }

    fal_set_partition_table_temp(part_table, part_count);

    if (mount_init_fs() != RT_EOK)
    {
        LOG_E("Failed to mount init_fs on '%s' after temp partition table.", INIT_PARTITION_NAME);
        return -RT_ERROR;
    }

    fal_show_part_table();

    mount_extra_partitions(part_table, part_count);

    return RT_EOK;
}

INIT_ENV_EXPORT(auto_mount);