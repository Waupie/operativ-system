#include "daemon.h"

void handle_signal(int sig) {
    save_flag = 1;
}

void write_pid_to_proc() {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d\n", getpid());
    int fd = open("/proc/daemonpid", O_WRONLY);
    if (fd < 0) {
        perror("Failed to open /proc/daemonpid");
        return;
    }
    write(fd, buf, strlen(buf));
    close(fd);
}

void save_hashtable(void)
{
    FILE *fp = fopen("/proc/ht", "r");
    if(!fp)
    {
        perror("Failed to open /proc/ht in daemon");
        return;
    }
    FILE *backup = fopen("/tmp/hashtable_backup.txt", "w");
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

void daemonize() {
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
}

int main(void)
{
    daemonize();

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction failed");
        exit(1);
    }

    write_pid_to_proc();

    while (1) {
        pause(); //wait for signal

        if (save_flag) {
            save_hashtable();
            save_flag = 0;
        }
    }
    return 0;
}