#!/bin/bash

# Exit on error
set -e

# Kill daemon processes if running
if pkill -x daemon 2>/dev/null; then
    echo "Killed daemon process(es)."
    sleep 1
else
    echo "No daemon process running."
fi

# Remove kernel module if loaded
MODULE=my_module
if lsmod | grep -q "$MODULE"; then
    echo "Removing $MODULE module..."
    sudo rmmod $MODULE
else
    echo "$MODULE module is not loaded."
fi

# Clean build files
make clean

echo "Module removed, daemon stopped, and build cleaned."
