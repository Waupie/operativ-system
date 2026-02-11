#ifndef HASHTABLE_MODULE_H
#define HASHTABLE_MODULE_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>

typedef struct ht_entry 
{
    const char* key;
    char* value;
    struct ht_entry* next;
} ht_entry;

typedef struct ht 
{
    int capacity;
    struct ht_entry** entries;
} ht;

ht* create_ht(void);
void destroy_ht(ht* table);
uint64_t hash_key(const char* key);
int ht_insert(ht* table, const char* key, char* value);
int ht_delete(ht* table, const char* key);
char* ht_search(ht* table, const char* key);
void test_hashtable(void);
int init_module(void);
void cleanup_module(void);
void test_hashtable_concurrent(void);

#endif