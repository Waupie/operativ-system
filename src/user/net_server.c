#include "net_server.h"

static volatile int server_running = 1;
static int server_fd = -1;

/**
 * Forward a command string to the kernel via /proc/ht.
 * For insert/delete: write to /proc/ht.
 * For lookup: write to /proc/ht, then read the response.
 */
int forward_to_proc(const char *cmd, char *response, size_t resp_len)
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
void *handle_client(void *arg)
{
    client_info *ci = (client_info *)arg;
    char buf[NET_BUF_SIZE];
    char response[NET_BUF_SIZE];
    char debug_msg[NET_BUF_SIZE];
    ssize_t n;

    char username[64];
    char password[64];

    /* Set timeout */
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(ci->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(ci->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    /* ---- STEP 1: READ AUTH LINE ---- */
    memset(buf, 0, sizeof(buf));
    n = read(ci->fd, buf, sizeof(buf) - 1);
    if (n <= 0)
        goto cleanup;

    buf[n] = '\0';

    if (sscanf(buf, "AUTH %63s %63s", username, password) != 2) {
        if(write(ci->fd, "ERROR: expected AUTH <user> <pass>\n", 35) < 0)
        {
            perror("Auth error write");
        }
        goto cleanup;
    }

    if (authenticate_user(username, password) != 0) {
        if (write(ci->fd, "AUTH FAIL\n", 10) < 0) {
            perror("Auth fail write");
        }
        goto cleanup;
    }

    if (write(ci->fd, "AUTH OK\n", 8) < 0) {
        perror("Auth ok write");
        goto cleanup;
    }

    /* ---- STEP 2: READ COMMAND ---- */
    memset(buf, 0, sizeof(buf));
    n = read(ci->fd, buf, sizeof(buf) - 1);
    if (n <= 0)
        goto cleanup;

    buf[n] = '\0';

    /* Debug message */
    snprintf(debug_msg, sizeof(debug_msg),
             "[REMOTE] from %s:%d user:%s cmd: %.256s",
             ci->addr, ci->port, username, buf);

    debug_send(debug_msg);

    /* Forward command */
    memset(response, 0, sizeof(response));
    forward_to_proc(buf, response, sizeof(response));

    if(write(ci->fd, response, strlen(response)) < 0) {
        perror("response write");
        goto cleanup;
    }

cleanup:
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

static int pam_password_conv(int num_msg, const struct pam_message **msg,
                             struct pam_response **resp, void *appdata_ptr)
{
    struct pam_response *replies;
    const char *password = appdata_ptr;
    int i;

    if (!msg || !resp || !password || num_msg <= 0)
        return PAM_CONV_ERR;

    replies = calloc((size_t)num_msg, sizeof(*replies));
    if (!replies)
        return PAM_CONV_ERR;

    for (i = 0; i < num_msg; i++) {
        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
        case PAM_PROMPT_ECHO_ON:
            replies[i].resp = strdup(password);
            if (!replies[i].resp)
                goto fail;
            break;
        case PAM_TEXT_INFO:
        case PAM_ERROR_MSG:
            replies[i].resp = NULL;
            break;
        default:
            goto fail;
        }
    }

    *resp = replies;
    return PAM_SUCCESS;

fail:
    for (i = 0; i < num_msg; i++)
        free(replies[i].resp);
    free(replies);
    return PAM_CONV_ERR;
}

int authenticate_user(const char *user, const char *pass)
{
    pam_handle_t *pamh = NULL;
    struct pam_conv conv = { pam_password_conv, (void *)pass };

    int ret = pam_start("login", user, &conv, &pamh);
    if (ret != PAM_SUCCESS)
        return -1;

    ret = pam_authenticate(pamh, 0);
    if (ret == PAM_SUCCESS)
        ret = pam_acct_mgmt(pamh, 0);

    pam_end(pamh, ret);

    return (ret == PAM_SUCCESS) ? 0 : -1;
}