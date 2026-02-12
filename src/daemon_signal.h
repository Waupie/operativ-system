#ifndef DAEMON_SIGNAL_H
#define DAEMON_SIGNAL_H

#include <linux/types.h>

void signal_daemon(void);
void set_daemon_pid(pid_t pid);
pid_t get_daemon_pid(void);

#endif