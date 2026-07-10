#include "upload_obj.h"

static rt_uint32_t event_mask = 0;

rt_uint32_t * upload_obj_init(void)
{
    return &event_mask;
}