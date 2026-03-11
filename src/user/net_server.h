#ifndef NET_SERVER_H
#define NET_SERVER_H

#define KVSTORE_PORT 5555
#define NET_BUF_SIZE 512

/**
 * Start the UDP server that listens for remote key-value commands.
 * Commands received are forwarded to /proc/ht.
 * Responses are sent back to the sender.
 * This function blocks; run it in a separate thread.
 */
void *net_server_run(void *arg);

/**
 * Stop the UDP server gracefully.
 */
void net_server_stop(void);

#endif /* NET_SERVER_H */
