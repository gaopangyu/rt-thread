#ifndef __PARAM_H__
#define __PARAM_H__

#include <rtthread.h>

#define PARAM_FILE_PATH     "/init/param_mcu.json"

rt_err_t param_set_from_file(char *path);
rt_err_t param_set_from_ui(const char *json_text);
rt_size_t param_get_json(const char *selector, char *out, rt_size_t out_sz);

#endif /* __PARAM_H__ */
