#include "rtc.h"

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <rtdevice.h>

#define DBG_TAG "rtc"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

rt_err_t rtc_set_utc(time_t utc)
{
    rt_err_t ret;

    if (utc < 0)
    {
        LOG_E("invalid utc timestamp: %ld", (long)utc);
        return -RT_EINVAL;
    }

    ret = set_timestamp(utc);
    if (ret != RT_EOK)
    {
        LOG_E("set utc timestamp failed: %d", ret);
        return ret;
    }

    LOG_I("rtc_set_utc: %ld (%s)", (long)utc, ctime(&utc));
    return RT_EOK;
}

rt_err_t rtc_set_time(rt_uint32_t year, rt_uint32_t month, rt_uint32_t day,
                      rt_uint32_t hour, rt_uint32_t minute, rt_uint32_t second)
{
    struct tm tm_new = {0};
    time_t utc;

    if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || minute > 59 || second > 59)
    {
        LOG_E("invalid datetime: %u-%02u-%02u %02u:%02u:%02u",
              year, month, day, hour, minute, second);
        return -RT_EINVAL;
    }

    tm_new.tm_year = (int)year - 1900;
    tm_new.tm_mon  = (int)month - 1;
    tm_new.tm_mday = (int)day;
    tm_new.tm_hour = (int)hour;
    tm_new.tm_min  = (int)minute;
    tm_new.tm_sec  = (int)second;
    tm_new.tm_isdst = -1;

    /* 按本地时区解释日历时间，再写成 UTC 时间戳 */
    utc = mktime(&tm_new);
    if (utc == (time_t)-1)
    {
        LOG_E("mktime failed");
        return -RT_ERROR;
    }

    return rtc_set_utc(utc);
}

static int rtc_app_init(void)
{
    static rt_device_t rtc_dev = RT_NULL;
    rtc_dev = rt_device_find("rtc");
    if (rtc_dev == RT_NULL)
    {
        LOG_E("rtc device not found");
        return -RT_ERROR;
    }

    if (rt_device_open(rtc_dev, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        LOG_E("rtc device open failed");
        return -RT_ERROR;
    }

    time_t now = time(RT_NULL);
    if (now < 1767196800)
    {
        LOG_W("RTC time not set, applying default time");
        if (rtc_set_time(2026, 1, 1, 0, 0, 0) != RT_EOK)
        {
            return -RT_ERROR;
        }
        now = time(RT_NULL);
    }

    LOG_I("RTC time: %s", ctime(&now));
    LOG_I("RTC utc timestamp: %ld", (long)now);

    return RT_EOK;
}
INIT_ENV_EXPORT(rtc_app_init);

static int cmd_rtc_set_time(int argc, char **argv)
{
    rt_uint32_t year, month, day, hour, minute, second;

    if (argc != 7)
    {
        rt_kprintf("usage: rtc_set_time <year> <month> <day> <hour> <min> <sec>\n");
        rt_kprintf("e.g. : rtc_set_time 2026 7 17 14 30 0\n");
        return -RT_ERROR;
    }

    year   = (rt_uint32_t)atoi(argv[1]);
    month  = (rt_uint32_t)atoi(argv[2]);
    day    = (rt_uint32_t)atoi(argv[3]);
    hour   = (rt_uint32_t)atoi(argv[4]);
    minute = (rt_uint32_t)atoi(argv[5]);
    second = (rt_uint32_t)atoi(argv[6]);

    if (rtc_set_time(year, month, day, hour, minute, second) != RT_EOK)
    {
        return -RT_ERROR;
    }

    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_rtc_set_time, rtc_set_time, set RTC time: rtc_set_time y m d H M S);