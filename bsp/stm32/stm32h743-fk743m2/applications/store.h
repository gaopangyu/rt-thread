#ifndef __STORE_H__
#define __STORE_H__

#include <rtthread.h>
#include <fal.h>
#include <dfs_fs.h>
#include <unistd.h>
#include <sys/stat.h>

void store_run(void);
void store_run_now(void);

int ensure_dir(const char *path);

#endif /* __STORE_H__ */