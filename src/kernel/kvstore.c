#include "kvstore.h"

extern struct rw_semaphore ht_sem; // refers to ht_sem in main_module.c, used for synchronizing access to table
extern ht *table; // refers to table in main_module.c

static int process_kv_command(const char *input, char *output, size_t outlen, struct rw_semaphore *sem, ht *table) {
    char cmd[16], key[64], value[64];
    int ret = 0;
    const char *res = NULL;
    memset(cmd, 0, sizeof(cmd));
    memset(key, 0, sizeof(key));
    memset(value, 0, sizeof(value));
    if (sscanf(input, "%15s %63s %63s", cmd, key, value) < 1) {
        snprintf(output, outlen, "Invalid command");
        return -EINVAL;
    }
    if (!strcmp(cmd, "insert")) {
        down_write(sem);
        ret = ht_insert(table, key, value);
        up_write(sem);
        signal_daemon();
        snprintf(output, outlen, ret ? "Insert failed" : "Inserted key: %s, value: %s", key, value);
    } else if (!strcmp(cmd, "delete")) {
        down_write(sem);
        ret = ht_delete(table, key);
        up_write(sem);
        signal_daemon();
        snprintf(output, outlen, ret ? "Delete failed" : "Deleted key: %s", key);
    } else if (!strcmp(cmd, "lookup")) {
        down_read(sem);
        res = ht_search(table, key);
        if (res)
            snprintf(output, outlen, "Lookup on key: %s, gave value: %s", key, res);
        else
            snprintf(output, outlen, "Not found");
        up_read(sem);
    } else {
        snprintf(output, outlen, "Unknown command");
    }
    return 0;
}

ssize_t ht_write(struct file *file,
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

    return count;
}