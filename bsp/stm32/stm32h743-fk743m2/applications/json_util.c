#include "json_util.h"

rt_bool_t json_get_u32(cJSON *obj, const char *key, rt_uint32_t *out)
{
    cJSON *item;

    if (obj == RT_NULL || key == RT_NULL || out == RT_NULL)
        return RT_FALSE;

    item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(item))
        return RT_FALSE;

    *out = (rt_uint32_t)item->valuedouble;
    return RT_TRUE;
}

rt_bool_t json_is_enabled(cJSON *obj, const char *key, rt_bool_t def)
{
    cJSON *item;

    if (obj == RT_NULL || key == RT_NULL)
        return def;

    item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == RT_NULL)
        return def;
    if (cJSON_IsTrue(item))
        return RT_TRUE;
    if (cJSON_IsFalse(item))
        return RT_FALSE;
    if (cJSON_IsNumber(item))
        return item->valuedouble != 0;

    return def;
}

void json_copy_str(char *dst, rt_size_t dst_sz, cJSON *obj, const char *key)
{
    cJSON *item;

    if (dst == RT_NULL || dst_sz == 0 || obj == RT_NULL || key == RT_NULL)
        return;

    item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsString(item) || item->valuestring == RT_NULL)
        return;

    rt_strncpy(dst, item->valuestring, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
}

void json_set_item(cJSON *obj, const char *key, cJSON *value)
{
    if (obj == RT_NULL || key == RT_NULL || value == RT_NULL)
        return;

    cJSON_DeleteItemFromObject(obj, key);
    cJSON_AddItemToObject(obj, key, value);
}

cJSON *json_ensure_object(cJSON *obj, const char *key)
{
    cJSON *nested;

    if (obj == RT_NULL || key == RT_NULL)
        return RT_NULL;

    nested = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsObject(nested))
        return nested;

    nested = cJSON_CreateObject();
    json_set_item(obj, key, nested);
    return nested;
}

void json_set_string(cJSON *obj, const char *key, const char *value)
{
    if (obj == RT_NULL || key == RT_NULL || value == RT_NULL)
        return;

    json_set_item(obj, key, cJSON_CreateString(value));
}

void json_copy_key(cJSON *dst, cJSON *src, const char *key)
{
    cJSON *item;

    if (dst == RT_NULL || src == RT_NULL || key == RT_NULL)
        return;

    item = cJSON_GetObjectItemCaseSensitive(src, key);
    if (item == RT_NULL)
        return;

    json_set_item(dst, key, cJSON_Duplicate(item, 1));
}

void json_merge_object(cJSON *dst, cJSON *src)
{
    cJSON *child;

    if (!cJSON_IsObject(dst) || !cJSON_IsObject(src))
        return;

    for (child = src->child; child != RT_NULL; child = child->next)
    {
        if (child->string == RT_NULL)
            continue;
        json_set_item(dst, child->string, cJSON_Duplicate(child, 1));
    }
}

void json_merge_nested(cJSON *dst, cJSON *src, const char *key)
{
    cJSON *src_obj;
    cJSON *dst_obj;

    src_obj = cJSON_GetObjectItemCaseSensitive(src, key);
    if (!cJSON_IsObject(src_obj))
        return;

    dst_obj = cJSON_GetObjectItemCaseSensitive(dst, key);
    if (!cJSON_IsObject(dst_obj))
    {
        json_set_item(dst, key, cJSON_Duplicate(src_obj, 1));
        return;
    }

    json_merge_object(dst_obj, src_obj);
}

cJSON *json_array_find_by_id(cJSON *arr, rt_uint32_t id)
{
    int i, count;

    if (!cJSON_IsArray(arr))
        return RT_NULL;

    count = cJSON_GetArraySize(arr);
    for (i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        rt_uint32_t v;

        if (cJSON_IsObject(item) && json_get_u32(item, "id", &v) && v == id)
            return item;
    }

    return RT_NULL;
}
