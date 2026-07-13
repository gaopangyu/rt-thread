/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-19     wanghaijing  the first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <stdlib.h>
#include <finsh.h>
#include <dev_spi_flash.h>
#include <dev_spi_flash_sfud.h>
#include <drv_qspi.h>
#include <sfud.h>
#include "stm32h7xx.h"
#include "board.h"

#ifdef BSP_USING_QSPI_FLASH

char w25qxx_read_status_register2(struct rt_qspi_device *device)
{
    /* 0x35 read status register2 */
    char instruction = 0x35, status;

    rt_qspi_send_then_recv(device, &instruction, 1, &status, 1);

    return status;
}

void w25qxx_write_enable(struct rt_qspi_device *device)
{
    /* 0x06 write enable */
    char instruction = 0x06;

    rt_qspi_send(device, &instruction, 1);
}

void w25qxx_enter_qspi_mode(struct rt_qspi_device *device)
{
    char status = 0;
    /* 0x38 enter qspi mode */
    char instruction = 0x38;
    char write_status2_buf[2] = {0};

    /* 0x31 write status register2 */
    write_status2_buf[0] = 0x31;

    status = w25qxx_read_status_register2(device);
    LOG_I("W25Q SR2=0x%02X, QE=%d", (unsigned)(rt_uint8_t)status, (status & 0x02) ? 1 : 0);

    if (!(status & 0x02))
    {
        status |= 1 << 1;
        w25qxx_write_enable(device);
        write_status2_buf[1] = status;
        rt_qspi_send(device, &write_status2_buf, 2);
        rt_qspi_send(device, &instruction, 1);
        LOG_I("enabled QE and sent Enter QPI (0x38)");
        rt_thread_mdelay(10);
    }
    else
    {
        /* QE already set: Quad I/O cmds (0xEB) usable in standard SPI mode; no need 0x38 */
        LOG_I("QE already set, skip Enter QPI");
    }
}

static void qspi_flash_dump_hw(void)
{
    uint32_t hclk = HAL_RCC_GetHCLKFreq();
    uint32_t presc = (QUADSPI->CR >> 24) & 0xFFU;
    uint32_t qspi_clk = hclk / (presc + 1U);
    struct rt_qspi_device *qspi_dev;

    LOG_I("---- QSPI HW ----");
    LOG_I("HCLK=%u Hz, RT_SFUD_SPI_MAX_HZ=%u", (unsigned)hclk, (unsigned)RT_SFUD_SPI_MAX_HZ);
    LOG_I("QUADSPI->CR=0x%08X PRESCALER=%u => QSPI_CLK~%u Hz",
          (unsigned)QUADSPI->CR, (unsigned)presc, (unsigned)qspi_clk);
    LOG_I("QUADSPI->DCR=0x%08X", (unsigned)QUADSPI->DCR);

    qspi_dev = (struct rt_qspi_device *)rt_device_find("qspi10");
    if (qspi_dev != RT_NULL)
    {
        LOG_I("qspi10: max_hz=%u, dl_width=%u, medium_size=%u",
              (unsigned)qspi_dev->config.parent.max_hz,
              (unsigned)qspi_dev->config.qspi_dl_width,
              (unsigned)qspi_dev->config.medium_size);
    }
}

static void qspi_flash_dump_sfud(sfud_flash_t flash)
{
    if (flash == RT_NULL)
        return;

    LOG_I("---- SFUD ----");
    LOG_I("chip=%s JEDEC=%02X %02X %02X capacity=%u",
          flash->chip.name ? flash->chip.name : "?",
          flash->chip.mf_id, flash->chip.type_id, flash->chip.capacity_id,
          (unsigned)flash->chip.capacity);
#ifdef SFUD_USING_QSPI
    LOG_I("read_cmd: ins=0x%02X ins_lines=%u addr_lines=%u dummy=%u data_lines=%u addr_size=%u",
          flash->read_cmd_format.instruction,
          flash->read_cmd_format.instruction_lines,
          flash->read_cmd_format.address_lines,
          flash->read_cmd_format.dummy_cycles,
          flash->read_cmd_format.data_lines,
          flash->read_cmd_format.address_size);
    if (flash->read_cmd_format.instruction == 0x03)
        LOG_W("read still uses 0x03 (1-1-1) — Quad Fast Read NOT active");
    else if (flash->read_cmd_format.instruction == 0xEB &&
             flash->read_cmd_format.data_lines == 4)
        LOG_I("Quad I/O Fast Read (0xEB, data 4-line) OK");
    else
        LOG_W("unexpected read format, check SFUD table / QE");
#endif
}

