/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-5      SummerGift   first version
 */

#ifndef __BOARD_H__
#define __BOARD_H__

#include <rtthread.h>
#include <stm32h7xx.h>
#include "drv_common.h"
#include "drv_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STM32_FLASH_START_ADRESS     ((uint32_t)0x08000000)
#define STM32_FLASH_SIZE             (2048 * 1024)
#define STM32_FLASH_END_ADDRESS      ((uint32_t)(STM32_FLASH_START_ADRESS + STM32_FLASH_SIZE))

/* 堆放在 D1 AXI SRAM */
#define STM32_SRAM_SIZE           (512)
#define STM32_SRAM_END            (0x24000000 + STM32_SRAM_SIZE * 1024)

/* D2：ramfs = SRAM1+2 (256KB)，AUX = SRAM3 (32KB) */
#define STM32_RAMFS_POOL_ADDR     ((uint32_t)0x30000000)
#define STM32_RAMFS_POOL_SIZE     (256U * 1024U)
#define STM32_RAM_D2_AUX_ADDR     ((uint32_t)0x30040000)
#define STM32_RAM_D2_AUX_SIZE     (32U * 1024U)

/* D3：USB/DMA noncacheable */
#define STM32_RAM_D3_ADDR         ((uint32_t)0x38000000)
#define STM32_RAM_D3_SIZE         (64U * 1024U)

#if defined(__ARMCC_VERSION)
extern int Image$$RW_IRAM1$$ZI$$Limit;
#define HEAP_BEGIN      (&Image$$RW_IRAM1$$ZI$$Limit)
#elif __ICCARM__
#pragma section="CSTACK"
#define HEAP_BEGIN      (__segment_end("CSTACK"))
#else
extern int __bss_end;
#define HEAP_BEGIN      (&__bss_end)
#endif

#define HEAP_END        STM32_SRAM_END

void SystemClock_Config(void);
void board_memory_early_init(void);

#ifdef __cplusplus
}
#endif

#endif
