#!/bin/bash
# test_schedstat.sh - Test the new epoch tracking in schedstat

# ------------
# read in args
# ------------
core="$1"
shift

# Launch the CPU-bound process in the background.
taskset -c $core cpu_burn &
PID=$!
echo "Launched CPU-bound process with PID: $PID"

# Give the process a moment to start running.
sleep 2

# Monitor the /proc/<PID>/schedstat output over time.
# An epoch is defined as 10 seconds of runtime.
# We'll run this loop for 20 seconds to see how the fourth field changes.
for i in {1..20}; do
    echo "Iteration $i:"
    # Read the schedstat file.
    # The expected format is: "exec_time wait_time timeslices [cpulist]"
    SCHEDSTAT=$(cat /proc/$PID/schedstat 2>/dev/null)
    if [ -z "$SCHEDSTAT" ]; then
        echo "Process $PID has ended."
        break
    fi
    echo "schedstat: $SCHEDSTAT"
    sleep 1
done

# Clean up: kill the cpu_burn process.
kill $PID
echo "Test complete."
