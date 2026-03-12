#ifndef NET_SERVER_H
#define NET_SERVER_H

#define KVSTORE_PORT 5555
#define NET_BUF_SIZE 512

#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "debug_net.h"

/* Per-client connection info passed to the handler thread */
typedef struct {
    int fd;
    char addr[INET_ADDRSTRLEN];
    int port;
} client_info;


int forward_to_proc(const char *cmd, char *response, size_t resp_len);

void *handle_client(void *arg);
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

int authenticate_user(const char *user, const char *pass);

#endif /* NET_SERVER_H */
