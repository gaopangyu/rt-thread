#ifndef __PARAM_H__
#define __PARAM_H__

#include <rtthread.h>

#define PARAM_JSON_PATH     "/init/param_mcu.json"

rt_err_t param_set_from_mcu_json(void);
rt_err_t param_set_from_ui(const char *json_text);

#endif /* __PARAM_H__ */
