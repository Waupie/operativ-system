#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>

#include "net_kvstore.h"
#include "hashtable_module.h"
#include "kvstore_commands.h"


static DECLARE_RWSEM(ht_sem);

static struct proc_dir_entry *proc_ht;
static struct proc_dir_entry *proc_hashtable;
static struct proc_dir_entry *proc_daemonpid;

static pid_t daemon_pid = -1;

static ht *table;

LIST_HEAD(cmd_history);

//static void signal_daemon(void);

void signal_daemon(void)
{
    struct pid *pid_struct;
    struct task_struct *task;

    if (daemon_pid <= 0)
        return;

    pid_struct = find_get_pid(daemon_pid);
    if (!pid_struct)
        return;

    task = pid_task(pid_struct, PIDTYPE_PID);
    if (!task)
        return;

    send_sig_info(SIGUSR1, SEND_SIG_PRIV, task);
    printk(KERN_INFO "Sent SIGUSR1 to daemon PID %d\n", daemon_pid);
}


/* /proc/ht
 * write: insert/delete/lookup
 * read : returns last lookup result for the calling process
 */
static ssize_t ht_write(struct file *file,
                        const char __user *user_buffer,
                        size_t count,
                        loff_t *offs)
{
    char buf[256];
    char output[PROC_BUF_SIZE];

    memset(buf, 0, sizeof(buf));
    memset(output, 0, sizeof(output));

    if (count >= sizeof(buf))
        return -EINVAL;

    if (copy_from_user(buf, user_buffer, count))
        return -EFAULT;

    buf[count] = '\0';
    buf[strcspn(buf, "\n")] = 0;

    process_kv_command(buf, output, sizeof(output), &ht_sem, table);
    signal_daemon();

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

static ssize_t ht_read(struct file *file,
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
    list_for_each_entry(entry, &cmd_history, list) {
        len += snprintf(buf + len, PROC_BUF_SIZE - len, "%s\n", entry->text);
        if (len >= PROC_BUF_SIZE - 1)
            break;
    }
    up_read(&ht_sem);

    if (copy_to_user(user_buffer, buf, len))
        return -EFAULT;

    *offs = len;
    return len;
}
/* /proc/hashtable
 * read only: prints entire table for daemon
 */
static ssize_t daemon_ht_read(struct file *file,
                              char __user *user_buffer,
                              size_t count,
                              loff_t *offs)
{
    char buf[PROC_BUF_SIZE];
    size_t len = 0;
    int i;
    ht_entry *e;

    if (*offs > 0)
        return 0;

    down_read(&ht_sem);

    for (i = 0; i < table->capacity; i++) {
        e = table->entries[i];
        while (e) {
            len += scnprintf(buf + len, PROC_BUF_SIZE - len,
                             "%s %s\n", e->key, e->value);
            if (len >= PROC_BUF_SIZE - 1)
                break;
            e = e->next;
        }
    }

    up_read(&ht_sem);

    return simple_read_from_buffer(user_buffer, count, offs, buf, len);
}

static ssize_t daemonpid_read(struct file *file,
                              char __user *user_buffer,
                              size_t count,
                              loff_t *offs)
{
    char buf[32];
    int len;

    if (*offs > 0)
        return 0;

    snprintf(buf, sizeof(buf), "%d\n", current->pid);
    len = strlen(buf);

    if (copy_to_user(user_buffer, buf, len))
        return -EFAULT;

    *offs = len;
    return len;
}

static ssize_t daemonpid_write(struct file *file,
                               const char __user *user_buffer,
                               size_t count,
                               loff_t *offs)
{
    char buf[32];
    long pid;
    
    if (count >= sizeof(buf))
        return -EINVAL;

    if (copy_from_user(buf, user_buffer, count))
        return -EFAULT;

    buf[count] = '\0';

    if (kstrtol(buf, 10, &pid) < 0)
        return -EINVAL;

    daemon_pid = (pid_t)pid;
    printk(KERN_INFO "Daemon PID received: %d\n", daemon_pid);

    return count;
}


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
