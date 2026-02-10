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

#define PROC_BUF_SIZE 512


static DECLARE_RWSEM(ht_sem);
static struct proc_dir_entry *proc_file;
static ht *table;
static atomic_t daemon_flag = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(ht_wq);
static struct task_struct *ht_daemon;

struct cmd_entry {
    char text[PROC_BUF_SIZE];
    struct list_head list;
};
static LIST_HEAD(cmd_history);


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

static ssize_t my_write(struct file *file,
                        const char __user *user_buffer,
                        size_t count,
                        loff_t *offs)
{
    char buf[256];
    char cmd[16], key[64], value[64];
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

    if (sscanf(buf, "%15s %63s %63s", cmd, key, value) < 1)
        return count;

    if (!strcmp(cmd, "insert")) {
        down_write(&ht_sem);
        ret = ht_insert(table, key, value);
        atomic_set(&daemon_flag, 1);
        up_write(&ht_sem);
        wake_up_interruptible(&ht_wq);
        snprintf(output, PROC_BUF_SIZE, ret ? "Insert failed" : "Inserted");

    } else if (!strcmp(cmd, "delete")) {
        down_write(&ht_sem);
        ret = ht_delete(table, key);
        atomic_set(&daemon_flag, 1);
        up_write(&ht_sem);
        wake_up_interruptible(&ht_wq);

        snprintf(output, PROC_BUF_SIZE, ret ? "Delete failed" : "Deleted");

    } else if (!strcmp(cmd, "lookup")) {
        const char *res;
        down_read(&ht_sem);
        res = ht_search(table, key);
        if (res)
            snprintf(output, PROC_BUF_SIZE, "%s", res);
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


static struct proc_ops my_proc_ops = {
    .proc_read = my_read,
    .proc_write = my_write,
};


static int ht_daemon_fn(void *data)
{
    while (1) 
    {
        wait_event_interruptible(ht_wq, atomic_read(&daemon_flag) || kthread_should_stop());

        if(atomic_read(&daemon_flag))
        {
            atomic_set(&daemon_flag, 0);
            down_read(&ht_sem);
            save_hashtable(table);
            up_read(&ht_sem);
        }

        if (kthread_should_stop()) 
            break;

        
    }
    return 0;
}

static void save_hashtable(ht *table, char *filename)
{
    struct file *file;
    loff_t pos = 0;
    int fd;

    mm_segment_t old_fs = get_fs();
    set_fs(KERNEL_DS);

    fd = sys_open(filename, O_WRONLY|O_CREAT, 0644);
    if (fd < 0) {
        printk(KERN_ERR "Failed to open file for writing: %d\n", fd);
        set_fs(old_fs);
        return;
    }
    if(fd >= 0)
    {
        //sys_write(fd, data, strlen(data)); but for table
        file = fget(fd);
        if (file) {
            // Write the hashtable data to the file
            //vfs_write(file, data, strlen(data), &pos); but for table
            fput(file);
        } else {
            printk(KERN_ERR "Failed to get file structure: %d\n", fd);
        }

    }
    sys_close(fd);
    set_fs(old_fs);
}

static void read_hashtable(ht *table)
{
    int fd;
    char buf[512];

    mm_segment_t old_fs = get_fs();
    set_fs(KERNEL_DS);

    fd = sys_open(filename, O_RDONLY, 0);
    if (fd < 0) {
        printk(KERN_ERR "Failed to open file for reading: %d\n", fd);
        return;
    }
    if (sys_read(fd, buf, sizeof(buf)) < 0) {
        printk(KERN_ERR "Failed to read from file: %d\n", fd);
        sys_close(fd);
        return;
    }
    if(fd >= 0)
    {
        // Process the buffer to reconstruct the hashtable
    }
    sys_close(fd);
    set_fs(old_fs);
}

void daemonize() {
    pid_t pid = fork();
    if (pid == 0) {
        // child process
        setsid();  // make session leader (daemonize)
        // run your daemon code here (wait for commands via procfs, socket, etc.)
        while (1) {
            sleep(1); // placeholder for real work
        }
        exit(0);
    }
}


int init_module(void)
{
    table = create_ht();
    if (!table)
        return -ENOMEM;

    ht_daemon = kthread_run(ht_daemon_fn, NULL, "ht_daemon");
    if (IS_ERR(ht_daemon)) 
    {
        destroy_ht(table);
        return PTR_ERR(ht_daemon);
    }

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
    if (ht_daemon)
        kthread_stop(ht_daemon);
    struct cmd_entry *entry, *tmp;

    proc_remove(proc_file);

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
