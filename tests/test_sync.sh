#!/bin/bash

HT="/proc/ht"

echo "=== TEST: Lock Contention ==="

run_worker() {
    ID=$1

    for i in {1..20}; do
        KEY="sharedkey"   # SAME key → maximum contention

        start=$(date +%s%N)

        echo "insert $KEY val$RANDOM" > $HT

        end=$(date +%s%N)

        duration=$(( (end - start) / 1000000 )) # ms

        echo "[Worker $ID] insert took ${duration} ms"
    done
}

# Start workers simultaneously
for i in {1..10}; do
    run_worker $i &
done

wait

echo "=== DONE ==="