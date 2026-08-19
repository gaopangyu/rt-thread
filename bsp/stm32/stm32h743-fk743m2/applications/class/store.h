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

/* 按事件编号或日期偏移读取 {YYYYMMDD}.event，每条事件 32 字节，返回写入字节数。 */
rt_size_t store_get_history(upload_store_obj_t obj, rt_uint8_t *buffer, rt_size_t size, event_history_t event_history);

#endif /* __STORE_H__ */