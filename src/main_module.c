#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/rwsem.h>
#include "hashtable_module.h"

#define PROC_BUF_SIZE 512

static DECLARE_RWSEM(ht_sem);
static struct proc_dir_entry *proc_file;
static ht *table;

static char proc_output[PROC_BUF_SIZE];
static size_t proc_output_len;


static ssize_t my_read(struct file *file,
                       char __user *user_buffer,
                       size_t count,
                       loff_t *offs)
{
    if (*offs >= proc_output_len)
        return 0;

    if (copy_to_user(user_buffer, proc_output, proc_output_len))
        return -EFAULT;

    *offs = proc_output_len;
    return proc_output_len;
}


static ssize_t my_write(struct file *file,
                        const char __user *user_buffer,
                        size_t count,
                        loff_t *offs)
{
    char buf[256];
    char cmd[16], key[64], value[64];
    int ret = 0;

    memset(proc_output, 0, PROC_BUF_SIZE);
    proc_output_len = 0;

    if (count >= sizeof(buf))
        return -EINVAL;

    if (copy_from_user(buf, user_buffer, count))
        return -EFAULT;

    buf[count] = '\0';
    buf[strcspn(buf, "\n")] = 0;

    if (sscanf(buf, "%15s %63s %63s", cmd, key, value) < 1)
        return count;

    if (!strcmp(cmd, "insert")) {

        down_write(&ht_sem);
        ret = ht_insert(table, key, value);
        up_write(&ht_sem);

        proc_output_len = snprintf(proc_output, PROC_BUF_SIZE,
                                   ret ? "Insert failed\n" : "Inserted\n");

    } else if (!strcmp(cmd, "delete")) {

        down_write(&ht_sem);
        ret = ht_delete(table, key);
        up_write(&ht_sem);

        proc_output_len = snprintf(proc_output, PROC_BUF_SIZE,
                                   ret ? "Delete failed\n" : "Deleted\n");

    } else if (!strcmp(cmd, "lookup")) {

        const char *res;

        down_read(&ht_sem);
        res = ht_search(table, key);
        if (res)
            proc_output_len = snprintf(proc_output, PROC_BUF_SIZE, "%s\n", res);
        else
            proc_output_len = snprintf(proc_output, PROC_BUF_SIZE, "Not found\n");
        up_read(&ht_sem);

    } else {
        proc_output_len = snprintf(proc_output, PROC_BUF_SIZE,
                                   "Unknown command\n");
    }

    return count;
}


static struct proc_ops my_proc_ops = {
    .proc_read  = my_read,
    .proc_write = my_write,
};


int init_module(void)
{
    table = create_ht();
    if (!table)
        return -ENOMEM;

    proc_file = proc_create("ht", 0666, NULL, &my_proc_ops);
    if (!proc_file) {
        destroy_ht(table);
        return -ENOMEM;
    }

    printk(KERN_INFO "Hashtable proc created at /proc/ht\n");
    return 0;
}

void cleanup_module(void)
{
    proc_remove(proc_file);

    down_write(&ht_sem);
    destroy_ht(table);
    up_write(&ht_sem);

    printk(KERN_INFO "Hashtable module unloaded\n");
}

MODULE_AUTHOR("Jack Edh");
MODULE_DESCRIPTION("Hashtable Module");
MODULE_LICENSE("GPL");
