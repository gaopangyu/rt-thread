/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "board.h"
#include "rtconfig.h"
#include <string.h>
#include <stdlib.h>
#include <rtdevice.h>
#include "usbd_core.h"

#include "usbd_cdc_acm.h"
#include "usbd_msc.h"
#include "console.h"

extern void usbd_cdc_acm_serial_init(uint8_t busid, uint8_t in_ep, uint8_t out_ep);
extern void usbd_cdc_acm_serial_detach(uint8_t busid);

#ifdef RT_USING_FINSH
#include <finsh.h>
#include <shell.h> /* finsh_get_device */
#endif

/*!< endpoint address */
#define CDC_IN_EP  0x81
#define CDC_OUT_EP 0x02
#define CDC_INT_EP 0x83

#define MSC_IN_EP  0x84
#define MSC_OUT_EP 0x05

#define USBD_VID           0xFFFF
#define USBD_PID           0xFFFF
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

#ifndef USB0_FINSH_CDC_NAME
#define USB0_FINSH_CDC_NAME "usb-acm0"
#endif

#define USB_CONFIG_SIZE_CDC_ACM_MSC (9 + CDC_ACM_DESCRIPTOR_LEN + MSC_DESCRIPTOR_LEN)
#define USB_CONFIG_SIZE_CDC_ACM (9 + CDC_ACM_DESCRIPTOR_LEN)

#ifdef CONFIG_USB_HS
#define CDC_MAX_MPS 512
#else
#define CDC_MAX_MPS 64
#endif

#ifdef CONFIG_USB_HS
#define MSC_MAX_MPS 512
#else
#define MSC_MAX_MPS 64
#endif

#ifdef CONFIG_USBDEV_ADVANCE_DESC
static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, 0x0100, 0x01)
};

static const uint8_t config_descriptor_cdc_acm_msc[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE_CDC_ACM_MSC, 0x03, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, CDC_MAX_MPS, 0x02),
    MSC_DESCRIPTOR_INIT(0x02, MSC_OUT_EP, MSC_IN_EP, MSC_MAX_MPS, 0x00)
};

static const uint8_t device_quality_descriptor_cdc_acm_msc[] = {
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
};

static const char *string_descriptors_cdc_acm_msc[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    "CherryUSB",                  /* Manufacturer */
    "CherryUSB CDC ACM MSC DEMO", /* Product */
    "2022123456",                 /* Serial Number */
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback_cdc_acm_msc(uint8_t speed)
{
    return config_descriptor_cdc_acm_msc;
}

static const uint8_t *device_quality_descriptor_callback_cdc_acm_msc(uint8_t speed)
{
    return device_quality_descriptor_cdc_acm_msc;
}

static const char *string_descriptor_callback_cdc_acm_msc(uint8_t speed, uint8_t index)
{
    if (index > 3) {
        return NULL;
    }
    return string_descriptors_cdc_acm_msc[index];
}

static const struct usb_descriptor descriptor_cdc_acm_msc = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback_cdc_acm_msc,
    .device_quality_descriptor_callback = device_quality_descriptor_callback_cdc_acm_msc,
    .string_descriptor_callback = string_descriptor_callback_cdc_acm_msc
};

static const uint8_t config_descriptor_cdc_acm[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE_CDC_ACM, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, CDC_MAX_MPS, 0x02)
};

static const uint8_t device_quality_descriptor_cdc_acm[] = {
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
};

static const char *string_descriptors_cdc_acm[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    "CherryUSB",                  /* Manufacturer */
    "CherryUSB CDC ACM DEMO",     /* Product */
    "2022123456",                 /* Serial Number */
};

static const uint8_t *config_descriptor_callback_cdc_acm(uint8_t speed)
{
    (void)speed;
    return config_descriptor_cdc_acm;
}

static const uint8_t *device_quality_descriptor_callback_cdc_acm(uint8_t speed)
{
    return device_quality_descriptor_cdc_acm;
}

static const char *string_descriptor_callback_cdc_acm(uint8_t speed, uint8_t index)
{
    if (index > 3) {
        return NULL;
    }
    return string_descriptors_cdc_acm[index];
}

static const struct usb_descriptor descriptor_cdc_acm = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback_cdc_acm,
    .device_quality_descriptor_callback = device_quality_descriptor_callback_cdc_acm,
    .string_descriptor_callback = string_descriptor_callback_cdc_acm
};
#else
    #error "CONFIG_USBDEV_ADVANCE_DESC is not defined"
