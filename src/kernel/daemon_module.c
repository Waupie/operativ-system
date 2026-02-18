#include "daemon_module.h"

extern struct rw_semaphore ht_sem; // refers to ht_sem in main_module.c, used for synchronizing access to cmd_history and table
extern ht *table; // refers to table in main_module.c

pid_t daemon_pid = -1;

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

/* /proc/hashtable
 * read only: prints entire table for daemon
 */
ssize_t daemon_ht_read(struct file *file,
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

ssize_t daemonpid_read(struct file *file,
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

ssize_t daemonpid_write(struct file *file,
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