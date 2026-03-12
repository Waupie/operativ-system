#ifndef KVSTORE_H
#define KVSTORE_H

#include <linux/rwsem.h>
#include "hashtable_module.h"

#define PROC_BUF_SIZE 512

#include "daemon_module.h"

ssize_t ht_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offs);

#endif