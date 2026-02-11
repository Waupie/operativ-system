#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/sched.h>

#include "hashtable_module.h"

#define PROC_BUF_SIZE PAGE_SIZE

//synchronization for hashtable access

static DECLARE_RWSEM(ht_sem);

//proc

static struct proc_dir_entry *proc_ht;
static struct proc_dir_entry *proc_lookup;
static struct proc_dir_entry *proc_hashtable;


//global variables
static ht *table;

struct lookup_session {
    pid_t pid;
    char buf[PROC_BUF_SIZE];
    size_t len;
    struct list_head list;
};

static LIST_HEAD(lookup_sessions);


//helper func
static struct lookup_session *get_session(pid_t pid)
{
    struct lookup_session *s;

    list_for_each_entry(s, &lookup_sessions, list) {
        if (s->pid == pid)
            return s;
    }

    s = kmalloc(sizeof(*s), GFP_KERNEL);
    if (!s)
        return NULL;

    s->pid = pid;
    s->len = 0;
    list_add_tail(&s->list, &lookup_sessions);

    return s;
}
/*
 * /proc/ht
 *   write: insert/delete/lookup
 *   read : current hashtable
 */

static ssize_t ht_write(struct file *file,
                        const char __user *user_buffer,
                        size_t count,
                        loff_t *offs)
{
    char input[256];
    char cmd[16], key[128], value[128];
    struct lookup_session *s;
    const char *res;

    if (count >= sizeof(input))
        return -EINVAL;

    if (copy_from_user(input, user_buffer, count))
        return -EFAULT;

    input[count] = '\0';
    input[strcspn(input, "\n")] = 0;

    if (sscanf(input, "%15s %127s %127s", cmd, key, value) < 2)
        return count;

    down_write(&ht_sem);

    if (!strcmp(cmd, "insert")) {
        ht_insert(table, key, value);

    } else if (!strcmp(cmd, "delete")) {
        ht_delete(table, key);

    } else if (!strcmp(cmd, "lookup")) {
        s = get_session(current->pid);
        if (s && s->len < PROC_BUF_SIZE) {
            res = ht_search(table, key);
            s->len += scnprintf(
                s->buf + s->len,
                PROC_BUF_SIZE - s->len,
                res ? "lookup %s=%s\n"
                    : "lookup %s: not found\n",
                key, res ?: ""
            );
        }
    }

    up_write(&ht_sem);
    return count;
}

static ssize_t ht_read(struct file *file,
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

    len += scnprintf(buf + len, PROC_BUF_SIZE - len,
                     "Hashtable:\n");

    for (i = 0; i < table->capacity; i++) {
        e = table->entries[i];
        while (e) {
            len += scnprintf(buf + len,
                             PROC_BUF_SIZE - len,
                             "%s=%s\n",
                             e->key, e->value);
            if (len >= PROC_BUF_SIZE - 1)
                break;
            e = e->next;
        }
    }

    up_read(&ht_sem);

    return simple_read_from_buffer(
        user_buffer, count, offs, buf, len
    );
}

static const struct proc_ops ht_proc_ops = {
    .proc_read  = ht_read,
    .proc_write = ht_write,
};

/*
 * /proc/lookup 
 *  read: lookup value for key
*/

static ssize_t lookup_read(struct file *file,
                           char __user *user_buffer,
                           size_t count,
                           loff_t *offs)
{
    struct lookup_session *s;

    if (*offs > 0)
        return 0;

    down_read(&ht_sem);
    s = get_session(current->pid);
    up_read(&ht_sem);

    if (!s)
        return 0;

    return simple_read_from_buffer(
        user_buffer, count, offs, s->buf, s->len
    );
}

static const struct proc_ops lookup_proc_ops = {
    .proc_read = lookup_read,
};

/* 
 * /proc/hashtable
 *  read: current hashtable for daemon
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
            len += scnprintf(buf + len,
                             PROC_BUF_SIZE - len,
                             "%s %s\n",
                             e->key, e->value);
            if (len >= PROC_BUF_SIZE - 1)
                break;
            e = e->next;
        }
    }

    up_read(&ht_sem);

    return simple_read_from_buffer(
        user_buffer, count, offs, buf, len
    );
}

static const struct proc_ops hashtable_proc_ops = {
    .proc_read = daemon_ht_read,
};


//module init/exit (temporary)
static int __init ht_init(void)
{
    table = create_ht();
    if (!table)
        return -ENOMEM;

    proc_ht = proc_create("ht", 0666, NULL, &ht_proc_ops);
    proc_lookup = proc_create("lookup", 0444, NULL, &lookup_proc_ops);
    proc_hashtable = proc_create("hashtable", 0444, NULL,
                                 &hashtable_proc_ops);

    if (!proc_ht || !proc_lookup || !proc_hashtable) {
        destroy_ht(table);
        return -ENOMEM;
    }

    printk(KERN_INFO "hashtable proc module loaded\n");
    return 0;
}

static void __exit ht_exit(void)
{
    struct lookup_session *s, *tmp;

    proc_remove(proc_ht);
    proc_remove(proc_lookup);
    proc_remove(proc_hashtable);

    down_write(&ht_sem);
    list_for_each_entry_safe(s, tmp, &lookup_sessions, list) {
        list_del(&s->list);
        kfree(s);
    }
    up_write(&ht_sem);

    destroy_ht(table);
    printk(KERN_INFO "hashtable proc module unloaded\n");
}

module_init(ht_init);
module_exit(ht_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jack Edh");
MODULE_DESCRIPTION("Hashtable kernel module with per-process lookup and daemon snapshot");
