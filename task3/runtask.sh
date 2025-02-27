# ----------------
# input validation
# ----------------
if [ "$#" -lt 2 ]; then
	echo "Usage: $0 <PID> <cpu1> [cpu2 ...]"
	exit 1
fi

# ------------
# read in args
# ------------
PID="$1"
shift

cpus=("$@")
cpu_list=$(printf ",%s" "${cpus[@]}")
cpu_list=${cpu_list:1}

# --------------------------------
# execute prgram on specified cpus
# --------------------------------
echo "Checking pid $PID"
echo "CPUs: $cpu_list"

# run task if it is not already running
if [ ! -d /proc/$PID ]; then
	echo "Process $PID not found."
	./infinite_program &
	PID=$!
	echo "Launched infinite program with PID: $PID"
fi

taskset -c "$cpu_list" "$PID"

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

# Clean up: kill the infinite process.
kill -9 $PID
echo "Test complete."