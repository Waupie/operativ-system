#include "daemon_signal.h"
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/kernel.h>

static pid_t daemon_pid = -1;

void set_daemon_pid(pid_t pid)
{
    daemon_pid = pid;
}

pid_t get_daemon_pid(void)
{
    return daemon_pid;
}

void signal_daemon(void)
{
    struct pid *pid_struct;
    struct task_struct *task;

    if (daemon_pid <= 0)
        return;

    pid_struct = find_get_pid(daemon_pid);
    if (!pid_struct)
        return;

    task = pid_task(pid_struct, PIDTYPE_PID);
    if (!task)
        return;

    send_sig_info(SIGUSR1, SEND_SIG_PRIV, task);
    printk(KERN_INFO "Sent SIGUSR1 to daemon PID %d\n", daemon_pid);
}