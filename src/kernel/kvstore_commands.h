#ifndef KVSTORE_COMMANDS_H
#define KVSTORE_COMMANDS_H

#include <linux/list.h>

#define PROC_BUF_SIZE 512

struct cmd_entry {
    char text[PROC_BUF_SIZE];
    struct list_head list;
};

extern struct list_head cmd_history;

#endif // KVSTORE_COMMANDS_H
