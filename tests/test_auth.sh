#!/bin/bash

SERVER="127.0.0.1"
PORT=5555

echo "=== TEST: Wrong Password ==="
(echo "AUTH fakeuser wrongpass"; sleep 0.1; echo "lookup dog") | nc $SERVER $PORT

echo "=== TEST: No AUTH ==="
(echo "lookup dog") | nc $SERVER $PORT

echo "=== TEST: Correct AUTH ==="
read -p "User: " USER
read -s -p "Password: " PASS
echo ""

(echo "AUTH $USER $PASS"; sleep 0.1; echo "insert dog baileys") | nc $SERVER $PORT
(echo "AUTH $USER $PASS"; sleep 0.1; echo "lookup dog") | nc $SERVER $PORT