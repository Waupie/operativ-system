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
    int ret = 0;
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
    return ret;
}

int ht_delete(ht* table, const char* key)
{
    int ret = -ENOENT;
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
            ret = 0;
            break;
        }
        prevEntry = entry;
        entry = entry->next;
    }
    return ret;
}

char* ht_search(ht* table, const char* key)
{
    char* result = NULL;
    uint64_t hash = hash_key(key);
    int index = (int)(hash % table->capacity);
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
    if (!result)
        printk(KERN_INFO "No value found for key: %s\n", key);
    return result;
}