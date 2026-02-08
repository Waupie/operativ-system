#include "hashtable_module.h"

#define SIZE 20
#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL


ht* create_ht(void)
{
    struct ht* table = kmalloc(sizeof(ht), GFP_KERNEL);
    if(table == NULL) {
        return NULL;
    }
    table->capacity = SIZE;

    table->entries = kcalloc(SIZE, sizeof(ht_entry*), GFP_KERNEL);
    if(table->entries == NULL)
    {
        kfree(table);
        return NULL;
    }
    return table;
}

void destroy_ht(ht* table)
{
    for(int i = 0; i < table->capacity; i++)
    {
        struct ht_entry* entry = table->entries[i];
        while(entry != NULL)
        {
            struct ht_entry* temp = entry;
            entry = entry->next;
            kfree(temp->key);
            kfree(temp->value);
            kfree(temp);
        }
    }

    kfree(table->entries);
    kfree(table);
}

uint64_t hash_key(const char* key)
{
    uint64_t hash = FNV_OFFSET;
    for(const char* i = key; *i; i++)
    {
        hash ^= (uint64_t)(*i);
        hash *= FNV_PRIME;
    }
    
    return hash;
}


int ht_insert(ht* table, const char* key, char* value)
{
    uint64_t hash = hash_key(key);
    int index = (int)(hash % table->capacity);

    struct ht_entry* entry = table->entries[index];
    while(entry != NULL)
    {
        if(!strcmp(entry->key, key))
        {
            kfree(entry->value); 
            entry->value = kstrdup(value, GFP_KERNEL);
            if (entry->value == NULL) {
                kfree(entry->key);
                kfree(entry);
                return -ENOMEM;
            }
            return 0;
        }
        entry = entry->next;
    }
    entry = kmalloc(sizeof(ht_entry), GFP_KERNEL);
    if(entry == NULL)
    {
        return -ENOMEM;
    }
    entry->key = kstrdup(key, GFP_KERNEL);
    if(entry->key == NULL)
    {
        kfree(entry);
        return -ENOMEM;
    } 
    entry->value = kstrdup(value, GFP_KERNEL);
    entry->next = table->entries[index];
    table->entries[index] = entry;
    return 0;
}

int ht_delete(ht* table, const char* key)
{
    uint64_t hash = hash_key(key);
    int index = (int)(hash % table->capacity);

    struct ht_entry* prevEntry = NULL;
    struct ht_entry* entry = table->entries[index];
    while(entry != NULL)
    {
        if(!strcmp(entry->key, key))
        {
            if(prevEntry != NULL)
            {
                prevEntry->next = entry->next;
            }
            else
            {
                table->entries[index] = entry->next;
            }
            kfree(entry->key);
            kfree(entry->value);
            kfree(entry);
            return 0;
        }
        prevEntry = entry;
        entry = entry->next;
    }
    return -ENOENT;
}

char* ht_search(ht* table, const char* key)
{
    uint64_t hash = hash_key(key);
    int index = (int)(hash % table->capacity);

    struct ht_entry* entry = table->entries[index];
    while(entry != NULL)
    {
        if(!strcmp(entry->key, key))
        {
            return entry->value;
        }
        entry = entry->next;
    }
    printk(KERN_INFO "No value found for key: %s\n", key);
    return NULL;
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
