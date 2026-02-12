#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rwsem.h>
#include "hashtable_module.h"
#include "proc_handlers.h"
#include "cmd_history.h"

static DECLARE_RWSEM(ht_sem);
static struct ht *table;

int init_module(void)
{
    int ret;
    table = create_ht();
    if (!table)
        return -ENOMEM;

    ret = proc_handlers_init(&ht_sem, table);
    if (ret) {
        destroy_ht(table);
        return ret;
    }

    printk(KERN_INFO "Hashtable proc module loaded\n");
    return 0;
}

void cleanup_module(void)
{
    proc_handlers_cleanup();
    cmd_history_cleanup();
    destroy_ht(table);
    printk(KERN_INFO "Hashtable proc module unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jack Edh");
MODULE_DESCRIPTION("Hashtable kernel module with per-process persistent lookup");
