#ifndef DAEMON_MODULE_H
#define DAEMON_MODULE_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>
#include <linux/pid.h>

#include "kvstore_commands.h"
#include "hashtable_module.h"

void signal_daemon(void);
ssize_t daemon_ht_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offs);
ssize_t daemonpid_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offs);
ssize_t daemonpid_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offs);

extern pid_t daemon_pid;

#endif // DAEMON_MODULE_H