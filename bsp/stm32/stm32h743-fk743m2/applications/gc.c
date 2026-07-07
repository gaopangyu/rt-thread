#include "gc.h"

#define DBG_TAG "gc"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static struct rt_semaphore gc_run_sem;

typedef struct
{
    rt_list_t *list;
    rt_list_t **array;
    rt_uint8_t type;
    int nr;             /* input: max nr, can't be 0 */
    int nr_out;         /* out: got nr */
} list_get_next_t;

static void list_find_init(list_get_next_t *p, rt_uint8_t type, rt_list_t **array, int nr)
{
    struct rt_object_information *info;
    rt_list_t *list;

    info = rt_object_get_information((enum rt_object_class_type)type);
    list = &info->object_list;

    p->list = list;
    p->type = type;
    p->array = array;
    p->nr = nr;
    p->nr_out = 0;
}

static rt_list_t *list_get_next(rt_list_t *current, list_get_next_t *arg)
{
    int first_flag = 0;
    rt_base_t level;
    rt_list_t *node, *list;
    rt_list_t **array;
    struct rt_object_information *info;
    int nr;

    arg->nr_out = 0;

    if (!arg->nr || !arg->type)
    {
        return (rt_list_t *)RT_NULL;
    }

    list = arg->list;
    info = rt_list_entry(list, struct rt_object_information, object_list);

    if (!current) /* find first */
    {
        node = list;
        first_flag = 1;
    }
    else
    {
        node = current;
    }

    level = rt_spin_lock_irqsave(&info->spinlock);

    if (!first_flag)
    {
        struct rt_object *obj;
        /* The node in the list? */
        obj = rt_list_entry(node, struct rt_object, list);
        if ((obj->type & ~RT_Object_Class_Static) != arg->type)
        {
            rt_spin_unlock_irqrestore(&info->spinlock, level);
            return (rt_list_t *)RT_NULL;
        }
    }

    nr = 0;
    array = arg->array;
    while (1)
    {
        node = node->next;

        if (node == list)
        {
            node = (rt_list_t *)RT_NULL;
            break;
        }
        nr++;
        *array++ = node;
        if (nr == arg->nr)
        {
            break;
        }
    }

    rt_spin_unlock_irqrestore(&info->spinlock, level);
    arg->nr_out = nr;
    return node;
}

static void gc_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        if (rt_sem_take(&gc_run_sem, 5000) != RT_EOK)
            continue;

        if (rt_object_get_information(RT_Object_Class_Custom) == RT_NULL)
            continue;

        #define LIST_FIND_OBJ_NR 8
        rt_base_t level;
        list_get_next_t find_arg;
        struct rt_object_information *info;
        rt_list_t *obj_list[LIST_FIND_OBJ_NR];
        rt_list_t *next = (rt_list_t *)RT_NULL;
        int maxlen = RT_NAME_MAX;

        list_find_init(&find_arg, RT_Object_Class_Custom, obj_list, sizeof(obj_list) / sizeof(obj_list[0]));
        info = rt_list_entry(find_arg.list, struct rt_object_information, object_list);

        do
        {
            next = list_get_next(next, &find_arg);
            for (int i = 0; i < find_arg.nr_out; i++)
            {
                struct rt_object *obj;

                obj = rt_list_entry(obj_list[i], struct rt_object, list);
                level = rt_spin_lock_irqsave(&info->spinlock);
                if ((obj->type & ~RT_Object_Class_Static) != find_arg.type)
                {
                    rt_spin_unlock_irqrestore(&info->spinlock, level);
                    continue;
                }
                rt_spin_unlock_irqrestore(&info->spinlock, level);

                // LOG_D("GC: check object: %s", obj->name);

                custom_obj_t custom_obj = (custom_obj_t)obj;

                if (rt_atomic_load(&custom_obj->gc_flag) != 1 || rt_atomic_load(&custom_obj->gc_take) != 0)
                    continue;

                custom_obj->ops->gc(custom_obj);
            }
        }while (next != (rt_list_t *)RT_NULL);
    }
}

static int gc_init(void)
{
    RT_ASSERT(rt_sem_init(&gc_run_sem, "gc_run", 0, RT_IPC_FLAG_FIFO) == RT_EOK);

    rt_thread_t tid = rt_thread_create("gc_thread",
                                      gc_entry,
                                      RT_NULL,
                                      1024,
                                      RT_THREAD_PRIORITY_MAX - 2,
                                      20);
    if (tid == RT_NULL)
    {
        LOG_E("GC: thread create error");
        return -RT_ERROR;
    }
    rt_thread_startup(tid);

    return RT_EOK;
}
INIT_APP_EXPORT(gc_init);

void gc_run(custom_obj_t obj)
{
    if (obj != RT_NULL)
    {
        rt_atomic_store(&obj->gc_flag, 1);
    }

    rt_sem_release(&gc_run_sem);
}

static int cmd_gc_sweep(int argc, char *argv[])
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);

    gc_run(RT_NULL);
    rt_kprintf("GC sweep scheduled\n");
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(cmd_gc_sweep, gc_sweep, Run GC sweep for deferred deletes);