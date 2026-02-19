#ifndef KVSTORE_H
#define KVSTORE_H

#include "hashtable_module.h"
#include "kvstore_commands.h"
#include "net_kvstore.h"

ssize_t ht_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offs);
ssize_t ht_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offs);

#endif