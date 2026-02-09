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
    table->bucket_locks = kmalloc_array(SIZE, sizeof(spinlock_t), GFP_KERNEL);
    if (!table->bucket_locks) {
        kfree(table->entries);
        kfree(table);
        return NULL;
    }
    for (int i = 0; i < SIZE; i++) {
        spin_lock_init(&table->bucket_locks[i]);
    }
    return table;
}

void destroy_ht(ht* table)
{
    for(int i = 0; i < table->capacity; i++)
    {
        spin_lock(&table->bucket_locks[i]);
        struct ht_entry* entry = table->entries[i];
        while(entry != NULL)
        {
            struct ht_entry* temp = entry;
            entry = entry->next;
            kfree(temp->key);
            kfree(temp->value);
            kfree(temp);
        }
        spin_unlock(&table->bucket_locks[i]);
    }
    kfree(table->entries);
    kfree(table->bucket_locks);
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
    int ret = 0;
    uint64_t hash = hash_key(key);
    int index = (int)(hash % table->capacity);
    spin_lock(&table->bucket_locks[index]);
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
                ret = -ENOMEM;
                goto out;
            }
            ret = 0;
            goto out;
        }
        entry = entry->next;
    }
    entry = kmalloc(sizeof(ht_entry), GFP_KERNEL);
    if(entry == NULL)
    {
        ret = -ENOMEM;
        goto out;
    }
    entry->key = kstrdup(key, GFP_KERNEL);
    if(entry->key == NULL)
    {
        kfree(entry);
        ret = -ENOMEM;
        goto out;
    } 
    entry->value = kstrdup(value, GFP_KERNEL);
    entry->next = table->entries[index];
    table->entries[index] = entry;
    ret = 0;
out:
    spin_unlock(&table->bucket_locks[index]);
    return ret;
}

int ht_delete(ht* table, const char* key)
{
    int ret = -ENOENT;
    uint64_t hash = hash_key(key);
    int index = (int)(hash % table->capacity);
    spin_lock(&table->bucket_locks[index]);
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
            ret = 0;
            break;
        }
        prevEntry = entry;
        entry = entry->next;
    }
    spin_unlock(&table->bucket_locks[index]);
    return ret;
}

char* ht_search(ht* table, const char* key)
{
    char* result = NULL;
    uint64_t hash = hash_key(key);
    int index = (int)(hash % table->capacity);
    spin_lock(&table->bucket_locks[index]);
    struct ht_entry* entry = table->entries[index];
    while(entry != NULL)
    {
        if(!strcmp(entry->key, key))
        {
            result = entry->value;
            break;
        }
        entry = entry->next;
    }
    spin_unlock(&table->bucket_locks[index]);
    if (!result)
        printk(KERN_INFO "No value found for key: %s\n", key);
    return result;
}