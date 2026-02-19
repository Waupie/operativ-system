#include "kvstore.h"

extern struct rw_semaphore ht_sem; // refers to ht_sem in main_module.c, used for synchronizing access to cmd_history and table
extern ht *table; // refers to table in main_module.c

ssize_t ht_write(struct file *file,
                        const char __user *user_buffer,
                        size_t count,
                        loff_t *offs)
{
    char buf[256];
    char output[PROC_BUF_SIZE];
    char cmd[16];

    memset(buf, 0, sizeof(buf));
    memset(output, 0, sizeof(output));

    if (count >= sizeof(buf))
        return -EINVAL;

    if (copy_from_user(buf, user_buffer, count))
        return -EFAULT;

    buf[count] = '\0';
    buf[strcspn(buf, "\n")] = 0;

    sscanf(buf, "%15s", cmd);

    process_kv_command(buf, output, sizeof(output), &ht_sem, table);
    //signal_daemon(); not needed as it is being used in process_kv_command, keep here incase for awhile..

    if (strcmp(cmd, "lookup") != 0)
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

ssize_t ht_read(struct file *file,
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