#endif

static volatile uint8_t dtr_enable = 0;

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    (void)busid;

    switch (event)
    {
    case USBD_EVENT_RESET:
    case USBD_EVENT_DISCONNECTED:
    case USBD_EVENT_SUSPEND:
        dtr_enable = 0;
        console_usb_state_notify(RT_FALSE);
        break;
    default:
        break;
    }
}

static struct usbd_interface intf2;

static void usb0_cdc_acm_msc_reattach(uint8_t busid, uintptr_t reg_base)
{
    rt_bool_t rebind_cons_usb = RT_FALSE;
    rt_bool_t rebind_finsh     = RT_FALSE;
#if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
    {
        rt_device_t c = rt_console_get_device();
        if (c != RT_NULL && strcmp(c->parent.name, USB0_FINSH_CDC_NAME) == 0) {
            rebind_cons_usb = RT_TRUE;
        }
    }
#endif
#ifdef RT_USING_FINSH
    {
        const char *n = finsh_get_device();
        if (n != RT_NULL && strcmp(n, USB0_FINSH_CDC_NAME) == 0) {
            rebind_finsh = RT_TRUE;
        }
    }
#endif

    if (rebind_cons_usb || rebind_finsh) {
        const char *fb = RT_CONSOLE_DEVICE_NAME;

        if (strcmp(fb, USB0_FINSH_CDC_NAME) != 0) {
#if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
            if (rebind_cons_usb) {
                rt_console_set_device(fb);
            }
#endif
#ifdef RT_USING_FINSH
            if (rebind_finsh) {
                finsh_set_device(fb);
            }
#endif
        }
    }

    /* 先停控制器与类回调，再释放 usbd_serial；否则 IRQ 里 bulk_in/bulk_out 仍可能
     * 在 memset 后访问 g_usbd_serial_cdc_acm[] 或已删的 sem。 */
    usbd_deinitialize(busid);
    usbd_cdc_acm_serial_detach(busid);
    usbd_desc_register(busid, &descriptor_cdc_acm_msc);
    usbd_cdc_acm_serial_init(busid, CDC_IN_EP, CDC_OUT_EP);
    usbd_add_interface(busid, usbd_msc_init_intf(busid, &intf2, MSC_OUT_EP, MSC_IN_EP));
    usbd_initialize(busid, reg_base, usbd_event_handler);

    if (rebind_cons_usb || rebind_finsh) {
        int w;
        for (w = 0; w < 500; w++) {
            if (usb_device_is_configured(busid)) {
                break;
            }
            rt_thread_mdelay(20);
        }
        if (usb_device_is_configured(busid)) {
            rt_thread_mdelay(500);
#if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
            rt_console_set_device(USB0_FINSH_CDC_NAME);
#endif
#ifdef RT_USING_FINSH
            if (rebind_finsh) {
                finsh_set_device(USB0_FINSH_CDC_NAME);
            }
#endif
            rt_thread_mdelay(500);
        }
    }

}

int usb0_cdc_acm_init(void)
{
    usbd_desc_register(0, &descriptor_cdc_acm);
    usbd_cdc_acm_serial_init(0, CDC_IN_EP, CDC_OUT_EP);
    usbd_initialize(0, USB_OTG_FS_PERIPH_BASE, usbd_event_handler);
    return 0;
}
INIT_PREV_EXPORT(usb0_cdc_acm_init);

#ifdef RT_USING_FINSH
static char mount_mtd_partition[RT_NAME_MAX] = "init";
static int cmd_usb0_cdc_acm_msc(int argc, char **argv)
{
    rt_size_t len = rt_strnlen(argv[1], RT_NAME_MAX - 1);
    if (argc > 1 && len > 0) {
        strncpy(mount_mtd_partition, argv[1], RT_NAME_MAX - 1);
        mount_mtd_partition[len] = 0;
    }
    usb0_cdc_acm_msc_reattach(0, USB_OTG_FS_PERIPH_BASE);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_usb0_cdc_acm_msc, usb0_cdc_acm_msc, Re-enumerate USB as CDC+ACM (Serial) + MSC (U disk));
#endif


