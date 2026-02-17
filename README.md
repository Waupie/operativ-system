
## operativ-system

### Quick Start

#### Build, Load Module, and Start Daemon
```bash
./build_and_run.sh
```
This script will:
- Clean previous builds
- Build the kernel module and daemon
- Remove the old kernel module if loaded
- Insert the new kernel module
- Start the daemon in the background

#### Clean and Remove Module
```bash
./clean_and_remove.sh
```
This script will:
- Remove the kernel module if loaded
- Clean all build files

### Manual Commands

- `make` to compile
- `sudo insmod my_module.ko` to load the kernel module
- `sudo rmmod my_module` to unload the kernel module
- `make clean` to clean build files
- `sudo dmesg` or `sudo dmesg | tail` to view kernel logs

### Interacting with the Module

- Write commands to `/proc/ht` (e.g., `echo "lookup" | sudo tee /proc/ht`)
- Read from `/proc/ht` (e.g., `cat /proc/ht`)

### Notes
- Scripts must be executable: `chmod +x build_and_run.sh clean_and_remove.sh`
- Some commands require root privileges (use `sudo`)

---
For more details, see the source files in `src/` and tests in `tests/`.

