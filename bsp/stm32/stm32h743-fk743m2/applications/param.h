#ifndef __PARAM_H__
#define __PARAM_H__

#include <rtthread.h>

#define PARAM_FILE_PATH     "/init/param_mcu.json"

rt_size_t param_get_from_file(char *path, const char *selector, char *out, rt_size_t out_sz);
rt_err_t param_set_all_from_file(char *path);
rt_size_t param_get_all_from_file(char *path, char *out, rt_size_t out_sz);

rt_err_t param_set_from_ui(const char *json_text);
rt_size_t param_get_to_ui(char *path, const char *selector, char *out, rt_size_t out_sz);
rt_err_t param_set_all_from_ui(const char *json_text);
rt_size_t param_get_all_to_ui(char *path, char *out, rt_size_t out_sz);

rt_err_t param_set_all_from_net(const char *json_text);
rt_size_t param_get_all_to_net(char *path, char *out, rt_size_t out_sz);

#endif /* __PARAM_H__ */
