/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-27     zylx         first version
 * 2026-07-13     gaopangyu    STM32H7 QSPI uses MDMA (not DMA1/2)
 */

#include "board.h"
#include "drv_qspi.h"
#include "drv_config.h"

#ifdef RT_USING_QSPI

#define DRV_DEBUG
#define LOG_TAG              "drv.qspi"
#include <drv_log.h>

#if defined(BSP_USING_QSPI)

struct stm32_qspi_bus
{
    QSPI_HandleTypeDef QSPI_Handler;
    char *bus_name;
#ifdef BSP_QSPI_USING_DMA
#if defined(SOC_SERIES_STM32H7)
    MDMA_HandleTypeDef hmdma_quadspi;
#else
    DMA_HandleTypeDef hdma_quadspi;
#endif
#endif
};

struct rt_spi_bus _qspi_bus1;
struct stm32_qspi_bus _stm32_qspi_bus;
#ifdef BSP_QSPI_USING_DMA
static rt_uint8_t _qspi_dma_ready = 0;
#endif

#ifdef BSP_QSPI_USING_DMA
#if defined(SOC_SERIES_STM32H7)
/* 与 MDMA BufferTransferLength / QSPI FifoThreshold 一致 */
#ifndef QSPI_MDMA_BTL
#define QSPI_MDMA_BTL   4u
#endif

/*
 * H7 MDMA 限制：
 * 1) 不能访问 DTCM(0x20000000)
 * 2) FIFO_TH 模式下长度需为 BufferTransferLength 的整数倍
 * 否则 SFUD 探测等小包读会 HardFault / Transfer Error
 */
static rt_bool_t qspi_recv_can_use_mdma(const rt_uint8_t *buf, rt_int32_t length)
{
    rt_uint32_t addr = (rt_uint32_t)buf;

    if (!_qspi_dma_ready || buf == RT_NULL || length <= 0)
        return RT_FALSE;
    if ((rt_uint32_t)length < QSPI_MDMA_BTL ||
        ((rt_uint32_t)length % QSPI_MDMA_BTL) != 0)
        return RT_FALSE;
    /* DTCM：仅 CPU 可访问 */
    if (addr >= 0x20000000u && addr < 0x20020000u)
        return RT_FALSE;
    /* 堆 / 常用 SRAM：AXI D1、D2、D3 */
    if ((addr >= 0x24000000u && addr < 0x24080000u) ||
        (addr >= 0x30000000u && addr < 0x30048000u) ||
        (addr >= 0x38000000u && addr < 0x38010000u))
        return RT_TRUE;

    return RT_FALSE;
}
#endif /* SOC_SERIES_STM32H7 */

static rt_err_t qspi_dma_wait_ready(QSPI_HandleTypeDef *hqspi)
{
    rt_tick_t start = rt_tick_get();

    while (hqspi->State != HAL_QSPI_STATE_READY)
    {
        if (hqspi->ErrorCode != HAL_QSPI_ERROR_NONE)
            return -RT_ERROR;
        if ((rt_tick_get() - start) > rt_tick_from_millisecond(5000))
        {
            LOG_E("QSPI DMA wait timeout, state=%u err=0x%X",
                  (unsigned)hqspi->State, (unsigned)hqspi->ErrorCode);
            hqspi->State = HAL_QSPI_STATE_READY;
            return -RT_ETIMEOUT;
        }
    }
    return RT_EOK;
}
#endif