static void qspi_flash_dump_all(void)
{
    sfud_flash_t flash;

    qspi_flash_dump_hw();
    flash = rt_sfud_flash_find_by_dev_name(FAL_USING_NOR_FLASH_DEV_NAME);
    if (flash == RT_NULL)
        flash = rt_sfud_flash_find("qspi10");
    qspi_flash_dump_sfud(flash);
}

/**
 * 只读测速，不擦写，不影响文件系统。
 * 用法: qspi_rdbench [总字节,默认1MB] [块大小,默认4096]
 */
static int qspi_rdbench(int argc, char **argv)
{
    sfud_flash_t flash;
    rt_uint8_t *buf;
    rt_size_t total = 1U * 1024U * 1024U;
    rt_size_t chunk = 4096;
    rt_size_t off;
    rt_tick_t t0, t1, dt;
    sfud_err err = SFUD_SUCCESS;

    if (argc >= 2)
        total = (rt_size_t)strtoul(argv[1], RT_NULL, 0);
    if (argc >= 3)
        chunk = (rt_size_t)strtoul(argv[2], RT_NULL, 0);
    if (chunk == 0 || total == 0)
    {
        rt_kprintf("usage: qspi_rdbench [total_bytes] [chunk_bytes]\n");
        return 0;
    }

    flash = rt_sfud_flash_find_by_dev_name(FAL_USING_NOR_FLASH_DEV_NAME);
    if (flash == RT_NULL)
    {
        rt_kprintf("sfud flash '%s' not found\n", FAL_USING_NOR_FLASH_DEV_NAME);
        return 0;
    }

    qspi_flash_dump_all();

    if (total > flash->chip.capacity)
        total = flash->chip.capacity;

    buf = (rt_uint8_t *)rt_malloc(chunk);
    if (buf == RT_NULL)
    {
        rt_kprintf("malloc %u failed\n", (unsigned)chunk);
        return 0;
    }

    rt_kprintf("rdbench: total=%u chunk=%u (read-only)\n", (unsigned)total, (unsigned)chunk);
    t0 = rt_tick_get();
    for (off = 0; off < total; off += chunk)
    {
        rt_size_t n = chunk;
        if (off + n > total)
            n = total - off;
        err = sfud_read(flash, off, n, buf);
        if (err != SFUD_SUCCESS)
        {
            rt_kprintf("sfud_read fail @0x%X err=%d\n", (unsigned)off, (int)err);
            break;
        }
    }
    t1 = rt_tick_get();
    dt = t1 - t0;

    if (err == SFUD_SUCCESS)
    {
        rt_uint32_t ms = (rt_uint32_t)(dt * 1000U / RT_TICK_PER_SECOND);
        rt_uint32_t bps = (ms > 0) ? (rt_uint32_t)((total * 1000ULL) / ms) : 0;
        rt_kprintf("rdbench OK: %u bytes in %u ms => %u B/s (%u KB/s)\n",
                   (unsigned)total, (unsigned)ms, (unsigned)bps, (unsigned)(bps / 1024U));
    }

    rt_free(buf);
    return 0;
}
MSH_CMD_EXPORT(qspi_rdbench, QSPI/SFUD read-only speed diag);

static int qspi_diag(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    qspi_flash_dump_all();
    return 0;
}
MSH_CMD_EXPORT(qspi_diag, dump QSPI clock and SFUD read format);

static int rt_qspi_flash_init(void)
{
    extern rt_spi_flash_device_t rt_sfud_flash_probe(const char *spi_flash_dev_name, const char *spi_dev_name);
    sfud_flash_t flash;

    rt_hw_qspi_device_attach("qspi1", "qspi10", RT_NULL, 4, w25qxx_enter_qspi_mode, RT_NULL);
    if (RT_NULL == rt_sfud_flash_probe(FAL_USING_NOR_FLASH_DEV_NAME, "qspi10"))
    {
        return -RT_ERROR;
    }

    /* probe 后再 dump（此时 PRESCALER / Quad 读格式已生效） */
    qspi_flash_dump_hw();
    flash = rt_sfud_flash_find_by_dev_name(FAL_USING_NOR_FLASH_DEV_NAME);
    qspi_flash_dump_sfud(flash);

    return RT_EOK;
}
INIT_DEVICE_EXPORT(rt_qspi_flash_init);

#endif/* BSP_USING_QSPI_FLASH */
