# Operativ-System

This project implements a Linux kernel module providing a persistent hashtable, with user-space interaction and backup/restore support via a daemon process. Networking support is included via Netfilter hooks.

## Quick Start

### Build, Load Module, and Start Daemon
```bash
./build_and_run.sh
```
This script will:
- Clean previous builds
- Build the kernel module and user-space daemon
- Remove the old kernel module if loaded
- Insert the new kernel module
- Start the daemon in the background

### Clean and Remove Module
```bash
./clean_and_remove.sh
```
This script will:
- Remove the kernel module if loaded
- Clean all build files

## Manual Usage

- `make` to compile everything
- `sudo insmod my_module.ko` to load the kernel module
- `sudo rmmod my_module` to unload the kernel module
- `make clean` to clean build files
- `sudo dmesg` or `sudo dmesg | tail` to view kernel logs

## Interacting with the Kernel Module

- Write commands to `/proc/ht` (e.g., `echo "insert foo bar" | sudo tee /proc/ht`)
- Read command history from `/proc/ht` (e.g., `cat /proc/ht`)
- Read the full hashtable from `/proc/hashtable` (for daemon backup)
- Write/read daemon PID to/from `/proc/daemonpid`

## Interacting Over the Network (UDP)

The kernel module supports remote access via UDP. You can send commands to the module from another machine using netcat (nc):

```
echo "insert dog baileys" | nc -u <server-ip> 5555
echo "lookup dog" | nc -u <server-ip> 5555
```

Replace `<server-ip>` with the IP address of the machine running the kernel module (e.g., 192.168.8.186).

This allows you to insert and look up key-value pairs remotely over the network.


## Daemon Process

- The user-space daemon (`daemon`) runs in the background, writes its PID to `/proc/daemonpid`, and listens for signals from the kernel.
- On signal, the daemon saves the hashtable to `/var/tmp/hashtable_backup.txt`.
- On startup, the daemon restores the hashtable from the backup file if it exists.

## Features

- Persistent hashtable in kernel space
- Command history for all non-lookup operations
- User-space daemon for backup/restore
- Netfilter UDP hook for networked access (see source for details)
- Test code in `tests/`

## Notes

- Scripts must be executable: `chmod +x build_and_run.sh clean_and_remove.sh`
- Some commands require root privileges (use `sudo`)

---
For more details, see the source files in `src/` and tests in `tests/`.