/* CDC bulk 由 usbd_serial.c 中 usbd_cdc_acm_serial_init 注册。 */
void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr)
{
    (void)busid;
    (void)intf;

    dtr_enable = dtr ? 1 : 0;
    /* 主机打开 COM 才切 USB console；关 COM / 拔线 DTR=0 切回上一个设备 */
    console_usb_state_notify(dtr ? RT_TRUE : RT_FALSE);
}

#if 1

#include <drivers/mtd_nor.h>

static struct rt_mtd_nor_device *mtd = RT_NULL;

static rt_uint32_t msc_mtd_sector_count(struct rt_mtd_nor_device *dev)
{
    return dev->block_end - dev->block_start;
}

void usbd_msc_get_cap(uint8_t busid, uint8_t lun, uint32_t *block_num, uint32_t *block_size)
{
    (void)busid;
    (void)lun;

    rt_device_t dev = rt_device_find(mount_mtd_partition);
    if (dev != RT_NULL && dev->type == RT_Device_Class_MTD)
    {
        mtd = RT_MTD_NOR_DEVICE(dev);
    }

    if (mtd != RT_NULL && mtd->block_size != 0)
    {
        *block_size = mtd->block_size;
        *block_num = msc_mtd_sector_count(mtd);
    }
    else
    {
        *block_num = 0;
        *block_size = 512;
    }
}

int usbd_msc_sector_read(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    rt_off_t offset;
    rt_uint32_t sector_cnt;

    (void)busid;
    (void)lun;

    if (mtd == RT_NULL || buffer == RT_NULL || length == 0)
    {
        return -1;
    }

    if (length % mtd->block_size != 0)
    {
        return -1;
    }

    sector_cnt = length / mtd->block_size;
    if (sector + sector_cnt > msc_mtd_sector_count(mtd))
    {
        return -1;
    }

    offset = (rt_off_t)sector * mtd->block_size;
    if (rt_mtd_nor_read(mtd, offset, buffer, length) != (rt_ssize_t)length)
    {
        return -1;
    }

    return 0;
}

int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    rt_off_t offset;
    rt_uint32_t sector_cnt;

    (void)busid;
    (void)lun;

    if (mtd == RT_NULL || buffer == RT_NULL || length == 0)
    {
        return -1;
    }

    if (length % mtd->block_size != 0)
    {
        return -1;
    }

    sector_cnt = length / mtd->block_size;
    if (sector + sector_cnt > msc_mtd_sector_count(mtd))
    {
        return -1;
    }

    offset = (rt_off_t)sector * mtd->block_size;
    if (rt_mtd_nor_erase_block(mtd, offset, length) != RT_EOK)
    {
        return -1;
    }
    if (rt_mtd_nor_write(mtd, offset, buffer, length) != (rt_ssize_t)length)
    {
        return -1;
    }

    return 0;
}

#else

#define BLOCK_SIZE  4096
#define BLOCK_COUNT 40

typedef struct
{
    uint8_t BlockSpace[BLOCK_SIZE];
} BLOCK_TYPE;

static BLOCK_TYPE mass_block[BLOCK_COUNT];

void usbd_msc_get_cap(uint8_t busid, uint8_t lun, uint32_t *block_num, uint32_t *block_size)
{
    *block_num = BLOCK_COUNT; //Pretend having so many buffer,not has actually.
    *block_size = BLOCK_SIZE;
}
int usbd_msc_sector_read(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    if (sector < BLOCK_COUNT)
        memcpy(buffer, mass_block[sector].BlockSpace, length);
    rt_kprintf("MSC read: sector %lu, length %lu\n", (unsigned long)sector, (unsigned long)length);
    return 0;
}

int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    if (sector < BLOCK_COUNT)
        memcpy(mass_block[sector].BlockSpace, buffer, length);
    rt_kprintf("MSC write: sector %lu, length %lu\n", (unsigned long)sector, (unsigned long)length);
    return 0;
}

#ifdef RT_USING_FINSH

#ifndef READ_BLOCK_HEX_WIDTH
#define READ_BLOCK_HEX_WIDTH 16
#endif

#ifndef __is_print
#define __is_print(ch) ((unsigned int)((ch) - ' ') < 127u - ' ')
#endif

