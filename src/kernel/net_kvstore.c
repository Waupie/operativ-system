#include <linux/string.h>

#include <linux/inet.h>

#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/rwsem.h>
#include <linux/netpoll.h>
#include "hashtable_module.h"
#include "kvstore_commands.h"
#include "net_kvstore.h"
#include "daemon_module.h"

#define KVSTORE_PORT 5555
#define NETPOLL_REMOTE_IP "192.168.1.100"
#define NETPOLL_REMOTE_PORT 6666
#define NETPOLL_LOCAL_IP "192.168.1.1"
#define NETPOLL_LOCAL_PORT 6666
#define NETPOLL_IFNAME "eth0"

static struct nf_hook_ops nfho;
static struct rw_semaphore *net_ht_sem;
static ht *net_table;
static struct netpoll np;
static int netpoll_initialized = 0;

// Shared command processing function
int process_kv_command(const char *input, char *output, size_t outlen, struct rw_semaphore *sem, ht *table) {
    char cmd[16], key[64], value[64];
    int ret = 0;
    const char *res = NULL;
    memset(cmd, 0, sizeof(cmd));
    memset(key, 0, sizeof(key));
    memset(value, 0, sizeof(value));
    if (sscanf(input, "%15s %63s %63s", cmd, key, value) < 1) {
        snprintf(output, outlen, "Invalid command");
        return -EINVAL;
    }
    if (!strcmp(cmd, "insert")) {
        down_write(sem);
        ret = ht_insert(table, key, value);
        up_write(sem);
        signal_daemon();
        snprintf(output, outlen, ret ? "Insert failed" : "Inserted key: %s, value: %s", key, value);
    } else if (!strcmp(cmd, "delete")) {
        down_write(sem);
        ret = ht_delete(table, key);
        up_write(sem);
        signal_daemon();
        snprintf(output, outlen, ret ? "Delete failed" : "Deleted key: %s", key);
    } else if (!strcmp(cmd, "lookup")) {
        down_read(sem);
        res = ht_search(table, key);
        if (res)
            snprintf(output, outlen, "Lookup on key: %s, gave value: %s", key, res);
        else
            snprintf(output, outlen, "Not found");
        up_read(sem);
    } else {
        snprintf(output, outlen, "Unknown command");
    }
    return 0;
}

void net_kvstore_send_debug(const char *msg)
{
    if (netpoll_initialized)
        netpoll_send_udp(&np, msg, strlen(msg));
}

static unsigned int kvstore_nf_hook(void *priv,
                                    struct sk_buff *skb,
                                    const struct nf_hook_state *state)
{
    struct iphdr *ip_header;
    struct udphdr *udp_header;
    unsigned char *data;
    unsigned int data_len;

    if (!skb)
        return NF_ACCEPT;

    ip_header = ip_hdr(skb);
    if (!ip_header || ip_header->protocol != IPPROTO_UDP)
        return NF_ACCEPT;

    udp_header = udp_hdr(skb);
    if (!udp_header)
        return NF_ACCEPT;

    if (ntohs(udp_header->dest) != KVSTORE_PORT)
        return NF_ACCEPT;

    data = (unsigned char *)((unsigned char *)udp_header + sizeof(struct udphdr));
    data_len = ntohs(udp_header->len) - sizeof(struct udphdr);

    if (data_len > 0 && data_len < 256) {
        char output[256];
        memset(output, 0, sizeof(output));
        process_kv_command((char *)data, output, sizeof(output), net_ht_sem, net_table);
        // Log result to /proc/ht history
        {
            struct cmd_entry *entry = kmalloc(sizeof(*entry), GFP_KERNEL);
            if (entry) {
                strncpy(entry->text, output, PROC_BUF_SIZE - 1);
                entry->text[PROC_BUF_SIZE - 1] = '\0';
                down_write(net_ht_sem);
                list_add_tail(&entry->list, &cmd_history);
                up_write(net_ht_sem);
            }
        }
    }

    return NF_ACCEPT;
}

void net_kvstore_init(void *ht_sem, void *table)
{
    net_ht_sem = (struct rw_semaphore *)ht_sem;
    net_table = (ht *)table;

    nfho.hook = kvstore_nf_hook;
    nfho.pf = PF_INET;
    nfho.hooknum = NF_INET_PRE_ROUTING;
    nfho.priority = NF_IP_PRI_FIRST;
    nf_register_net_hook(&init_net, &nfho);

    // Setup Netpoll for debug messages
    memset(&np, 0, sizeof(np));
    np.name = "kvstore_netpoll";
    strncpy(np.dev_name, NETPOLL_IFNAME, sizeof(np.dev_name) - 1);
    np.dev_name[sizeof(np.dev_name) - 1] = '\0';
    np.local_ip.ip = in_aton(NETPOLL_LOCAL_IP);
    np.remote_ip.ip = in_aton(NETPOLL_REMOTE_IP);
    np.local_port = NETPOLL_LOCAL_PORT;
    np.remote_port = NETPOLL_REMOTE_PORT;
    np.remote_mac[0] = 0xff; np.remote_mac[1] = 0xff; np.remote_mac[2] = 0xff;
    np.remote_mac[3] = 0xff; np.remote_mac[4] = 0xff; np.remote_mac[5] = 0xff;
    if (!netpoll_setup(&np))
        netpoll_initialized = 1;
    else
        netpoll_initialized = 0;
}

void net_kvstore_exit(void)
{
    nf_unregister_net_hook(&init_net, &nfho);

    if (netpoll_initialized) {
        netpoll_cleanup(&np);
        netpoll_initialized = 0;
    }
}
