#!/bin/bash

HT="/proc/ht"
RAW="/proc/hashtable"

echo "=== TEST: Basic Hashtable Functionality ==="

# Clean start
echo "delete dog" > $HT 2>/dev/null

echo "[1] Insert dog=baileys"
echo "insert dog baileys" > $HT

echo "[2] Lookup dog"
res=$(cat $RAW | grep "^dog ")
if [[ "$res" == "dog baileys" ]]; then
    echo "PASS: lookup works"
else
    echo "FAIL: lookup failed"
fi

echo "[3] Overwrite dog=whiskey"
echo "insert dog whiskey" > $HT

res=$(cat $RAW | grep "^dog ")
if [[ "$res" == "dog whiskey" ]]; then
    echo "PASS: overwrite works"
else
    echo "FAIL: overwrite failed"
fi

echo "[4] Delete dog"
echo "delete dog" > $HT

echo "[5] Lookup deleted key"
res=$(cat $RAW | grep "^dog ")
if [[ -z "$res" ]]; then
    echo "PASS: delete works"
else
    echo "FAIL: delete failed"
fi

echo "=== DONE ==="