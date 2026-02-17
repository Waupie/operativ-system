#include "daemon.h"

void handle_signal(int sig) {
    save_flag = 1;
}

void write_pid_to_proc() {
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
}

void daemonize()
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
            (void)system(cmd);
        }
    }
    fclose(backup);
}

int main(void)
{
    daemonize();

    write_pid_to_proc();

    restore_hashtable();

    while (1) {
        pause(); //wait for signal

        if (save_flag) {
            save_hashtable();
            save_flag = 0;
        }
    }
    return 0;
}