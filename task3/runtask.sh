# ----------------
# input validation
# ----------------
if [ "$#" -lt 2 ]; then
	echo "Usage: $0 <program_name> <cpu1> [cpu2 ...]"
	exit 1
fi

# ------------
# read in args
# ------------
program_name="$1"
shift

cpus=("$@")
cpu_list=$(printf ",%s" "${cpus[@]}")
cpu_list=${cpu_list:1}

# --------------------------------
# execute prgram on specified cpus
# --------------------------------
echo "Running program: ${program_name}"
echo "CPUs: $cpu_list"

taskset -c "$cpu_list" ./"$program_name"
