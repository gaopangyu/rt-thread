#ifndef __STORE_H__
#define __STORE_H__

#include <rtthread.h>
#include <fal.h>
#include <dfs_fs.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "upload_store.h"
#include "funnel_default.h"

#define RAMFS_ROOT              "/ramfs"
#define DISKFS_ROOT             "/diskfs"

int ensure_dir(const char *path);

void store_run(void);
void store_run_now(void);
void store_check_ramfs(void);

/* 按 event_history 读历史：{base}/{YYYYMMDD}/{type}.event，每条 event_num_pack，返回写入字节数。 */
rt_size_t store_get_history(upload_store_obj_t obj, rt_uint8_t *buffer, rt_size_t size, event_history_t event_history);

/* dir/path 同 upload_store_config。在 ramfs、diskfs 下找各类型最近日期目录的最后一条事件，out 填 number+1（无记录则为 0）。 */
rt_err_t store_get_event_last_num(const char *dir, const char *path, event_last_num_t out);

#endif /* __STORE_H__ */