#define _GNU_SOURCE
#include "daemon.h"
#include "net_server.h"
#include "debug_net.h"

static pthread_t net_thread;

void handle_signal(int sig) {
    if (sig == SIGUSR1)
        save_flag = 1;
}

void write_pid_to_proc(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d\n", getpid());
    int fd = open("/proc/daemonpid", O_WRONLY | O_TRUNC);
    if (fd < 0) {
        perror("Failed to open /proc/daemonpid");
        return;
    }
    if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) {
        perror("Failed to write PID to /proc/daemonpid");
    }
    close(fd);
}

void save_hashtable(void)
{
    FILE *fp = fopen("/proc/hashtable", "r");
    if(!fp)
    {
        perror("Failed to open /proc/hashtable in daemon");
        return;
    }
    FILE *backup = fopen("/var/tmp/hashtable_backup.txt", "w");
    if(!backup)
    {
        perror("Failed to open backup file in daemon");
        fclose(fp);
        return;
    }
    char buf[512];
    while(fgets(buf, sizeof(buf), fp))
    {
        fputs(buf, backup);
    }
    fclose(fp);
    fclose(backup);

    debug_send("[DAEMON] hashtable saved to /var/tmp/hashtable_backup.txt");
}

void daemonize(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }
    if (pid > 0) {
        exit(0); // parent exits
    }

    if (setsid() < 0) {
        perror("setsid failed");
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction failed");
        exit(1);
    }

    pid = fork();
    if (pid < 0) {
        perror("Second fork failed");
        exit(1);
    }

    if (pid > 0) {
        exit(0); // first child exits
    }
    umask(0);
    if (chdir("/") < 0) {
        perror("chdir failed");
        exit(1);
    }
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--) {
        close(x);
    }

    /* Reopen stdin/stdout/stderr to /dev/null */
    open("/dev/null", O_RDONLY); /* stdin  */
    open("/dev/null", O_WRONLY); /* stdout */
    open("/dev/null", O_WRONLY); /* stderr */
}

void restore_hashtable(void)
{
    FILE *backup = fopen("/var/tmp/hashtable_backup.txt", "r");
    if (!backup) {
        // No backup file, nothing to restore
        return;
    }
    char buf[512];
    while (fgets(buf, sizeof(buf), backup)) {
        char key[256], value[256];
        if (sscanf(buf, "%255s %255s", key, value) == 2) {
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "echo \"insert %s %s\" > /proc/ht", key, value);
            int ret = system(cmd);
            if (ret != 0) {
                perror("system() failed");
            }
        }
    }
    fclose(backup);
    debug_send("[DAEMON] hashtable restored from backup");
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "  -d, --debug-ip IP     Enable debug messages to remote IP\n"
        "  -p, --debug-port PORT Debug UDP port (default: 6666)\n"
        "  -n, --no-daemon       Run in foreground (don't daemonize)\n"
        "  -h, --help            Show this help\n",
        prog);
}

int main(int argc, char *argv[])
{
    const char *debug_ip = NULL;
    int debug_port = DEBUG_DEFAULT_PORT;
    int foreground = 0;

    static struct option long_opts[] = {
        {"debug-ip",   required_argument, NULL, 'd'},
        {"debug-port", required_argument, NULL, 'p'},
        {"no-daemon",  no_argument,       NULL, 'n'},
        {"help",       no_argument,       NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "d:p:nh", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'd':
                debug_ip = optarg;
                break;
            case 'p':
                debug_port = atoi(optarg);
                break;
            case 'n':
                foreground = 1;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                exit(0);
        }
    }

    if (!foreground)
        daemonize();

    /* Initialize debug message sender if configured */
    if (debug_ip) {
        if (debug_init(debug_ip, debug_port) == 0) {
            debug_send("[DAEMON] started, debug messages enabled");
        }
    }

    write_pid_to_proc();
    restore_hashtable();

    /* Start the tpc network server in a separate thread */
    if (pthread_create(&net_thread, NULL, net_server_run, NULL) != 0) {
        perror("Failed to start network server thread");
    } else {
        debug_send("[DAEMON] network server started on UDP port 5555");
    }

    /* Main daemon loop: wait for signals */
    while (1) {
        pause(); /* wait for signal */

        if (save_flag) {
            save_hashtable();
            save_flag = 0;
        }
    }

    /* Cleanup (unreachable, but good practice) */
    net_server_stop();
    pthread_join(net_thread, NULL);
    debug_cleanup();

    return 0;
}