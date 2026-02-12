#ifndef CMD_HISTORY_H
#define CMD_HISTORY_H

#define PROC_BUF_SIZE 512

void cmd_history_add(const char *text);
int cmd_history_get(char *buf, int buf_size);
void cmd_history_cleanup(void);

#endif