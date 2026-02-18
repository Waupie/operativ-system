#include "hashtable_module.h"
#ifndef NET_KVSTORE_H
#define NET_KVSTORE_H


#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/netpoll.h>



void net_kvstore_init(void *ht_sem, void *table);
void net_kvstore_exit(void);
void net_kvstore_send_debug(const char *msg);
int process_kv_command(const char *input, char *output, size_t outlen, struct rw_semaphore *sem, ht *table);

#endif // NET_KVSTORE_H
