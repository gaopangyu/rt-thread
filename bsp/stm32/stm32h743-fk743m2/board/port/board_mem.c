/*
 * Copyright (c) 2006-2025 RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Board memory bring-up for STM32H743IIT6:
 *  - Enable D2/D3 SRAM clocks
 *  - Relocate kernel core to ITCM / DTCM (GCC)
 *  - Mount dfs_ramfs on D2 SRAM1+2 (256KB)
 */
#include <rtthread.h>
#include <string.h>
#include "stm32h7xx.h"
#include "board.h"

#define DBG_TAG "board.mem"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* GCC linker symbols */
#if defined(__GNUC__) && !defined(__ARMCC_VERSION)
extern rt_uint32_t _sitcm_load, _sitcm, _eitcm;
extern rt_uint32_t _sdtcm_data_load, _sdtcm_data, _edtcm_data;
extern rt_uint32_t _sdtcm_bss, _edtcm_bss;
extern rt_uint32_t _sramfs_pool, _eramfs_pool;
#endif

/**
 * 使能 D2/D3 SRAM 时钟，并从 Flash 拷贝 ITCM/DTCM 映像。
 * 必须在跳入 ITCM 代码之前调用（当前由 startup Reset_Handler 调用）。
 */
void board_memory_early_init(void)
{
    volatile rt_uint32_t tmpreg;

    /*
     * ITCM 链接在 0x00000000。复位后 VTOR=0，向量表依赖 boot remap。
     * 往 ITCM 拷贝代码前必须把 VTOR 指到 Flash，否则异常向量错乱。
     */
    SCB->VTOR = FLASH_BANK1_BASE;

    /* D2: SRAM1 / SRAM2 / SRAM3（ramfs + AUX） */
    SET_BIT(RCC->AHB2ENR, RCC_AHB2ENR_D2SRAM1EN | RCC_AHB2ENR_D2SRAM2EN | RCC_AHB2ENR_D2SRAM3EN);
    tmpreg = RCC->AHB2ENR;
    (void)tmpreg;

    /* D3 SRAM4 无独立时钟位，随 D3 域上电可用；BKPSRAM 另需 BKPRAMEN（当前未用） */

#if defined(__GNUC__) && !defined(__ARMCC_VERSION)
    {
        rt_uint32_t *src;
        rt_uint32_t *dst;
        rt_uint32_t *end;

        /* ITCM text */
        src = &_sitcm_load;
        dst = &_sitcm;
        end = &_eitcm;
        while (dst < end)
            *dst++ = *src++;

#if defined(__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
        SCB_InvalidateICache();
#endif

        /* DTCM data */
        src = &_sdtcm_data_load;
        dst = &_sdtcm_data;
        end = &_edtcm_data;
        while (dst < end)
            *dst++ = *src++;

        /* DTCM bss */
        dst = &_sdtcm_bss;
        end = &_edtcm_bss;
        while (dst < end)
            *dst++ = 0;
    }
#endif
}

#ifdef RT_USING_DFS_RAMFS
#include <dfs_fs.h>
#include <dfs_ramfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <finsh.h>

#ifndef RAMFS_MOUNT_POINT
#define RAMFS_MOUNT_POINT   "/ramfs"
#endif

#ifndef RAMFS_BENCH_PATH
#define RAMFS_BENCH_PATH    RAMFS_MOUNT_POINT "/bench.bin"
#endif

/* 确保 path 为已存在的目录；不存在则创建 */
static int ensure_dir(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
            return 0;
        LOG_E("%s exists but is not a directory", path);
        return -1;
    }
    if (mkdir(path, 0777) == 0)
        return 0;
    LOG_E("mkdir %s failed", path);
    return -1;
}

/**
 * ramfs 读写测速（走 DFS 文件接口）。
 * 用法: ramfs_bench [总字节,默认65536] [块大小,默认4096]
 * 注意: ramfs 总池约 256KB，总字节勿过大。
 */
