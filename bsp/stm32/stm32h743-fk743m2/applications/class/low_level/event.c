#include "event.h"

char * event_get_file_name(enum event_file_type type)
{
    switch (type & 0x0F)
    {
    case FILE_TYPE_RESET:
        return "reset.event";
    case FILE_TYPE_FIRE:
        return "fire.event";
    case FILE_TYPE_MONITOR:
        return "monitor.event";
    case FILE_TYPE_FAULT:
        return "fault.event";
    case FILE_TYPE_ACTION:
        return "action.event";
    case FILE_TYPE_LINKAGE:
        return "linkage.event";
    case FILE_TYPE_MASK:
        return "mask.event";
    case FILE_TYPE_OTHER:
        return "other.event";
    case FILE_TYPE_CTRL:
        return "ctrl.event";
    default:
        return "other.event";
    }
}