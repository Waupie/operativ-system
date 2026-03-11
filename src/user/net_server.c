#include "net_server.h"
#include "debug_net.h"

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

static volatile int server_running = 1;
static int server_fd = -1;

/* Per-client connection info passed to the handler thread */
typedef struct {
    int fd;
    char addr[INET_ADDRSTRLEN];
    int port;
} client_info;

/**
 * Forward a command string to the kernel via /proc/ht.
 * For insert/delete: write to /proc/ht.
 * For lookup: write to /proc/ht, then read the response.
 */
static int forward_to_proc(const char *cmd, char *response, size_t resp_len)
{
    int fd;
    ssize_t n;
    char clean[NET_BUF_SIZE];

    /* Strip trailing newline/whitespace */
    strncpy(clean, cmd, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    clean[strcspn(clean, "\r\n")] = '\0';

    /* For lookup: search /proc/hashtable directly instead of going through /proc/ht */
    if (strncmp(clean, "lookup", 6) == 0) {
        char key[64];
        if (sscanf(clean, "lookup %63s", key) != 1) {
            snprintf(response, resp_len, "ERROR: missing key\n");
            return -1;
        }
        fd = open("/proc/hashtable", O_RDONLY);
        if (fd < 0) {
            snprintf(response, resp_len, "ERROR: cannot open /proc/hashtable: %s\n", strerror(errno));
            return -1;
        }
        char buf[4096];
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n <= 0) {
            snprintf(response, resp_len, "Not found\n");
            return 0;
        }
        buf[n] = '\0';
        /* Parse lines "key value\n" to find our key */
        char *line = buf;
        while (line && *line) {
            char k[64], v[64];
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            if (sscanf(line, "%63s %63s", k, v) == 2 && strcmp(k, key) == 0) {
                snprintf(response, resp_len, "Lookup on key: %s, gave value: %s\n", k, v);
                return 0;
            }
            line = nl ? nl + 1 : NULL;
        }
        snprintf(response, resp_len, "Not found\n");
        return 0;
    }

    /* Validate command before forwarding */
    char verb[16];
    if (sscanf(clean, "%15s", verb) != 1 ||
        (strcmp(verb, "insert") != 0 && strcmp(verb, "delete") != 0)) {
        snprintf(response, resp_len, "ERROR: unknown command '%s'. Use: insert, delete, lookup\n", verb);
        return -1;
    }

    /* For insert/delete: write to /proc/ht */
    fd = open("/proc/ht", O_WRONLY);
    if (fd < 0) {
        snprintf(response, resp_len, "ERROR: cannot open /proc/ht: %s\n", strerror(errno));
        return -1;
    }
    n = write(fd, clean, strlen(clean));
    close(fd);
    if (n < 0) {
        snprintf(response, resp_len, "ERROR: write to /proc/ht failed: %s\n", strerror(errno));
        return -1;
    }

    snprintf(response, resp_len, "OK: %s\n", clean);
    return 0;
}

/**
 * Thread handler for a single client connection.
 */
static void *handle_client(void *arg)
{
    client_info *ci = (client_info *)arg;
    char buf[NET_BUF_SIZE];
    char response[NET_BUF_SIZE];
    char debug_msg[NET_BUF_SIZE];
    ssize_t n;

    /* Set a timeout so a stuck client doesn't block this thread forever */
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(ci->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(ci->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    /* Read the command from the client */
    memset(buf, 0, sizeof(buf));
    n = read(ci->fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';

        /* Send debug message about the incoming request */
        snprintf(debug_msg, sizeof(debug_msg), "[REMOTE] from %s:%d cmd: %.256s",
                 ci->addr, ci->port, buf);
        debug_send(debug_msg);

        /* Process the command */
        memset(response, 0, sizeof(response));
        forward_to_proc(buf, response, sizeof(response));

        /* Send response back */
        write(ci->fd, response, strlen(response));
    }

    close(ci->fd);
    free(ci);
    return NULL;
}

void *net_server_run(void *arg)
{
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int client_fd;

    (void)arg;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("net_server: socket");
        return NULL;
    }

    /* Allow address reuse */
    int optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(KVSTORE_PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("net_server: bind");
        close(server_fd);
        server_fd = -1;
        return NULL;
    }

    if (listen(server_fd, 5) < 0) {
        perror("net_server: listen");
        close(server_fd);
        server_fd = -1;
        return NULL;
    }

    /* Set a timeout on accept so we can check server_running periodically */
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (server_running) {
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; /* timeout, check server_running */
            }
            perror("net_server: accept");
            continue;
        }

        /* Allocate client info and spawn a handler thread */
        client_info *ci = malloc(sizeof(client_info));
        if (!ci) {
            close(client_fd);
            continue;
        }
        ci->fd = client_fd;
        inet_ntop(AF_INET, &client_addr.sin_addr, ci->addr, sizeof(ci->addr));
        ci->port = ntohs(client_addr.sin_port);

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, ci) != 0) {
            perror("net_server: pthread_create");
            close(client_fd);
            free(ci);
        } else {
            pthread_detach(tid); /* auto-cleanup when thread finishes */
        }
    }

    close(server_fd);
    server_fd = -1;
    return NULL;
}

void net_server_stop(void)
{
    server_running = 0;
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
}
