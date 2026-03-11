#!/bin/bash

# Exit on error
set -e

# Kill any old daemon processes
if pkill -x daemon 2>/dev/null; then
    echo "Killed old daemon process(es)."
    sleep 1
fi

# Clean previous builds
make clean

# Build kernel module and daemon
make all

# Insert kernel module (remove if already loaded)
MODULE=my_module
if lsmod | grep -q "$MODULE"; then
    echo "Removing existing $MODULE module..."
    sudo rmmod $MODULE
fi

echo "Inserting $MODULE module..."
sudo insmod my_module.ko

echo "Starting daemon..."
./daemon

echo "Build and startup complete."
