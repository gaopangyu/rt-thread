#ifndef __RTC_H__
#define __RTC_H__

#include <rtthread.h>
#include <time.h>

rt_err_t rtc_set_utc(time_t utc);
rt_err_t rtc_set_time(rt_uint32_t year, rt_uint32_t month, rt_uint32_t day,
                      rt_uint32_t hour, rt_uint32_t minute, rt_uint32_t second);

#endif /* __RTC_H__ */
