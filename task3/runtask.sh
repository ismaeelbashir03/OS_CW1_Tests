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
			sleep 1
        done
    else
        echo "Setting affinity to CPU $CPU for process $PID..."
        taskset -cp "$CPU" "$PID"
		sleep 1
    fi
	sleep 10

    # Monitor schedstat for 10 seconds after applying affinity
    END=$((SECONDS + 1))
    while [ $SECONDS -lt $END ]; do
        if [ -e /proc/$PID/schedstat ]; then
            SCHEDSTAT=$(cat /proc/$PID/schedstat 2>/dev/null)
            echo "Time $SECONDS sec - schedstat: $SCHEDSTAT"
			## add check to see that the last column is in format [1] (for multiple its in the format [0-1])

			# Extract the CPU information from the last column (enclosed in square brackets)
            CPU_INFO=$(echo "$SCHEDSTAT" | awk '{print $NF}' | tr -d '[]')
            
            # Verify CPU information matches expected CPU affinity
            if [[ "$CPU" == *,* ]]; then
                # For multi-CPU combinations, validate the format with comma or range notation
                # Convert current CPU into expected format for comparison
                EXPECTED_CPUS=$(echo "$CPU" | tr ',' ' ' | tr ' ' '\n' | sort -n | uniq)
                FIRST_CPU=$(echo "$EXPECTED_CPUS" | head -n 1)
                LAST_CPU=$(echo "$EXPECTED_CPUS" | tail -n 1)
                
                # Check if continuous range or comma-separated list
                if [[ $(echo "$EXPECTED_CPUS" | wc -l) -eq $((LAST_CPU - FIRST_CPU + 1)) ]]; then
                    # Continuous range
                    EXPECTED_FORMAT="${FIRST_CPU}-${LAST_CPU}"
                else
                    # Non-continuous range, should be comma-separated
                    EXPECTED_FORMAT=$(echo "$EXPECTED_CPUS" | tr '\n' ',' | sed 's/,$//')
                fi
                
                if [[ "$CPU_INFO" == "$EXPECTED_FORMAT" || "$CPU_INFO" == *,* && "$EXPECTED_FORMAT" == *,* ]]; then
                    echo "✓ CPU info matches expected: [$CPU_INFO]"
                else
                    # For single CPU, just check the number matches
					if [[ "$CPU_INFO" == "$CPU" ]]; then
						echo "✓ CPU info matches expected: [$CPU_INFO]"
					else
						echo "✗ CPU info mismatch: got [$CPU_INFO], expected [$CPU] for CPU setting $CPU"
					fi
                fi
        else
            echo "/proc/$PID/schedstat not available."
            break
        fi
        sleep 1
    done
done

# Cleanup: terminate the background process
kill "$PID"
echo "Process $PID terminated. Test complete."