
#include "../src/kernel/hashtable_module.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/random.h>
#define NUM_THREADS 4
#define NUM_OPS 100

struct thread_args {
    ht *table;
    int id;
};

static int hashtable_thread(void *data) {
    struct thread_args *args = (struct thread_args *)data;
    char key[16], val[16];
    for (int i = 0; i < NUM_OPS; i++) {
        int op = get_random_u32() % 3;
        int n = get_random_u32() % 50;
        snprintf(key, sizeof(key), "k%d", n);
        snprintf(val, sizeof(val), "v%d", n);
        if (op == 0) {
            ht_insert(args->table, key, kstrdup(val, GFP_KERNEL));
            printk(KERN_INFO "[T%d] Inserted %s => %s\n", args->id, key, val);
        } else if (op == 1) {
            char *found = ht_search(args->table, key);
            if (found)
                printk(KERN_INFO "[T%d] Found %s => %s\n", args->id, key, found);
        } else {
            ht_delete(args->table, key);
            printk(KERN_INFO "[T%d] Deleted %s\n", args->id, key);
        }
        msleep(10);
    }
    return 0;
}

void test_hashtable_concurrent(void)
{
    ht *table = create_ht();
    struct task_struct *threads[NUM_THREADS];
    struct thread_args args[NUM_THREADS];
    printk(KERN_INFO "=== Concurrent Hashtable test start ===\n");
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].table = table;
        args[i].id = i;
        threads[i] = kthread_run(hashtable_thread, &args[i], "ht_tester_%d", i);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        if (threads[i])
            kthread_stop(threads[i]);
    }
    destroy_ht(table);
    printk(KERN_INFO "=== Concurrent Hashtable test end ===\n");
}


void test_hashtable(void)
{
    ht *table;
    char *value;

    printk(KERN_INFO "=== Hashtable test start ===\n");

    table = create_ht();
    if (!table) {
        printk(KERN_ERR "Failed to create hashtable\n");
        return;
    }

    printk(KERN_INFO "Hashtable created\n");

    /* Basic inserts */
    ht_insert(table, "name", kstrdup("jack", GFP_KERNEL));
    ht_insert(table, "course", kstrdup("os", GFP_KERNEL));
    ht_insert(table, "year", kstrdup("2026", GFP_KERNEL));

    value = ht_search(table, "name");
    if (value) printk(KERN_INFO "name => %s\n", value);

    value = ht_search(table, "course");
    if (value) printk(KERN_INFO "course => %s\n", value);

    value = ht_search(table, "year");
    if (value) printk(KERN_INFO "year => %s\n", value);

    /* Overwrite existing key */
    printk(KERN_INFO "Test: overwrite existing key\n");
    ht_insert(table, "name", kstrdup("jack2", GFP_KERNEL));

    value = ht_search(table, "name");
    if (value) printk(KERN_INFO "name (after overwrite) => %s\n", value);

    /* Collision handling */
    printk(KERN_INFO "Test: collision handling\n");
    ht_insert(table, "a", kstrdup("1", GFP_KERNEL));
    ht_insert(table, "b", kstrdup("2", GFP_KERNEL));
    ht_insert(table, "c", kstrdup("3", GFP_KERNEL));

    ht_delete(table, "b");

    value = ht_search(table, "a");
    if (value) printk(KERN_INFO "a => %s\n", value);

    value = ht_search(table, "b");
    if (!value) printk(KERN_INFO "b deleted correctly\n");

    value = ht_search(table, "c");
    if (value) printk(KERN_INFO "c => %s\n", value);

    /* Delete non-existent key */
    printk(KERN_INFO "Test: delete missing key\n");
    if (ht_delete(table, "does-not-exist") == -ENOENT)
        printk(KERN_INFO "Correctly handled delete of missing key\n");

    /* Empty string key */
    printk(KERN_INFO "Test: empty string key\n");
    ht_insert(table, "", kstrdup("empty", GFP_KERNEL));

    value = ht_search(table, "");
    if (value) printk(KERN_INFO "empty key => %s\n", value);

    /* Long key */
    printk(KERN_INFO "Test: long key\n");
    ht_insert(
        table,
        "this_is_a_very_long_key_to_test_hashing_and_memory_handling",
        kstrdup("longvalue", GFP_KERNEL)
    );

    value = ht_search(
        table,
        "this_is_a_very_long_key_to_test_hashing_and_memory_handling"
    );
    if (value) printk(KERN_INFO "long key => %s\n", value);

    /* Many inserts (force collisions) */
    printk(KERN_INFO "Test: many inserts\n");
    for (int i = 0; i < 50; i++) {
        char key[16];
        char val[16];

        snprintf(key, sizeof(key), "k%d", i);
        snprintf(val, sizeof(val), "v%d", i);

        ht_insert(table, key, kstrdup(val, GFP_KERNEL));
    }

    value = ht_search(table, "k42");
    if (value) printk(KERN_INFO "k42 => %s\n", value);

    /* Final cleanup */
    destroy_ht(table);
    printk(KERN_INFO "Hashtable destroyed\n");
    printk(KERN_INFO "=== Hashtable test end ===\n");
}
