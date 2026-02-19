#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>

#include "kvstore_net.h"
#include "hashtable_module.h"
#include "kvstore_commands.h"
#include "daemon_module.h"
#include "kvstore.h"


static struct proc_dir_entry *proc_ht;
static struct proc_dir_entry *proc_hashtable;
static struct proc_dir_entry *proc_daemonpid;

//static pid_t daemon_pid = -1;

DECLARE_RWSEM(ht_sem);
LIST_HEAD(cmd_history);

ht *table;

static const struct proc_ops ht_proc_ops = {
    .proc_read  = ht_read,
    .proc_write = ht_write,
};

static const struct proc_ops hashtable_proc_ops = {
    .proc_read  = daemon_ht_read,
};

static const struct proc_ops daemonpid_proc_ops = {
    .proc_read  = daemonpid_read,
    .proc_write = daemonpid_write,
};

int init_module(void)
{
    table = create_ht();
    if (!table)
        return -ENOMEM;

    proc_ht = proc_create("ht", 0666, NULL, &ht_proc_ops);
    proc_hashtable = proc_create("hashtable", 0444, NULL, &hashtable_proc_ops);
    proc_daemonpid = proc_create("daemonpid", 0666, NULL, &daemonpid_proc_ops);

    if (!proc_ht || !proc_hashtable || !proc_daemonpid) {
        destroy_ht(table);
        return -ENOMEM;
    }

    // Register Netfilter UDP hook (moved to net_kvstore.c)
    net_kvstore_init(&ht_sem, table);

    printk(KERN_INFO "Hashtable proc module loaded with Netfilter UDP hook\n");
    return 0;
}

void cleanup_module(void)
{
    struct cmd_entry *entry, *tmp;


    // Unregister Netfilter UDP hook (moved to net_kvstore.c)
    net_kvstore_exit();

    proc_remove(proc_ht);
    proc_remove(proc_hashtable);
    proc_remove(proc_daemonpid);

    down_write(&ht_sem);
    list_for_each_entry_safe(entry, tmp, &cmd_history, list) {
        list_del(&entry->list);
        kfree(entry);
    }
    destroy_ht(table);
    up_write(&ht_sem);
    printk(KERN_INFO "Hashtable proc module unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jack Edh");
MODULE_DESCRIPTION("Hashtable kernel module with per-process persistent lookup");
