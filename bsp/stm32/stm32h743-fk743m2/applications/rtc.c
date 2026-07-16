#include "rtc.h"

#include <sys/time.h>
#include <time.h>
#include <rtdevice.h>

#define DBG_TAG "rtc"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define RTC_TIME_VALID_EPOCH 946684800 /* 2000-01-01 00:00:00 UTC */

static rt_device_t rtc_dev = RT_NULL;

static rt_err_t rtc_set_default_time(void)
{
    struct timeval tv = {0};

    tv.tv_sec = RTC_TIME_VALID_EPOCH;
    tv.tv_usec = 0;
    if (settimeofday(&tv, RT_NULL) != 0)
    {
        LOG_W("set default rtc time failed");
        return -RT_ERROR;
    }

    LOG_I("rtc default time set to 2000-01-01 08:00:00 (UTC+8)");
    return RT_EOK;
}

static int rtc_app_init(void)
{
    struct timeval tv;
    struct tm tm_now;
    time_t now;

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

    if (gettimeofday(&tv, RT_NULL) != 0)
    {
        LOG_W("rtc get time failed");
        return -RT_ERROR;
    }

    now = (time_t)tv.tv_sec;
    if (now < RTC_TIME_VALID_EPOCH)
    {
        LOG_W("rtc time not set, applying default time");
        return rtc_set_default_time();
    }

    localtime_r(&now, &tm_now);
    LOG_I("rtc ready: %04d-%02d-%02d %02d:%02d:%02d",
          tm_now.tm_year + 1900,
          tm_now.tm_mon + 1,
          tm_now.tm_mday,
          tm_now.tm_hour,
          tm_now.tm_min,
          tm_now.tm_sec);

    return RT_EOK;
}
INIT_APP_EXPORT(rtc_app_init);
