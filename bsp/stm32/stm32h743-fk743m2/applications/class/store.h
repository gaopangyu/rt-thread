#ifndef __STORE_H__
#define __STORE_H__

#include <rtthread.h>
#include <fal.h>
#include <dfs_fs.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "upload_store.h"

#define RAMFS_ROOT              "/ramfs"
#define DISKFS_ROOT             "/diskfs"

int ensure_dir(const char *path);

void store_run(void);
void store_run_now(void);

/* 按 [start_time, end_time] 日期范围读取历史日志到 buffer，最多 count 个文件，返回写入字节数。
 * event_type: 0=全部, 1/'0'=fire, 2/'1'=fault, 其它=other
 */
rt_size_t store_get_history(upload_store_obj_t obj, rt_uint8_t *buffer, rt_size_t size,
                            time_t start_time, time_t end_time, rt_size_t count, rt_uint8_t event_type);

#endif /* __STORE_H__ */