static int ramfs_bench(int argc, char **argv)
{
    rt_size_t total = 64U * 1024U;
    rt_size_t chunk = 4096;
    rt_uint8_t *buf;
    int fd;
    rt_size_t off;
    rt_tick_t t0, t1, dt;
    rt_ssize_t n;
    rt_uint32_t ms, bps;

    if (argc >= 2)
        total = (rt_size_t)strtoul(argv[1], RT_NULL, 0);
    if (argc >= 3)
        chunk = (rt_size_t)strtoul(argv[2], RT_NULL, 0);
    if (chunk == 0 || total == 0)
    {
        rt_kprintf("usage: ramfs_bench [total_bytes] [chunk_bytes]\n");
        return 0;
    }

    buf = (rt_uint8_t *)rt_malloc(chunk);
    if (buf == RT_NULL)
    {
        rt_kprintf("malloc %u failed\n", (unsigned)chunk);
        return 0;
    }
    for (off = 0; off < chunk; off++)
        buf[off] = (rt_uint8_t)(off & 0xFF);

    unlink(RAMFS_BENCH_PATH);

    /* ---- write ---- */
    fd = open(RAMFS_BENCH_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0)
    {
        rt_kprintf("open %s for write failed\n", RAMFS_BENCH_PATH);
        rt_free(buf);
        return 0;
    }

    rt_kprintf("ramfs write: total=%u chunk=%u -> %s\n",
               (unsigned)total, (unsigned)chunk, RAMFS_BENCH_PATH);
    t0 = rt_tick_get();
    for (off = 0; off < total; off += chunk)
    {
        rt_size_t len = chunk;
        if (off + len > total)
            len = total - off;
        n = write(fd, buf, len);
        if (n != (rt_ssize_t)len)
        {
            rt_kprintf("write fail @%u ret=%d (pool full?)\n", (unsigned)off, (int)n);
            close(fd);
            rt_free(buf);
            unlink(RAMFS_BENCH_PATH);
            return 0;
        }
    }
    t1 = rt_tick_get();
    close(fd);
    dt = t1 - t0;
    ms = (rt_uint32_t)(dt * 1000U / RT_TICK_PER_SECOND);
    if (ms == 0)
        ms = 1;
    bps = (rt_uint32_t)((total * 1000ULL) / ms);
    rt_kprintf("write OK: %u bytes in %u ms => %u B/s (%u KB/s)\n",
               (unsigned)total, (unsigned)ms, (unsigned)bps, (unsigned)(bps / 1024U));

    /* ---- read ---- */
    fd = open(RAMFS_BENCH_PATH, O_RDONLY, 0);
    if (fd < 0)
    {
        rt_kprintf("open %s for read failed\n", RAMFS_BENCH_PATH);
        rt_free(buf);
        unlink(RAMFS_BENCH_PATH);
        return 0;
    }

    rt_kprintf("ramfs read: total=%u chunk=%u\n", (unsigned)total, (unsigned)chunk);
    t0 = rt_tick_get();
    for (off = 0; off < total; off += chunk)
    {
        rt_size_t len = chunk;
        if (off + len > total)
            len = total - off;
        n = read(fd, buf, len);
        if (n != (rt_ssize_t)len)
        {
            rt_kprintf("read fail @%u ret=%d\n", (unsigned)off, (int)n);
            close(fd);
            rt_free(buf);
            unlink(RAMFS_BENCH_PATH);
            return 0;
        }
    }
    t1 = rt_tick_get();
    close(fd);
    dt = t1 - t0;
    ms = (rt_uint32_t)(dt * 1000U / RT_TICK_PER_SECOND);
    if (ms == 0)
        ms = 1;
    bps = (rt_uint32_t)((total * 1000ULL) / ms);
    rt_kprintf("read OK: %u bytes in %u ms => %u B/s (%u KB/s)\n",
               (unsigned)total, (unsigned)ms, (unsigned)bps, (unsigned)(bps / 1024U));

    unlink(RAMFS_BENCH_PATH);
    rt_free(buf);
    return 0;
}
MSH_CMD_EXPORT(ramfs_bench, ramfs DFS read/write speed test);

static int board_ramfs_init(void)
{
    struct dfs_ramfs *ramfs;
    rt_uint8_t *pool;
    rt_size_t size;

#if defined(__GNUC__) && !defined(__ARMCC_VERSION)
    pool = (rt_uint8_t *)&_sramfs_pool;
    size = (rt_size_t)((rt_uint8_t *)&_eramfs_pool - (rt_uint8_t *)&_sramfs_pool);
#elif defined(__ARMCC_VERSION)
    /* Keil: 使用固定地址与 scatter 中 RW_RAMFS 一致 */
    pool = (rt_uint8_t *)0x30000000;
    size = 256U * 1024U;
#elif defined(__ICCARM__)
    pool = (rt_uint8_t *)0x30000000;
    size = 256U * 1024U;
#else
    return -RT_ERROR;
#endif

    if (size < 4096)
    {
        LOG_E("ramfs pool too small: %u", (unsigned)size);
        return -RT_ERROR;
    }

    ramfs = dfs_ramfs_create(pool, size);
    if (ramfs == RT_NULL)
    {
        LOG_E("dfs_ramfs_create failed");
        return -RT_ERROR;
    }

    /* 挂载点不存在则先在根文件系统上创建 */
    if (ensure_dir(RAMFS_MOUNT_POINT) != 0)
        return -RT_ERROR;

    if (dfs_mount(RT_NULL, RAMFS_MOUNT_POINT, "ram", 0, (const void *)ramfs) != 0)
    {
        LOG_E("mount %s failed", RAMFS_MOUNT_POINT);
        return -RT_ERROR;
    }

    LOG_I("ramfs mounted on %s, pool=%p size=%u", RAMFS_MOUNT_POINT, pool, (unsigned)size);

    return RT_EOK;
}
INIT_APP_EXPORT(board_ramfs_init);
#endif /* RT_USING_DFS_RAMFS */
