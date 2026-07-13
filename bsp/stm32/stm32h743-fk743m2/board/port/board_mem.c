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
 * 由 board.c 的 SystemClock_Config() 开头调用（与 artpi 一样把板级初始化放 board.c）。
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

#ifndef RAMFS_MOUNT_POINT
#define RAMFS_MOUNT_POINT   "/ramfs"
#endif

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

    if (dfs_mount(RT_NULL, RAMFS_MOUNT_POINT, "ram", 0, (const void *)ramfs) != 0)
    {
        LOG_E("mount %s failed", RAMFS_MOUNT_POINT);
        return -RT_ERROR;
    }

    mkdir(RAMFS_MOUNT_POINT "/log", 0777);
    LOG_I("ramfs mounted on %s, pool=%p size=%u", RAMFS_MOUNT_POINT, pool, (unsigned)size);

    return RT_EOK;
}
INIT_ENV_EXPORT(board_ramfs_init);
#endif /* RT_USING_DFS_RAMFS */
