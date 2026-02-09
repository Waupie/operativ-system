#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include "hashtable_module.h"

static struct proc_dir_entry* proc_folder;
static struct proc_dir_entry* proc_file;



static ssize_t my_read(struct file* File, char* user_buffer, size_t count, loff_t* offs)
{
    char text[] = "Hello from the proc file!\n";
    int to_copy, not_copied, delta;
    to_copy = min(count, sizeof(text));
    not_copied = copy_to_user(user_buffer, text, to_copy);
    delta = to_copy - not_copied;
    return delta;
}

static ssize_t my_write(struct file* File, const char* user_buffer, size_t count, loff_t* offs)
{
    char text[255];
    int to_copy, not_copied, delta;
    memset(text, 0, sizeof(text));
    to_copy = min(count, sizeof(text));
    not_copied = copy_from_user(text, user_buffer, to_copy);
    printk(KERN_INFO "Received from user through proc: %s\n", text);
    delta = to_copy - not_copied;
    return delta;
}

static struct proc_ops my_proc_ops = {
    .proc_read = my_read,
    .proc_write = my_write,
};


int init_module(void)
{
    printk(KERN_INFO "Hello, World!\n");
    
    proc_folder = proc_mkdir("ht", NULL);
    if (proc_folder == NULL) 
    {
        printk(KERN_ERR "Failed to create proc folder 'ht' \n");
        return -ENOMEM;
    }
    proc_file = proc_create("htfile", 0666, proc_folder, &my_proc_ops);
    if (proc_file == NULL) 
    {
        printk(KERN_ERR "Failed to create proc file 'htfile' \n");
        proc_remove(proc_folder);
        return -ENOMEM;
    }
    printk(KERN_INFO "Proc file created at /proc/ht/htfile\n");
    return 0;
}

void cleanup_module(void)
{
    printk(KERN_INFO "Removing proc dir and file\n");
    proc_remove(proc_file);
    proc_remove(proc_folder);
    printk(KERN_INFO "Goodbye, World!\n");
}


MODULE_AUTHOR("Jack Edh");
MODULE_DESCRIPTION("Hashtable Module");
MODULE_LICENSE("GPL");