static int stm32_qspi_init(struct rt_qspi_device *device, struct rt_qspi_configuration *qspi_cfg)
{
    int result = RT_EOK;
    unsigned int i = 1;
#ifdef BSP_QSPI_USING_DMA
#if defined(SOC_SERIES_STM32H7)
    MDMA_HandleTypeDef *hmdma_save;
#else
    DMA_HandleTypeDef *hdma_save;
#endif
#endif

    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(qspi_cfg != RT_NULL);

    struct rt_spi_configuration *cfg = &qspi_cfg->parent;
    struct stm32_qspi_bus *qspi_bus = device->parent.bus->parent.user_data;

    /*
     * 禁止整结构 memset：会把已 link 的 hmdma 清成 NULL，
     * 而 MDMA IRQ 仍使能 → 空指针 HardFault（MMFAR=0）。
     */
#ifdef BSP_QSPI_USING_DMA
#if defined(SOC_SERIES_STM32H7)
    hmdma_save = qspi_bus->QSPI_Handler.hmdma;
#else
    hdma_save = qspi_bus->QSPI_Handler.hdma;
#endif
#endif
    {
        QSPI_HandleTypeDef QSPI_Handler_config = QSPI_BUS_CONFIG;
        qspi_bus->QSPI_Handler = QSPI_Handler_config;
    }
#ifdef BSP_QSPI_USING_DMA
#if defined(SOC_SERIES_STM32H7)
    qspi_bus->QSPI_Handler.hmdma = hmdma_save;
#else
    qspi_bus->QSPI_Handler.hdma = hdma_save;
#endif
#endif

#if defined(SOC_SERIES_STM32MP1)
    while (cfg->max_hz < HAL_RCC_GetACLKFreq() / (i + 1))
#else
    while (cfg->max_hz < HAL_RCC_GetHCLKFreq() / (i + 1))
#endif
   {
        i++;
        if (i == 255)
        {
            LOG_E("QSPI init failed, QSPI frequency(%d) is too low.", cfg->max_hz);
            return -RT_ERROR;
        }
    }
    /* f_qspi = f_kernel / (1+i) */
    qspi_bus->QSPI_Handler.Init.ClockPrescaler = i;

    if (!(cfg->mode & RT_SPI_CPOL))
    {
        /* QSPI MODE0 */
        qspi_bus->QSPI_Handler.Init.ClockMode = QSPI_CLOCK_MODE_0;
    }
    else
    {
        /* QSPI MODE3 */
        qspi_bus->QSPI_Handler.Init.ClockMode = QSPI_CLOCK_MODE_3;
    }

    /* flash size */
    qspi_bus->QSPI_Handler.Init.FlashSize = POSITION_VAL(qspi_cfg->medium_size) - 1;

#if defined(BSP_QSPI_USING_DMA) && defined(SOC_SERIES_STM32H7)
    /* FifoThreshold 必须与 MDMA BufferTransferLength 一致 */
    qspi_bus->QSPI_Handler.Init.FifoThreshold = QSPI_MDMA_BTL;
#endif

    result = HAL_QSPI_Init(&qspi_bus->QSPI_Handler);
    if (result  == HAL_OK)
    {
        LOG_D("qspi init success!");
    }
    else
    {
        LOG_E("qspi init failed (%d)!", result);
        return result;
    }

#ifdef BSP_QSPI_USING_DMA
    /* MDMA/DMA 只初始化一次；反复 DeInit/EnableIRQ 易在 SFUD probe 时踩空指针 */
    if (_qspi_dma_ready)
    {
#if defined(SOC_SERIES_STM32H7)
        __HAL_LINKDMA(&qspi_bus->QSPI_Handler, hmdma, qspi_bus->hmdma_quadspi);
#else
        __HAL_LINKDMA(&qspi_bus->QSPI_Handler, hdma, qspi_bus->hdma_quadspi);
#endif
        return RT_EOK;
    }

#if defined(SOC_SERIES_STM32H7)
    /* H7 QUADSPI 只能挂 MDMA（对齐 ST QSPI_ReadWrite_DMA 例程） */
    __HAL_RCC_MDMA_CLK_ENABLE();

    rt_memset(&qspi_bus->hmdma_quadspi, 0, sizeof(qspi_bus->hmdma_quadspi));
    /* 本驱动接收走 MDMA：FIFO 不递增 → 内存递增（HAL_QSPI_Receive_DMA 会再写 CTCR） */
    qspi_bus->hmdma_quadspi.Instance                 = MDMA_Channel1;
    qspi_bus->hmdma_quadspi.Init.Request             = MDMA_REQUEST_QUADSPI_FIFO_TH;
    qspi_bus->hmdma_quadspi.Init.TransferTriggerMode = MDMA_BUFFER_TRANSFER;
    qspi_bus->hmdma_quadspi.Init.Priority            = MDMA_PRIORITY_HIGH;
    qspi_bus->hmdma_quadspi.Init.Endianness          = MDMA_LITTLE_ENDIANNESS_PRESERVE;
    qspi_bus->hmdma_quadspi.Init.SourceInc           = MDMA_SRC_INC_DISABLE;
    qspi_bus->hmdma_quadspi.Init.DestinationInc      = MDMA_DEST_INC_BYTE;
    qspi_bus->hmdma_quadspi.Init.SourceDataSize      = MDMA_SRC_DATASIZE_BYTE;
    qspi_bus->hmdma_quadspi.Init.DestDataSize        = MDMA_DEST_DATASIZE_BYTE;
    qspi_bus->hmdma_quadspi.Init.DataAlignment       = MDMA_DATAALIGN_PACKENABLE;
    qspi_bus->hmdma_quadspi.Init.BufferTransferLength = QSPI_MDMA_BTL;
    qspi_bus->hmdma_quadspi.Init.SourceBurst         = MDMA_SOURCE_BURST_SINGLE;
    qspi_bus->hmdma_quadspi.Init.DestBurst           = MDMA_DEST_BURST_SINGLE;
    qspi_bus->hmdma_quadspi.Init.SourceBlockAddressOffset = 0;
    qspi_bus->hmdma_quadspi.Init.DestBlockAddressOffset   = 0;

    __HAL_LINKDMA(&qspi_bus->QSPI_Handler, hmdma, qspi_bus->hmdma_quadspi);
    HAL_MDMA_DeInit(&qspi_bus->hmdma_quadspi);
    if (HAL_MDMA_Init(&qspi_bus->hmdma_quadspi) != HAL_OK)
    {
        LOG_E("qspi mdma init failed!");
        qspi_bus->QSPI_Handler.hmdma = RT_NULL;
        return -RT_ERROR;
    }

    HAL_NVIC_SetPriority(QSPI_IRQn, 0x0F, 0);
    HAL_NVIC_EnableIRQ(QSPI_IRQn);
    HAL_NVIC_SetPriority(MDMA_IRQn, 0x0F, 0);
    HAL_NVIC_EnableIRQ(MDMA_IRQn);

    _qspi_dma_ready = 1;
    LOG_I("QSPI MDMA ready (Ch1, BTL=%u); small/DTCM -> poll",
          (unsigned)QSPI_MDMA_BTL);
#else
    HAL_NVIC_SetPriority(QSPI_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(QSPI_IRQn);
    HAL_NVIC_SetPriority(QSPI_DMA_IRQ, 5, 0);
    HAL_NVIC_EnableIRQ(QSPI_DMA_IRQ);

    if (QSPI_DMA_RCC == RCC_AHB1ENR_DMA1EN)
        __HAL_RCC_DMA1_CLK_ENABLE();
    else
        __HAL_RCC_DMA2_CLK_ENABLE();

    {
        DMA_HandleTypeDef hdma_quadspi_config = QSPI_DMA_CONFIG;
        qspi_bus->hdma_quadspi = hdma_quadspi_config;
    }
    __HAL_LINKDMA(&qspi_bus->QSPI_Handler, hdma, qspi_bus->hdma_quadspi);
    if (qspi_bus->QSPI_Handler.hdma != RT_NULL)
        HAL_DMA_DeInit(qspi_bus->QSPI_Handler.hdma);
    if (HAL_DMA_Init(&qspi_bus->hdma_quadspi) != HAL_OK)
    {
        LOG_E("qspi dma init failed!");
        return -RT_ERROR;
    }
    _qspi_dma_ready = 1;
    LOG_I("QSPI DMA enabled");
#endif /* SOC_SERIES_STM32H7 */
#endif /* BSP_QSPI_USING_DMA */

    return result;
}

static void qspi_send_cmd(struct stm32_qspi_bus *qspi_bus, struct rt_qspi_message *message)
{
    RT_ASSERT(qspi_bus != RT_NULL);
    RT_ASSERT(message != RT_NULL);

    QSPI_CommandTypeDef Cmdhandler;

    /* set QSPI cmd struct */
    Cmdhandler.Instruction = message->instruction.content;
    Cmdhandler.Address = message->address.content;
    Cmdhandler.DummyCycles = message->dummy_cycles;
    if (message->instruction.qspi_lines == 0)
    {
        Cmdhandler.InstructionMode = QSPI_INSTRUCTION_NONE;
    }
    else if (message->instruction.qspi_lines == 1)
    {
        Cmdhandler.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    }
    else if (message->instruction.qspi_lines == 2)
    {
        Cmdhandler.InstructionMode = QSPI_INSTRUCTION_2_LINES;
    }
    else if (message->instruction.qspi_lines == 4)
    {
        Cmdhandler.InstructionMode = QSPI_INSTRUCTION_4_LINES;
    }
    if (message->address.qspi_lines == 0)
    {
        Cmdhandler.AddressMode = QSPI_ADDRESS_NONE;
    }
    else if (message->address.qspi_lines == 1)
    {
        Cmdhandler.AddressMode = QSPI_ADDRESS_1_LINE;
    }
    else if (message->address.qspi_lines == 2)
    {
        Cmdhandler.AddressMode = QSPI_ADDRESS_2_LINES;
    }
    else if (message->address.qspi_lines == 4)
    {
        Cmdhandler.AddressMode = QSPI_ADDRESS_4_LINES;
    }
    if (message->address.size == 24)
    {
        Cmdhandler.AddressSize = QSPI_ADDRESS_24_BITS;
    }
    else
    {
        Cmdhandler.AddressSize = QSPI_ADDRESS_32_BITS;
    }
    if (message->qspi_data_lines == 0)
    {
        Cmdhandler.DataMode = QSPI_DATA_NONE;
    }
    else if (message->qspi_data_lines == 1)
    {
        Cmdhandler.DataMode = QSPI_DATA_1_LINE;
    }
    else if (message->qspi_data_lines == 2)
    {
        Cmdhandler.DataMode = QSPI_DATA_2_LINES;
    }
    else if (message->qspi_data_lines == 4)
    {
        Cmdhandler.DataMode = QSPI_DATA_4_LINES;
    }

    Cmdhandler.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    Cmdhandler.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    Cmdhandler.DdrMode = QSPI_DDR_MODE_DISABLE;
    Cmdhandler.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    Cmdhandler.NbData = message->parent.length;
    HAL_QSPI_Command(&qspi_bus->QSPI_Handler, &Cmdhandler, 5000);
}

static rt_ssize_t qspixfer(struct rt_spi_device *device, struct rt_spi_message *message)
{
    rt_ssize_t result = 0;

    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(device->bus != RT_NULL);

    struct rt_qspi_message *qspi_message = (struct rt_qspi_message *)message;
    struct stm32_qspi_bus *qspi_bus = device->bus->parent.user_data;

    const rt_uint8_t *sndb = message->send_buf;
    rt_uint8_t *rcvb = message->recv_buf;
    rt_int32_t length = message->length;

#ifdef BSP_QSPI_USING_SOFTCS
    if (message->cs_take && (device->cs_pin != PIN_NONE))
    {
        rt_pin_write(device->cs_pin, PIN_LOW);
    }
#endif

    /* send data */
    if (sndb)
    {
        qspi_send_cmd(qspi_bus, qspi_message);
        if (qspi_message->parent.length != 0)
        {
            if (HAL_QSPI_Transmit(&qspi_bus->QSPI_Handler, (rt_uint8_t *)sndb, 5000) == HAL_OK)
            {
                result = length;
            }
            else
            {
                LOG_E("QSPI send data failed(%d)!", qspi_bus->QSPI_Handler.ErrorCode);
                qspi_bus->QSPI_Handler.State = HAL_QSPI_STATE_READY;
                result = -RT_ERROR;
                goto __exit;
            }
        }
        else
        {
            result = 1;
        }
    }
    else if (rcvb)/* recv data */
    {
        qspi_send_cmd(qspi_bus, qspi_message);
#ifdef BSP_QSPI_USING_DMA
#if defined(SOC_SERIES_STM32H7)
        if (qspi_recv_can_use_mdma(rcvb, length))
        {
            if (HAL_QSPI_Receive_DMA(&qspi_bus->QSPI_Handler, rcvb) == HAL_OK &&
                qspi_dma_wait_ready(&qspi_bus->QSPI_Handler) == RT_EOK)
            {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
                {
                    rt_uint32_t a = (rt_uint32_t)rcvb & ~31u;
                    rt_int32_t n = (rt_int32_t)((rt_uint32_t)rcvb + (rt_uint32_t)length - a);
                    n = (n + 31) & ~31;
                    SCB_InvalidateDCache_by_Addr((uint32_t *)a, n);
                }
#endif
                result = length;
            }
            else
            {
                LOG_E("QSPI MDMA recv failed(%d)!", qspi_bus->QSPI_Handler.ErrorCode);
                qspi_bus->QSPI_Handler.State = HAL_QSPI_STATE_READY;
                result = -RT_ERROR;
                goto __exit;
            }
        }
        else if (HAL_QSPI_Receive(&qspi_bus->QSPI_Handler, rcvb, 5000) == HAL_OK)
        {
            result = length;
        }
        else
        {
            LOG_E("QSPI poll recv failed(%d)!", qspi_bus->QSPI_Handler.ErrorCode);
            qspi_bus->QSPI_Handler.State = HAL_QSPI_STATE_READY;
            result = -RT_ERROR;
            goto __exit;
        }
#else
        if (HAL_QSPI_Receive_DMA(&qspi_bus->QSPI_Handler, rcvb) == HAL_OK)
        {
            if (qspi_dma_wait_ready(&qspi_bus->QSPI_Handler) != RT_EOK)
            {
                result = -RT_ERROR;
                goto __exit;
            }
            result = length;
        }
        else
        {
            LOG_E("QSPI recv data failed(%d)!", qspi_bus->QSPI_Handler.ErrorCode);
            qspi_bus->QSPI_Handler.State = HAL_QSPI_STATE_READY;
            result = -RT_ERROR;
            goto __exit;
        }
#endif /* SOC_SERIES_STM32H7 */
#else
        if (HAL_QSPI_Receive(&qspi_bus->QSPI_Handler, rcvb, 5000) == HAL_OK)
        {
            result = length;
        }
        else
        {
            LOG_E("QSPI recv data failed(%d)!", qspi_bus->QSPI_Handler.ErrorCode);
            qspi_bus->QSPI_Handler.State = HAL_QSPI_STATE_READY;
            result = -RT_ERROR;
            goto __exit;
        }
#endif /* BSP_QSPI_USING_DMA */
    }

__exit:
#ifdef BSP_QSPI_USING_SOFTCS
    if (message->cs_release && (device->cs_pin != PIN_NONE))
    {
        rt_pin_write(device->cs_pin, PIN_HIGH);
    }
#endif
    return result;
}

static rt_err_t qspi_configure(struct rt_spi_device *device, struct rt_spi_configuration *configuration)
{
    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(configuration != RT_NULL);

    struct rt_qspi_device *qspi_device = (struct rt_qspi_device *)device;
    return stm32_qspi_init(qspi_device, &qspi_device->config);
}

static const struct rt_spi_ops stm32_qspi_ops =
{
    .configure = qspi_configure,
    .xfer = qspixfer,
};

static int stm32_qspi_register_bus(struct stm32_qspi_bus *qspi_bus, const char *name)
{
    RT_ASSERT(qspi_bus != RT_NULL);
    RT_ASSERT(name != RT_NULL);

    _qspi_bus1.parent.user_data = qspi_bus;
    return rt_qspi_bus_register(&_qspi_bus1, name, &stm32_qspi_ops);
}

/**
  * @brief  This function attach device to QSPI bus.
  * @param  device_name      QSPI device name
  * @param  cs_pin           QSPI cs pin number
  * @param  data_line_width  QSPI data lines width, such as 1, 2, 4
  * @param  enter_qspi_mode  Callback function that lets FLASH enter QSPI mode
  * @param  exit_qspi_mode   Callback function that lets FLASH exit QSPI mode
  * @retval 0 : success
  *        -1 : failed
  */
rt_err_t rt_hw_qspi_device_attach(const char *bus_name, const char *device_name, rt_base_t cs_pin, rt_uint8_t data_line_width, void (*enter_qspi_mode)(), void (*exit_qspi_mode)())
{
    struct rt_qspi_device *qspi_device = RT_NULL;
    rt_err_t result = RT_EOK;

    RT_ASSERT(bus_name != RT_NULL);
    RT_ASSERT(device_name != RT_NULL);
    RT_ASSERT(data_line_width == 1 || data_line_width == 2 || data_line_width == 4);

    qspi_device = (struct rt_qspi_device *)rt_malloc(sizeof(struct rt_qspi_device));
    if (qspi_device == RT_NULL)
    {
        LOG_E("no memory, qspi bus attach device failed!");
        result = -RT_ENOMEM;
        goto __exit;
    }
    rt_memset(qspi_device, 0, sizeof(struct rt_qspi_device));
    qspi_device->enter_qspi_mode = enter_qspi_mode;
    qspi_device->exit_qspi_mode = exit_qspi_mode;
    qspi_device->config.qspi_dl_width = data_line_width;

#ifdef BSP_QSPI_USING_SOFTCS
    result = rt_spi_bus_attach_device_cspin(&qspi_device->parent, device_name, bus_name, cs_pin, RT_NULL);
#else
    result = rt_spi_bus_attach_device_cspin(&qspi_device->parent, device_name, bus_name, PIN_NONE, RT_NULL);
#endif /* BSP_QSPI_USING_SOFTCS */

__exit:
    if (result != RT_EOK)
    {
        if (qspi_device)
        {
            rt_free(qspi_device);
        }
    }

    return  result;
}

#ifdef BSP_QSPI_USING_DMA
void QSPI_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_QSPI_IRQHandler(&_stm32_qspi_bus.QSPI_Handler);

    /* leave interrupt */
    rt_interrupt_leave();
}

#if defined(SOC_SERIES_STM32H7)
void MDMA_IRQHandler(void)
{
    rt_interrupt_enter();
    if (_stm32_qspi_bus.hmdma_quadspi.Instance != RT_NULL)
        HAL_MDMA_IRQHandler(&_stm32_qspi_bus.hmdma_quadspi);
    rt_interrupt_leave();
}
#else
void QSPI_DMA_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&_stm32_qspi_bus.hdma_quadspi);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* SOC_SERIES_STM32H7 */
#endif /* BSP_QSPI_USING_DMA */

static int rt_hw_qspi_bus_init(void)
{
    return stm32_qspi_register_bus(&_stm32_qspi_bus, "qspi1");
}
INIT_BOARD_EXPORT(rt_hw_qspi_bus_init);

#endif /* BSP_USING_QSPI */
#endif /* RT_USING_QSPI */
