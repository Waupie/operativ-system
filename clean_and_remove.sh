#!/bin/bash

# Exit on error
set -e

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

echo "Module removed and build cleaned."
