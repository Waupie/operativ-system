#include "cmd_history.h"
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/string.h>

struct cmd_entry {
    char text[PROC_BUF_SIZE];
    struct list_head list;
};

static LIST_HEAD(cmd_history);

void cmd_history_add(const char *text)
{
    struct cmd_entry *entry = kmalloc(sizeof(*entry), GFP_KERNEL);
    if (entry) {
        strncpy(entry->text, text, PROC_BUF_SIZE - 1);
        entry->text[PROC_BUF_SIZE - 1] = '\0';
        list_add_tail(&entry->list, &cmd_history);
    }
}

int cmd_history_get(char *buf, int buf_size)
{
    struct cmd_entry *entry;
    int len = 0;

    list_for_each_entry(entry, &cmd_history, list) {
        len += snprintf(buf + len, buf_size - len, "%s\n", entry->text);
        if (len >= buf_size - 1)
            break;
    }
    return len;
}

void cmd_history_cleanup(void)
{
    struct cmd_entry *entry, *tmp;
    list_for_each_entry_safe(entry, tmp, &cmd_history, list) {
        list_del(&entry->list);
        kfree(entry);
    }
}