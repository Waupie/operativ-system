#ifndef HASHTABLE_MODULE_H
#define HASHTABLE_MODULE_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>

typedef struct value_node {
    char *value;
    struct value_node *next;
} value_node;

typedef struct ht_entry {
    char *key;
    value_node *values; // Linked list of values
    struct ht_entry *next;
} ht_entry;

/*
Den gamla ht_entry structen, som bara hade en value, har nu en pekare till 
en linked list av value_node structs. 
Varje value_node innehåller en value och en pekare till nästa node i listan. 
Detta gör att varje nyckel kan ha flera värden kopplade till sig, 
vilket möjliggör att hantera dubbletter av nycklar i hashtabellen.
typedef struct ht_entry 
{
    const char* key;
    char* value;
    struct ht_entry* next;
} ht_entry;

*/

typedef struct ht 
{
    int capacity;
    struct ht_entry** entries;
} ht;

ht* create_ht(void);
void destroy_ht(ht* table);
uint64_t hash_key(const char* key);
void ht_insert(ht* table, const char* key, const char* value);
int ht_delete(ht* table, const char* key);
char* ht_search(ht* table, const char* key);
void test_hashtable(void);
int init_module(void);
void cleanup_module(void);
void test_hashtable_concurrent(void);
char* ht_search_all(ht* table, const char* key, char* outbuf, size_t outbuf_size);

#endif