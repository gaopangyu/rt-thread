#include "interface_obj.h"

static rt_bool_t first_in = RT_TRUE;
static rt_uint32_t event_mask = 0;
static custom_obj_t obj_list[CUSTOM_PER_LAYER_OBJ_MAX] = {0};
static struct rt_event rx_pub_event;
static struct rt_event tx_pub_event;


void interface_obj_init(custom_obj_t custom_obj)
{
    rt_enter_critical();
    if(first_in)
    {
        rt_event_init(&rx_pub_event, "interface_rx_pub_event", RT_IPC_FLAG_PRIO);
        rt_event_init(&tx_pub_event, "interface_tx_pub_event", RT_IPC_FLAG_PRIO);
        first_in = RT_FALSE;
    }
    rt_exit_critical();

    custom_obj->event_mask = &event_mask;
    custom_obj->obj_list = obj_list;
    custom_obj->rxb.pub_event = &rx_pub_event;
    custom_obj->txb.pub_event = &tx_pub_event;
}