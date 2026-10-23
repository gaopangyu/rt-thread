#ifndef __MSH_UTIL_H__
#define __MSH_UTIL_H__

#include <rtthread.h>

const char *msh_get_opt(int argc, char *argv[], const char *opt);
rt_bool_t msh_has_opt(int argc, char *argv[], const char *opt);
const char *msh_get_pos(int argc, char *argv[], int index, const char *flag_only_opts);
rt_err_t msh_check_create_name(const char *name, const char *type_prefix);
rt_err_t msh_check_delete_name(const char *name);
int msh_parse_u32(const char *s, rt_uint32_t *out);

#endif /* __MSH_UTIL_H__ */
