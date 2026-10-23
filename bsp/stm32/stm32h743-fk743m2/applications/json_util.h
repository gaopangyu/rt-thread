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
void json_copy_keys(cJSON *dst, cJSON *src, const char * const *keys, rt_size_t n);
cJSON *json_pick_object(cJSON *src, const char * const *keys, rt_size_t n);
rt_size_t json_print(cJSON *obj, char *out, rt_size_t out_sz);
cJSON *json_wrap(const char *key, cJSON *child);
rt_bool_t json_is_off(cJSON *item);
void json_drop_empty(cJSON *obj, const char *key);
cJSON *json_from_text(cJSON *old_item, const char *s);

#endif /* __JSON_UTIL_H__ */