static int read_block(int argc, char **argv)
{
    rt_uint32_t sector;
    rt_uint32_t nbytes;
    rt_uint32_t i;
    rt_uint32_t j;
    const rt_uint8_t *data;

    if (argc < 2) {
        rt_kprintf("Usage: read_block <sector> [bytes]\n");
        rt_kprintf("  sector: eventical sector index, 0 .. %u\n", (unsigned)(BLOCK_COUNT - 1));
        rt_kprintf("  bytes: optional dump length from sector start, default 256, max %u\n",
                   (unsigned)BLOCK_SIZE);
        return 0;
    }

    sector = (rt_uint32_t)strtoul(argv[1], RT_NULL, 0);
    if (sector >= BLOCK_COUNT) {
        rt_kprintf("read_block: sector %lu out of range (max %u)\n",
                   (unsigned long)sector, (unsigned)(BLOCK_COUNT - 1));
        return -RT_EINVAL;
    }

    if (argc >= 3) {
        nbytes = (rt_uint32_t)strtoul(argv[2], RT_NULL, 0);
        if (nbytes == 0 || nbytes > BLOCK_SIZE) {
            rt_kprintf("read_block: bytes must be 1 .. %u\n", (unsigned)BLOCK_SIZE);
            return -RT_EINVAL;
        }
    } else {
        nbytes = 256;
    }

    data = mass_block[sector].BlockSpace;
    rt_kprintf("MSC RAM disk sector %lu, offset in sector 0, length %lu:\n",
               (unsigned long)sector, (unsigned long)nbytes);
    rt_kprintf("Offset (h) 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");

    for (i = 0; i < nbytes; i += READ_BLOCK_HEX_WIDTH) {
        rt_kprintf("[%08lX] ", (unsigned long)i);
        for (j = 0; j < READ_BLOCK_HEX_WIDTH; j++) {
            if (i + j < nbytes) {
                rt_kprintf("%02X ", data[i + j]);
            } else {
                rt_kprintf("   ");
            }
        }
        for (j = 0; j < READ_BLOCK_HEX_WIDTH; j++) {
            if (i + j < nbytes) {
                rt_kprintf("%c", __is_print(data[i + j]) ? data[i + j] : '.');
            }
        }
        rt_kprintf("\n");
    }
    rt_kprintf("\n");

    return 0;
}

MSH_CMD_EXPORT(read_block, Dump MSC RAM-disk sector (read_block sector [bytes]));

static int write_block(int argc, char **argv)
{
    rt_uint32_t sector;
    rt_uint32_t byte_val;
    rt_uint32_t nbytes;

    if (argc < 3) {
        rt_kprintf("Usage: write_block <sector> <byte> [bytes]\n");
        rt_kprintf("  Fill MSC RAM-disk sector from offset 0 with repeated byte.\n");
        rt_kprintf("  sector: 0 .. %u\n", (unsigned)(BLOCK_COUNT - 1));
        rt_kprintf("  byte:   0..255 (decimal or 0x hex)\n");
        rt_kprintf("  bytes:  optional fill length, default %u (full sector), max %u\n",
                   (unsigned)BLOCK_SIZE, (unsigned)BLOCK_SIZE);
        return 0;
    }

    sector = (rt_uint32_t)strtoul(argv[1], RT_NULL, 0);
    if (sector >= BLOCK_COUNT) {
        rt_kprintf("write_block: sector %lu out of range (max %u)\n",
                   (unsigned long)sector, (unsigned)(BLOCK_COUNT - 1));
        return -RT_EINVAL;
    }

    byte_val = (rt_uint32_t)strtoul(argv[2], RT_NULL, 0);
    if (byte_val > 0xFFu) {
        rt_kprintf("write_block: byte must be 0..255\n");
        return -RT_EINVAL;
    }

    if (argc >= 4) {
        nbytes = (rt_uint32_t)strtoul(argv[3], RT_NULL, 0);
        if (nbytes == 0 || nbytes > BLOCK_SIZE) {
            rt_kprintf("write_block: bytes must be 1 .. %u\n", (unsigned)BLOCK_SIZE);
            return -RT_EINVAL;
        }
    } else {
        nbytes = BLOCK_SIZE;
    }

    memset(mass_block[sector].BlockSpace, (int)byte_val, nbytes);
    rt_kprintf("write_block: sector %lu, filled %lu bytes with 0x%02lX\n",
               (unsigned long)sector, (unsigned long)nbytes,
               (unsigned long)(byte_val & 0xFFu));

    return 0;
}

MSH_CMD_EXPORT(write_block, Fill MSC RAM-disk sector (write_block sector byte [bytes]));
#endif /* RT_USING_FINSH */
#endif /* 1 */