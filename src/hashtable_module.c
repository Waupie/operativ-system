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
            value_node *vn = temp->values;
            while (vn) {
                value_node *tmp_vn = vn;
                vn = vn->next;
                kfree(tmp_vn->value);
                kfree(tmp_vn);
            }
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


void ht_insert(ht *table, const char *key, const char *value) {
    uint64_t idx = hash_key(key) % table->capacity;
    ht_entry *e = table->entries[idx];

    while (e) {
        if (strcmp(e->key, key) == 0) {
            // Key exists, append value if not duplicate
            value_node *vn = e->values;
            while (vn) {
                if (strcmp(vn->value, value) == 0)
                    return; // Don't add duplicate value
                if (!vn->next) break;
                vn = vn->next;
            }
            value_node *new_vn = kmalloc(sizeof(value_node), GFP_KERNEL);
            new_vn->value = kstrdup(value, GFP_KERNEL);
            new_vn->next = NULL;
            if (vn)
                vn->next = new_vn;
            else
                e->values = new_vn;
            return;
        }
        e = e->next;
    }

    // Key does not exist, create new entry
    ht_entry *new_entry = kmalloc(sizeof(ht_entry), GFP_KERNEL);
    new_entry->key = kstrdup(key, GFP_KERNEL);
    new_entry->next = table->entries[idx];
    table->entries[idx] = new_entry;

    value_node *new_vn = kmalloc(sizeof(value_node), GFP_KERNEL);
    new_vn->value = kstrdup(value, GFP_KERNEL);
    new_vn->next = NULL;
    new_entry->values = new_vn;
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
            value_node *vn = entry->values;
            while (vn) {
                value_node *tmp_vn = vn;
                vn = vn->next;
                kfree(tmp_vn->value);
                kfree(tmp_vn);
            }
            kfree(entry);
            ret = 0;
            break;
        }
        prevEntry = entry;
        entry = entry->next;
    }
    return ret;
}
/*
char* ht_search(ht* table, const char* key)
{
    uint64_t hash = hash_key(key);
    int index = hash % table->capacity;
    struct ht_entry* entry = table->entries[index];

    while (entry) {
        if (!strcmp(entry->key, key))
            return entry->value;
        entry = entry->next;
    }

    return NULL;
}
*/

char* ht_search(ht* table, const char* key)
{
    uint64_t idx = hash_key(key) % table->capacity;
    ht_entry *e = table->entries[idx];
    while (e) {
        if (strcmp(e->key, key) == 0 && e->values) {
            return e->values->value; // Return first value
        }
        e = e->next;
    }
    return NULL;
}

char* ht_search_all(ht* table, const char* key, char* outbuf, size_t outbuf_size)
{
    uint64_t idx = hash_key(key) % table->capacity;
    ht_entry *e = table->entries[idx];
    size_t len = 0;

    while (e) {
        if (strcmp(e->key, key) == 0) {
            value_node *vn = e->values;
            while (vn) {
                len += scnprintf(outbuf + len, outbuf_size - len, "%s ", vn->value);
                vn = vn->next;
            }
            if (len > 0 && len < outbuf_size)
                outbuf[len-1] = '\0'; // Remove trailing space
            return outbuf;
        }
        e = e->next;
    }
    return NULL;
}