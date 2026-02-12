#ifndef PROC_HANDLERS_H
#define PROC_HANDLERS_H

#include <linux/proc_fs.h>
#include "hashtable_module.h"

int proc_handlers_init(struct rw_semaphore *ht_sem, struct ht *table);
void proc_handlers_cleanup(void);

#endif