#!/bin/bash
# filepath: /home/waup/Documents/github/operativ-system/stop_module.sh

set -e

echo "Unloading kernel module..."
sudo rmmod my_module

echo "Cleaning build files..."
make clean

echo "Module unloaded and cleaned."