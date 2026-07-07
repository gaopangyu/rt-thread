/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-01-02     zylx         first version
 * 2019-01-08     SummerGift   clean up the code
 * 2020-05-02     whj4674672   support stm32h7 dma1 and dma2
 */

#ifndef __DMA_CONFIG_H__
#define __DMA_CONFIG_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fixed UART DMA stream assignment (H7 DMAMUX).
 * Each enabled UART RX/TX gets a dedicated stream so UART1~UART8
 * can use DMA concurrently. SPI/QSPI use remaining streams via #elif.
 *
 * DMA2: USART1/2/3, UART4
 * DMA1: UART5/7/8, USART6
 */
#if defined(BSP_UART1_RX_USING_DMA) && !defined(UART1_RX_DMA_INSTANCE)
#define UART1_DMA_RX_IRQHandler          DMA2_Stream0_IRQHandler
#define UART1_RX_DMA_RCC                 RCC_AHB1ENR_DMA2EN
#define UART1_RX_DMA_INSTANCE            DMA2_Stream0
#define UART1_RX_DMA_REQUEST             DMA_REQUEST_USART1_RX
#define UART1_RX_DMA_IRQ                 DMA2_Stream0_IRQn
#endif

#if defined(BSP_UART1_TX_USING_DMA) && !defined(UART1_TX_DMA_INSTANCE)
#define UART1_DMA_TX_IRQHandler          DMA2_Stream1_IRQHandler
#define UART1_TX_DMA_RCC                 RCC_AHB1ENR_DMA2EN
#define UART1_TX_DMA_INSTANCE            DMA2_Stream1
#define UART1_TX_DMA_REQUEST             DMA_REQUEST_USART1_TX
#define UART1_TX_DMA_IRQ                 DMA2_Stream1_IRQn
#endif

#if defined(BSP_UART2_RX_USING_DMA) && !defined(UART2_RX_DMA_INSTANCE)
#define UART2_DMA_RX_IRQHandler          DMA2_Stream2_IRQHandler
#define UART2_RX_DMA_RCC                 RCC_AHB1ENR_DMA2EN
#define UART2_RX_DMA_INSTANCE            DMA2_Stream2
#define UART2_RX_DMA_REQUEST             DMA_REQUEST_USART2_RX
#define UART2_RX_DMA_IRQ                 DMA2_Stream2_IRQn
#endif

#if defined(BSP_UART2_TX_USING_DMA) && !defined(UART2_TX_DMA_INSTANCE)
#define UART2_DMA_TX_IRQHandler          DMA2_Stream3_IRQHandler
#define UART2_TX_DMA_RCC                 RCC_AHB1ENR_DMA2EN
#define UART2_TX_DMA_INSTANCE            DMA2_Stream3
#define UART2_TX_DMA_REQUEST             DMA_REQUEST_USART2_TX
#define UART2_TX_DMA_IRQ                 DMA2_Stream3_IRQn
#endif

#if defined(BSP_UART3_RX_USING_DMA) && !defined(UART3_RX_DMA_INSTANCE)
#define UART3_DMA_RX_IRQHandler          DMA2_Stream4_IRQHandler
#define UART3_RX_DMA_RCC                 RCC_AHB1ENR_DMA2EN
#define UART3_RX_DMA_INSTANCE            DMA2_Stream4
#define UART3_RX_DMA_REQUEST             DMA_REQUEST_USART3_RX
#define UART3_RX_DMA_IRQ                 DMA2_Stream4_IRQn
#endif

#if defined(BSP_UART3_TX_USING_DMA) && !defined(UART3_TX_DMA_INSTANCE)
#define UART3_DMA_TX_IRQHandler          DMA2_Stream5_IRQHandler
#define UART3_TX_DMA_RCC                 RCC_AHB1ENR_DMA2EN
#define UART3_TX_DMA_INSTANCE            DMA2_Stream5
#define UART3_TX_DMA_REQUEST             DMA_REQUEST_USART3_TX
#define UART3_TX_DMA_IRQ                 DMA2_Stream5_IRQn
#endif

#if defined(BSP_UART4_RX_USING_DMA) && !defined(UART4_RX_DMA_INSTANCE)
#define UART4_DMA_RX_IRQHandler          DMA2_Stream6_IRQHandler
#define UART4_RX_DMA_RCC                 RCC_AHB1ENR_DMA2EN
#define UART4_RX_DMA_INSTANCE            DMA2_Stream6
#define UART4_RX_DMA_REQUEST             DMA_REQUEST_UART4_RX
#define UART4_RX_DMA_IRQ                 DMA2_Stream6_IRQn
#endif

#if defined(BSP_UART4_TX_USING_DMA) && !defined(UART4_TX_DMA_INSTANCE)
#define UART4_DMA_TX_IRQHandler          DMA2_Stream7_IRQHandler
#define UART4_TX_DMA_RCC                 RCC_AHB1ENR_DMA2EN
#define UART4_TX_DMA_INSTANCE            DMA2_Stream7
#define UART4_TX_DMA_REQUEST             DMA_REQUEST_UART4_TX
#define UART4_TX_DMA_IRQ                 DMA2_Stream7_IRQn
#endif

