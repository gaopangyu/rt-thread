#ifndef __STORE_H__
#define __STORE_H__

#include <rtthread.h>
#include <fal.h>
#include <dfs_fs.h>
#include <unistd.h>
#include <sys/stat.h>

#define RAMFS_ROOT              "/ramfs"
#define DISKFS_ROOT             "/diskfs"

int ensure_dir(const char *path);

void store_run(void);
void store_run_now(void);



#endif /* __STORE_H__ */