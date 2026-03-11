#include "debug_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

static int debug_fd = -1;
static struct sockaddr_in debug_target;
static int debug_enabled = 0;

int debug_init(const char *remote_ip, int remote_port)
{
    if (!remote_ip || strlen(remote_ip) == 0)
        return -1;

    debug_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (debug_fd < 0) {
        perror("debug_init: socket");
        return -1;
    }

    memset(&debug_target, 0, sizeof(debug_target));
    debug_target.sin_family = AF_INET;
    debug_target.sin_port = htons(remote_port > 0 ? remote_port : DEBUG_DEFAULT_PORT);

    if (inet_pton(AF_INET, remote_ip, &debug_target.sin_addr) <= 0) {
        perror("debug_init: invalid remote_ip");
        close(debug_fd);
        debug_fd = -1;
        return -1;
    }

    debug_enabled = 1;
    return 0;
}

void debug_send(const char *msg)
{
    if (!debug_enabled || debug_fd < 0 || !msg)
        return;

    sendto(debug_fd, msg, strlen(msg), 0,
           (struct sockaddr *)&debug_target, sizeof(debug_target));
}

void debug_enable(int enable)
{
    debug_enabled = enable;
}

int debug_set_target(const char *remote_ip, int remote_port)
{
    if (!remote_ip || strlen(remote_ip) == 0)
        return -1;

    memset(&debug_target, 0, sizeof(debug_target));
    debug_target.sin_family = AF_INET;
    debug_target.sin_port = htons(remote_port > 0 ? remote_port : DEBUG_DEFAULT_PORT);

    if (inet_pton(AF_INET, remote_ip, &debug_target.sin_addr) <= 0) {
        perror("debug_set_target: invalid remote_ip");
        return -1;
    }

    return 0;
}

void debug_cleanup(void)
{
    debug_enabled = 0;
    if (debug_fd >= 0) {
        close(debug_fd);
        debug_fd = -1;
    }
}
