int init_module(void)
{
    printk(KERN_INFO "Hello, World!\n");
    test_hashtable();
    return 0;
}

void cleanup_module(void)
{
    printk(KERN_INFO "Goodbye, World!\n");
}

module_init(init_module);
module_exit(cleanup_module);
MODULE_AUTHOR("Jack Edh");
MODULE_DESCRIPTION("Hashtable Module");
MODULE_LICENSE("GPL");