#ifndef DAEMON_H
#define DAEMON_H


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "hashtable_module.h"

static volatile sig_atomic_t save_flag = 0;

static void save_hashtable(void/*ht *table, char *filename*/);
void daemonize();
void daemon_func(void);


#endif