#include <fal.h>
#include <dfs_fs.h>
#include <unistd.h>
#include "upload_store.h"

#define DBG_TAG "store"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define RAM_STORE_DIR_PATH      "/ramfs/log/"  //超时写+半满写 0x3000 0000 + 0x0004 0000(256kB)
#define FLASH_STORE_DIR_PATH    "/filesystem/log/"

// master_index+lib_name/log_version/
// 1_tx3016a/logv1/20260101/fire1.log