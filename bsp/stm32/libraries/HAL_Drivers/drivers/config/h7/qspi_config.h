/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-12-22     zylx         first version
 */

#ifndef __QSPI_CONFIG_H__
#define __QSPI_CONFIG_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BSP_USING_QSPI
#ifndef QSPI_BUS_CONFIG
#define QSPI_BUS_CONFIG                                        \
    {                                                          \
        .Instance = QUADSPI,                                   \
        .Init.FifoThreshold = 4,                               \
        .Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE, \
        .Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_4_CYCLE,  \
    }
#endif /* QSPI_BUS_CONFIG */
#endif /* BSP_USING_QSPI */

/*
 * STM32H7 QUADSPI 使用 MDMA（见 drv_qspi.c），不用 DMA1/2。
 * 下列 QSPI_DMA_CONFIG 仅给非 H7 系列参考，H7 不会引用。
 */
#if defined(BSP_QSPI_USING_DMA) && !defined(SOC_SERIES_STM32H7)
#ifndef QSPI_DMA_CONFIG
#define QSPI_DMA_CONFIG                                        \
    {                                                          \
        .Instance = QSPI_DMA_INSTANCE,                         \
        .Init.Channel  = QSPI_DMA_CHANNEL,                     \
        .Init.Direction = DMA_PERIPH_TO_MEMORY,                \
        .Init.PeriphInc = DMA_PINC_DISABLE,                    \
        .Init.MemInc = DMA_MINC_ENABLE,                        \
        .Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE,       \
        .Init.MemDataAlignment = DMA_MDATAALIGN_BYTE,          \
        .Init.Mode = DMA_NORMAL,                               \
        .Init.Priority = DMA_PRIORITY_LOW                      \
    }
#endif /* QSPI_DMA_CONFIG */
#endif /* BSP_QSPI_USING_DMA && !H7 */

#define QSPI_IRQn                   QUADSPI_IRQn
#define QSPI_IRQHandler             QUADSPI_IRQHandler

#ifdef __cplusplus
}
#endif

#endif /* __QSPI_CONFIG_H__ */
