#!/bin/bash
# test_schedstat.sh
# This script tests the new schedstat functionality.
# It starts a CPU-bound process, then for each CPU combination it:
#   - Sets CPU affinity. For multi-CPU combinations (with commas), it sets affinity
#     to each CPU individually with a 2-second pause in between.
#   - Monitors the schedstat output for 10 seconds to observe the changes.

echo "Starting a CPU-bound process..."
( while true; do :; done ) &
PID=$!
echo "Process started with PID: $PID"

# Give the process a couple of seconds to start running
sleep 2

echo "Monitoring /proc/$PID/schedstat and changing CPU affinity..."

CPU_COMBS=(0 1 2 3 0,1 0,2 0,3 1,2 1,3 2,3 0,1,2 0,1,3 0,2,3 1,2,3 0,1,2,3)

for CPU in "${CPU_COMBS[@]}"; do
    echo "Testing CPU combination: $CPU"
    
    # If the combination has a comma, split and apply each CPU individually.
    if [[ "$CPU" == *,* ]]; then
        IFS=',' read -ra cpus <<< "$CPU"
        for single_cpu in "${cpus[@]}"; do
            echo "Setting affinity to CPU $single_cpu for process $PID..."
            taskset -cp "$single_cpu" "$PID"
            sleep 2
        done
    else
        echo "Setting affinity to CPU $CPU for process $PID..."
        taskset -cp "$CPU" "$PID"
        sleep 2
    fi

    # Monitor schedstat for 10 seconds after applying affinity
    END=$((SECONDS + 10))
    while [ $SECONDS -lt $END ]; do
        if [ -e /proc/$PID/schedstat ]; then
            SCHEDSTAT=$(cat /proc/$PID/schedstat 2>/dev/null)
            echo "Time $SECONDS sec - schedstat: $SCHEDSTAT"
        else
            echo "/proc/$PID/schedstat not available."
            break
        fi
        sleep 2
    done
done

# Cleanup: terminate the background process
kill "$PID"
echo "Process $PID terminated. Test complete."