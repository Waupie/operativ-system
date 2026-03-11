#ifndef DEBUG_NET_H
#define DEBUG_NET_H

#define DEBUG_DEFAULT_PORT 6666

/**
 * Initialize the debug message sender.
 * @param remote_ip  IP address of the remote machine to send debug messages to.
 * @param remote_port  UDP port on the remote machine (0 = use default 6666).
 * @return 0 on success, -1 on error.
 */
int debug_init(const char *remote_ip, int remote_port);

/**
 * Send a debug message to the configured remote target.
 * Does nothing if debug is not enabled/initialized.
 */
void debug_send(const char *msg);

/**
 * Enable or disable debug message sending at runtime.
 */
void debug_enable(int enable);

/**
 * Reconfigure the debug target at runtime (no recompile needed).
 * @param remote_ip  New remote IP address.
 * @param remote_port  New remote UDP port.
 */
int debug_set_target(const char *remote_ip, int remote_port);

/**
 * Clean up and close the debug socket.
 */
void debug_cleanup(void);

#endif /* DEBUG_NET_H */
