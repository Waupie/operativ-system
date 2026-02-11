#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/rwsem.h>
#include <linux/wait.h>
#include <linux/sched/signal.h>
#include "hashtable_module.h"

#define PROC_BUF_SIZE PAGE_SIZE


static DECLARE_RWSEM(ht_sem);
static struct proc_dir_entry *proc_htfile;
static struct proc_dir_entry *proc_pidfile; 
static ht *table;
static atomic_t daemon_flag = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(ht_wq);

struct cmd_entry {
    char text[PROC_BUF_SIZE];
    struct list_head list;
};
static LIST_HEAD(cmd_history);
static LIST_HEAD(current_hashtable);
static pid_t user_daemon_pid = 0;


static ssize_t my_read(struct file *file,
                       char __user *user_buffer,
                       size_t count,
                       loff_t *offs)
{
    struct cmd_entry *entry;
    char buf[PROC_BUF_SIZE];
    int len = 0;

    if (*offs > 0)
        return 0;  // EOF

    memset(buf, 0, PROC_BUF_SIZE);

    down_read(&ht_sem);
    for (int i = 0; i < table->capacity; i++) {
        ht_entry* e = table->entries[i];
        while (e) {
            len += scnprintf(buf + len, PROC_BUF_SIZE - len,
                             "key=%s value=%s\n",
                              e->key, e->value);
            e = e->next;

            if (len >= PROC_BUF_SIZE - 1)
                goto out;
        }
    }
    up_read(&ht_sem);

    if (copy_to_user(user_buffer, buf, len))
        return -EFAULT;

    *offs = len;
    return len;
}


static ssize_t ht_write(struct file *file,
                        const char __user *user_buffer,
                        size_t count,
                        loff_t *offs)
{
    char buf[512];
    char cmd[16], key[128], value[128];
    int ret = 0;
    char output[PROC_BUF_SIZE];

    memset(buf, 0, sizeof(buf));
    memset(output, 0, sizeof(output));

    if (count >= sizeof(buf))
        return -EINVAL;

    if (copy_from_user(buf, user_buffer, count))
        return -EFAULT;

    buf[count] = '\0';
    buf[strcspn(buf, "\n")] = 0;

    if (sscanf(buf, "%15s %127s %127s", cmd, key, value) < 1)
        return count;

    if (!strcmp(cmd, "insert")) {
        down_write(&ht_sem);
        ret = ht_insert(table, key, value);
        up_write(&ht_sem);
        snprintf(output, PROC_BUF_SIZE, ret ? "Insert failed" : "Inserted key-value %s - %s", key, value);

    } else if (!strcmp(cmd, "delete")) {
        down_write(&ht_sem);
        ret = ht_delete(table, key);
        up_write(&ht_sem);

        snprintf(output, PROC_BUF_SIZE, ret ? "Delete failed" : "Deleted key %s", key);

    } else if (!strcmp(cmd, "lookup")) {
        const char *res;
        down_read(&ht_sem);
        res = ht_search(table, key);
        if (res)
            snprintf(output, PROC_BUF_SIZE, "Lookup on key %s gave %s", key, res);
        else
            snprintf(output, PROC_BUF_SIZE, "Not found");
        up_read(&ht_sem);

    } else {
        snprintf(output, PROC_BUF_SIZE, "Unknown command");
    }

    {
        struct cmd_entry *entry = kmalloc(sizeof(*entry), GFP_KERNEL);
        if (entry) {
            strncpy(entry->text, output, PROC_BUF_SIZE - 1);
            entry->text[PROC_BUF_SIZE - 1] = '\0';

            down_write(&ht_sem);
            list_add_tail(&entry->list, &cmd_history);
            up_write(&ht_sem);
        }
    }

    return count;
}


static struct proc_ops ht_proc_ops = {
    .proc_read = ht_read,
    .proc_write = ht_write,
};

static ssize_t pid_read(struct file *file, char __user *user_buffer,
                        size_t count, loff_t *offs)
{
    char buf[32];
    int len;

    if (*offs > 0)
        return 0;

    len = snprintf(buf, sizeof(buf), "%d\n", user_daemon_pid);
    if (copy_to_user(user_buffer, buf, len))
        return -EFAULT;

    *offs = len;
    return len;
}

static ssize_t pid_write(struct file *file, const char __user *user_buffer,
                         size_t count, loff_t *offs)
{
    char buf[32];
    pid_t pid;

    if (count >= sizeof(buf))
        return -EINVAL;

    if (copy_from_user(buf, user_buffer, count))
        return -EFAULT;

    buf[count] = '\0';
    pid = simple_strtoul(buf, NULL, 10);
    user_daemon_pid = pid;

    return count;
}

static const struct proc_ops pid_proc_ops = {
    .proc_read = pid_read,
    .proc_write = pid_write,
};


int init_module(void)
{
    table = create_ht();
    if (!table)
        return -ENOMEM;

    deamon_func();

    proc_htfile = proc_create("ht", 0666, NULL, &ht_proc_ops);
    if (!proc_htfile) {
        destroy_ht(table);
        return -ENOMEM;
    }
    proc_pidfile = proc_create("daemonpid", 0666, NULL, &pid_proc_ops);
    if (!proc_pidfile) {
        destroy_ht(table);
        return -ENOMEM;
    }

    printk(KERN_INFO "Hashtable proc created at /proc/ht\n");
    return 0;
}

void cleanup_module(void)
{
    struct cmd_entry *entry, *tmp;

    proc_remove(proc_htfile);

    down_write(&ht_sem);
    list_for_each_entry_safe(entry, tmp, &cmd_history, list) {
        list_del(&entry->list);
        kfree(entry);
    }

    destroy_ht(table);
    up_write(&ht_sem);

    printk(KERN_INFO "Hashtable module unloaded\n");
}

MODULE_AUTHOR("Jack Edh");
MODULE_DESCRIPTION("Hashtable Module");
MODULE_LICENSE("GPL");