#if defined(BSP_UART5_RX_USING_DMA) && !defined(UART5_RX_DMA_INSTANCE)
#define UART5_DMA_RX_IRQHandler          DMA1_Stream0_IRQHandler
#define UART5_RX_DMA_RCC                 RCC_AHB1ENR_DMA1EN
#define UART5_RX_DMA_INSTANCE            DMA1_Stream0
#define UART5_RX_DMA_REQUEST             DMA_REQUEST_UART5_RX
#define UART5_RX_DMA_IRQ                 DMA1_Stream0_IRQn
#endif

#if defined(BSP_UART5_TX_USING_DMA) && !defined(UART5_TX_DMA_INSTANCE)
#define UART5_DMA_TX_IRQHandler          DMA1_Stream1_IRQHandler
#define UART5_TX_DMA_RCC                 RCC_AHB1ENR_DMA1EN
#define UART5_TX_DMA_INSTANCE            DMA1_Stream1
#define UART5_TX_DMA_REQUEST             DMA_REQUEST_UART5_TX
#define UART5_TX_DMA_IRQ                 DMA1_Stream1_IRQn
#endif

#if defined(BSP_UART6_RX_USING_DMA) && !defined(UART6_RX_DMA_INSTANCE)
#define UART6_DMA_RX_IRQHandler          DMA1_Stream2_IRQHandler
#define UART6_RX_DMA_RCC                 RCC_AHB1ENR_DMA1EN
#define UART6_RX_DMA_INSTANCE            DMA1_Stream2
#define UART6_RX_DMA_REQUEST             DMA_REQUEST_USART6_RX
#define UART6_RX_DMA_IRQ                 DMA1_Stream2_IRQn
#endif

#if defined(BSP_UART6_TX_USING_DMA) && !defined(UART6_TX_DMA_INSTANCE)
#define UART6_DMA_TX_IRQHandler          DMA1_Stream3_IRQHandler
#define UART6_TX_DMA_RCC                 RCC_AHB1ENR_DMA1EN
#define UART6_TX_DMA_INSTANCE            DMA1_Stream3
#define UART6_TX_DMA_REQUEST             DMA_REQUEST_USART6_TX
#define UART6_TX_DMA_IRQ                 DMA1_Stream3_IRQn
#endif

#if defined(BSP_UART7_RX_USING_DMA) && !defined(UART7_RX_DMA_INSTANCE)
#define UART7_DMA_RX_IRQHandler          DMA1_Stream4_IRQHandler
#define UART7_RX_DMA_RCC                 RCC_AHB1ENR_DMA1EN
#define UART7_RX_DMA_INSTANCE            DMA1_Stream4
#define UART7_RX_DMA_REQUEST             DMA_REQUEST_UART7_RX
#define UART7_RX_DMA_IRQ                 DMA1_Stream4_IRQn
#endif

#if defined(BSP_UART7_TX_USING_DMA) && !defined(UART7_TX_DMA_INSTANCE)
#define UART7_DMA_TX_IRQHandler          DMA1_Stream5_IRQHandler
#define UART7_TX_DMA_RCC                 RCC_AHB1ENR_DMA1EN
#define UART7_TX_DMA_INSTANCE            DMA1_Stream5
#define UART7_TX_DMA_REQUEST             DMA_REQUEST_UART7_TX
#define UART7_TX_DMA_IRQ                 DMA1_Stream5_IRQn
#endif

#if defined(BSP_UART8_RX_USING_DMA) && !defined(UART8_RX_DMA_INSTANCE)
#define UART8_DMA_RX_IRQHandler          DMA1_Stream6_IRQHandler
#define UART8_RX_DMA_RCC                 RCC_AHB1ENR_DMA1EN
#define UART8_RX_DMA_INSTANCE            DMA1_Stream6
#define UART8_RX_DMA_REQUEST             DMA_REQUEST_UART8_RX
#define UART8_RX_DMA_IRQ                 DMA1_Stream6_IRQn
#endif

#if defined(BSP_UART8_TX_USING_DMA) && !defined(UART8_TX_DMA_INSTANCE)
#define UART8_DMA_TX_IRQHandler          DMA1_Stream7_IRQHandler
#define UART8_TX_DMA_RCC                 RCC_AHB1ENR_DMA1EN
#define UART8_TX_DMA_INSTANCE            DMA1_Stream7
#define UART8_TX_DMA_REQUEST             DMA_REQUEST_UART8_TX
#define UART8_TX_DMA_IRQ                 DMA1_Stream7_IRQn
#endif

/* DMA2 stream7: QSPI (only when UART4 TX DMA is disabled) */
#if defined(BSP_QSPI_USING_DMA) && !defined(QSPI_DMA_INSTANCE) \
    && !defined(BSP_UART4_TX_USING_DMA)
#define QSPI_DMA_IRQHandler              DMA2_Stream7_IRQHandler
#define QSPI_DMA_RCC                     RCC_AHB1ENR_DMA2EN
#define QSPI_DMA_INSTANCE                DMA2_Stream7
#define QSPI_DMA_IRQ                     DMA2_Stream7_IRQn
#endif

#ifdef __cplusplus
}
#endif

#endif /* __DMA_CONFIG_H__ */
