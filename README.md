# Operativ-System

A Linux kernel module providing an in-memory key-value store (hashtable), with authenticated user-space remote access via TCP sockets, debug message forwarding over UDP, and a daemon for backup/restore.

## Architecture

```
Remote Machine                        User Space (daemon)                Kernel Space
┌──────────┐    TCP port 5555      ┌───────────────────┐              ┌──────────────┐
│  netcat   │ ─ AUTH + command ─▶  │  net_server        │ ──write──▶ │  /proc/ht     │
│  client   │ ◀── AUTH/result ─── │  (PAM + per conn)  │ ◀─read───  │  (kvstore.c)  │
└──────────┘      response         └───────────────────┘              └──────────────┘
                                        │                                    │
                                        │ debug msgs (UDP port 6666)   SIGUSR1 signal
                                        ▼                                    │
                                 ┌───────────────┐                   ┌──────────────┐
                                 │  Debug host    │                   │  daemon main  │
                                 │  (nc -lu 6666) │                   │  thread       │
                                 └───────────────┘                   │  → saves to   │
                                                                      │    disk        │
                                                                      └──────────────┘
```

## Quick Start

### Build, Load Module, and Start Daemon

```bash
chmod +x build_and_run.sh clean_and_remove.sh
./build_and_run.sh
```

This script will:
- Kill any old daemon processes
- Clean previous builds
- Build the kernel module and user-space daemon
- Remove the old kernel module if loaded
- Insert the new kernel module
- Start the daemon (with TCP server on port 5555)

### Clean and Remove Module

```bash
./clean_and_remove.sh
```

This script will:
- Kill any running daemon processes
- Remove the kernel module if loaded
- Clean all build files

## Manual Usage

```bash
make                        # Build everything
sudo insmod my_module.ko    # Load kernel module
./daemon                    # Start daemon (daemonizes itself)
sudo rmmod my_module        # Unload kernel module
make clean                  # Clean build files
sudo dmesg | tail           # View kernel logs
```

## Interacting Locally

Write commands directly to `/proc/ht`:

```bash
# Insert a key-value pair
echo "insert dog baileys" > /proc/ht

# Delete a key
echo "delete dog" > /proc/ht

# Lookup a key (result appears in command history)
echo "lookup dog" > /proc/ht

# Read command history
cat /proc/ht

# Read the full hashtable (raw key-value dump)
cat /proc/hashtable
```

## Interacting Remotely (TCP + Authentication)

Remote TCP access requires an authentication step first:

1. Send `AUTH <linux-username> <linux-password>`
2. Wait for `AUTH OK`
3. Send one key-value command (`insert`, `lookup`, `delete`)

If authentication fails, the server replies with `AUTH FAIL` and closes the connection.

Example sessions with `nc` (recommended pattern):

```bash
# Lookup after auth
(echo "AUTH <user> <pass>"; sleep 0.1; echo "lookup dog") | nc <server-ip> 5555

# Insert after auth
(echo "AUTH <user> <pass>"; sleep 0.1; echo "insert dog baileys") | nc <server-ip> 5555

# Delete after auth
(echo "AUTH <user> <pass>"; sleep 0.1; echo "delete dog") | nc <server-ip> 5555
```

`sleep 0.1` ensures the auth line is processed before the command is sent on the same TCP connection.

Replace `<server-ip>` with the IP of the machine running the module (e.g., `192.168.8.186`).

**Auth line format:** `AUTH <user> <pass>`

**Supported commands (after AUTH OK):** `insert <key> <value>`, `delete <key>`, `lookup <key>`

### Authentication Notes

- Authentication uses Linux PAM (`pam_authenticate` with the `login` service)
- Credentials are validated against accounts on the server machine
- The daemon must be built with PAM (`-lpam -lpam_misc`, already configured in `Makefile`)

## Debug Messages (UDP)

The daemon can send debug messages over UDP to a remote machine. Start the daemon with debug options:

```bash
./daemon --debug-ip 192.168.1.100 --debug-port 6666
```

On the remote machine, listen for debug messages:

```bash
nc -lu 6666
```

You'll see messages like:
```
[REMOTE] from 192.168.8.50:43210 cmd: insert dog baileys
[DAEMON] hashtable saved to /var/tmp/hashtable_backup.txt
```

### Daemon Options

| Flag | Description |
|---|---|
| `-d, --debug-ip IP` | Enable debug messages to this remote IP |
| `-p, --debug-port PORT` | Debug UDP port (default: 6666) |
| `-n, --no-daemon` | Run in foreground (don't daemonize) |
| `-h, --help` | Show help |

## Proc Interfaces

| Path | Read | Write | Purpose |
|---|---|---|---|
| `/proc/ht` | Command history (insert/delete log) | Execute commands (`insert`, `delete`, `lookup`) | Main command interface |
| `/proc/hashtable` | Raw key-value dump | — | Live view of hashtable contents |
| `/proc/daemonpid` | Current daemon PID | Set daemon PID | Kernel ↔ daemon communication |

## Daemon Process

- Double-forks to become a background daemon
- Registers its PID with the kernel via `/proc/daemonpid`
- Restores hashtable from `/var/tmp/hashtable_backup.txt` on startup
- Runs a TCP server thread (port 5555) for remote access
- On `SIGUSR1` from the kernel (triggered by insert/delete), saves the hashtable to disk

## Project Structure

```
├── build_and_run.sh              # Build and start everything
├── clean_and_remove.sh           # Stop and clean everything
├── Makefile                      # Build kernel module + daemon
├── src/
│   ├── kernel/
│   │   ├── main_module.c         # Module init/cleanup, proc entries
│   │   ├── hashtable_module.c/h  # Hashtable implementation (FNV-1a)
│   │   ├── kvstore.c/h           # /proc/ht read/write + command processing
│   │   ├── daemon_module.c/h     # Signal daemon, /proc/hashtable, /proc/daemonpid
│   │   └── kvstore_commands.h    # Command history structures
│   └── user/
│       ├── daemon.c/h            # User-space daemon (backup/restore + main loop)
│       ├── net_server.c/h        # TCP server for remote access (port 5555)
│       └── debug_net.c/h         # UDP debug message sender (port 6666)
└── tests/
    └── test_hashtable.c          # Hashtable unit tests
```

## Notes

- Scripts must be executable: `chmod +x build_and_run.sh clean_and_remove.sh`
- Some commands require root privileges (use `sudo`)
- The TCP server spawns a thread per connection with a 5-second timeout
- Debug message sending is configurable at runtime (no recompile needed)