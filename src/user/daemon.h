#ifndef DAEMON_H
#define DAEMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>
#include <getopt.h>

static volatile sig_atomic_t save_flag = 0;

void handle_signal(int sig);
void write_pid_to_proc(void);
void save_hashtable(void);
void daemonize(void);
void restore_hashtable(void);

#endif