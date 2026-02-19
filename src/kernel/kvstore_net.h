#ifndef KVSTORE_NET_H
#define KVSTORE_NET_H

#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/netpoll.h>
#include <linux/inet.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <linux/string.h>

#include "daemon_module.h"
#include "hashtable_module.h"
#include "hashtable_module.h"
#include "kvstore_commands.h"

void net_kvstore_init(void *ht_sem, void *table);
void net_kvstore_exit(void);
void net_kvstore_send_debug(const char *msg);
int process_kv_command(const char *input, char *output, size_t outlen, struct rw_semaphore *sem, ht *table);

#endif // KVSTORE_NET_H
