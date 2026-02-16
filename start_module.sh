#!/bin/bash
# filepath: /home/waup/Documents/github/operativ-system/start_module.sh

set -e

echo "Compiling..."
make

echo "Loading kernel module..."
sudo insmod my_module.ko

echo "Showing last kernel messages:"
sudo dmesg | tail

echo "Module loaded."