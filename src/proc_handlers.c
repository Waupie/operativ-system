#include "proc_handlers.h"
#include "hashtable_module.h"
#include "daemon_signal.h"
#include "cmd_history.h"

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rwsem.h>

#define PROC_BUF_SIZE 512

static struct proc_dir_entry *proc_ht;
static struct proc_dir_entry *proc_hashtable;
static struct proc_dir_entry *proc_daemonpid;

static struct rw_semaphore *local_ht_sem;
static struct ht *local_table;

static ssize_t ht_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offs)
{
    char buf[256], cmd[16], key[64], value[64], output[PROC_BUF_SIZE];
    int ret = 0;

    memset(buf, 0, sizeof(buf));
    memset(output, 0, sizeof(output));

    if (count >= sizeof(buf))
        return -EINVAL;
    if (copy_from_user(buf, user_buffer, count))
        return -EFAULT;
    buf[count] = '\0';
    buf[strcspn(buf, "\n")] = 0;

    if (sscanf(buf, "%15s %63s %63s", cmd, key, value) < 1)
        return count;

    if (!strcmp(cmd, "insert")) {
        down_write(local_ht_sem);
        ret = ht_insert(local_table, key, value);
        signal_daemon();
        up_write(local_ht_sem);
        snprintf(output, PROC_BUF_SIZE, ret ? "Insert failed" : "Inserted key: %s, value: %s", key, value);
    } else if (!strcmp(cmd, "delete")) {
        down_write(local_ht_sem);
        ret = ht_delete(local_table, key);
        signal_daemon();
        up_write(local_ht_sem);
        snprintf(output, PROC_BUF_SIZE, ret ? "Delete failed" : "Deleted key: %s", key);
    } else if (!strcmp(cmd, "lookup")) {
        const char *res;
        down_read(local_ht_sem);
        res = ht_search(local_table, key);
        if (res)
            snprintf(output, PROC_BUF_SIZE, "Lookup on key: %s, gave value: %s", key, res);
        else
            snprintf(output, PROC_BUF_SIZE, "Not found");
        up_read(local_ht_sem);
    } else {
        snprintf(output, PROC_BUF_SIZE, "Unknown command");
    }

    cmd_history_add(output);

    return count;
}

static ssize_t ht_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offs)
{
    char buf[PROC_BUF_SIZE];
    int len = 0;

    if (*offs > 0)
        return 0;  // EOF

    memset(buf, 0, PROC_BUF_SIZE);

    len = cmd_history_get(buf, PROC_BUF_SIZE);

    if (copy_to_user(user_buffer, buf, len))
        return -EFAULT;

    *offs = len;
    return len;
}

static ssize_t daemon_ht_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offs)
{
    char buf[PROC_BUF_SIZE];
    size_t len = 0;
    int i;
    ht_entry *e;

    if (*offs > 0)
        return 0;

    down_read(local_ht_sem);

    for (i = 0; i < local_table->capacity; i++) {
        e = local_table->entries[i];
        while (e) {
            len += scnprintf(buf + len, PROC_BUF_SIZE - len, "%s %s\n", e->key, e->value);
            if (len >= PROC_BUF_SIZE - 1)
                break;
            e = e->next;
        }
    }

    up_read(local_ht_sem);

    return simple_read_from_buffer(user_buffer, count, offs, buf, len);
}

static ssize_t daemonpid_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offs)
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

static ssize_t daemonpid_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offs)
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

    set_daemon_pid((pid_t)pid);
    printk(KERN_INFO "Daemon PID received: %ld\n", pid);

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

int proc_handlers_init(struct rw_semaphore *ht_sem, struct ht *table)
{
    local_ht_sem = ht_sem;
    local_table = table;

    proc_ht = proc_create("ht", 0666, NULL, &ht_proc_ops);
    proc_hashtable = proc_create("hashtable", 0444, NULL, &hashtable_proc_ops);
    proc_daemonpid = proc_create("daemonpid", 0666, NULL, &daemonpid_proc_ops);

    if (!proc_ht || !proc_hashtable || !proc_daemonpid)
        return -ENOMEM;

    return 0;
}

void proc_handlers_cleanup(void)
{
    proc_remove(proc_ht);
    proc_remove(proc_hashtable);
    proc_remove(proc_daemonpid);
}