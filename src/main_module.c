#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include "hashtable_module.h"

int init_module(void)
{
    printk(KERN_INFO "Hello, World!\n");
    test_hashtable();
    test_hashtable_concurrent();
    return 0;
}

void cleanup_module(void)
{
    printk(KERN_INFO "Goodbye, World!\n");
}


MODULE_AUTHOR("Jack Edh");
MODULE_DESCRIPTION("Hashtable Module");
MODULE_LICENSE("GPL");