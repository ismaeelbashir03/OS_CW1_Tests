#!/bin/bash
# test_schedstat.sh
# This script tests the new schedstat functionality.
# It starts a CPU-bound process, monitors its schedstat output,
# then forces a CPU migration to observe changes in the CPU bitmask.

echo "Starting a CPU-bound process..."
( while true; do :; done ) &
PID=$!
echo "Process started with PID: $PID"

# Give the process a couple of seconds to start running
sleep 2

# Monitor schedstat for 35 seconds (over several epochs)
echo "Monitoring /proc/$PID/schedstat for 35 seconds..."
CPU_MIN=0
CPU_MAX=3
CPU_COMBS=(0 1 2 3 0,1 0,2 0,3 1,2 1,3 2,3 0,1,2 0,1,3 0,2,3 1,2,3 0,1,2,3)

# loop for each combination of CPU bitmask
for CPU in ${CPU_COMBS[@]}; do
	echo "Monitoring CPU bitmask: $CPU"
	END=$((SECONDS + 35))
	MIGRATION_DONE=0
	while [ $SECONDS -lt $END ]; do
		if [ -e /proc/$PID/schedstat ]; then
			# Read schedstat and print timestamp and output
			SCHEDSTAT=$(cat /proc/$PID/schedstat 2>/dev/null)
			echo "Time $SECONDS sec - schedstat: $SCHEDSTAT"

			# Force migration after 10 seconds
			if [ $SECONDS -ge 10 ] && [ $MIGRATION_DONE -eq 0 ]; then
				echo "Forcing process $PID onto CPU $CPU..."
				taskset -cp $CPU $PID
				MIGRATION_DONE=1
			fi
		else
			echo "/proc/$PID/schedstat not available."
			break
		fi
		sleep 2
	done
done

# Cleanup: terminate the background process
kill $PID
echo "Process $PID terminated. Test complete."