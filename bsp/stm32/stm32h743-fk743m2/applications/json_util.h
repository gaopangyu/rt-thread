#ifndef __JSON_UTIL_H__
#define __JSON_UTIL_H__

#include <rtthread.h>
#include <cJSON.h>

rt_bool_t json_get_u32(cJSON *obj, const char *key, rt_uint32_t *out);
rt_bool_t json_is_enabled(cJSON *obj, const char *key, rt_bool_t def);
void json_copy_str(char *dst, rt_size_t dst_sz, cJSON *obj, const char *key);
void json_set_item(cJSON *obj, const char *key, cJSON *value);
cJSON *json_ensure_object(cJSON *obj, const char *key);
void json_set_string(cJSON *obj, const char *key, const char *value);
void json_copy_key(cJSON *dst, cJSON *src, const char *key);
void json_merge_object(cJSON *dst, cJSON *src);
void json_merge_nested(cJSON *dst, cJSON *src, const char *key);
cJSON *json_array_find_by_id(cJSON *arr, rt_uint32_t id);

#endif /* __JSON_UTIL_H__ */
