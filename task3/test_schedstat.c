/******************************************************************************
 * test_schedstat.c
 *
 * A comprehensive test harness to check a modified /proc/<PID>/schedstat
 * that now includes a CPU mask in brackets for each process's scheduling epoch.
 *
 * Author: Your Name (Adapt as needed)
 *
 * Build:   gcc -o test_schedstat test_schedstat.c -lpthread
 * Run:     sudo ./test_schedstat
 *
 * This program performs the following:
 *
 * 1) Reads the schedstat file for the current process (/proc/self/schedstat)
 *    and parses out:
 *        - total CPU time (ns)
 *        - total runqueue wait time (ns)
 *        - number of timeslices
 *        - CPU mask in current epoch (between square brackets)
 *
 * 2) Performs CPU-bound work on one CPU, re-checks schedstat, and verifies
 *    that CPU usage/time slices have changed, and that the CPU mask includes
 *    at least the CPU used.
 *
 * 3) Creates another thread bound to a different CPU (if available), does more
 *    work, and re-checks schedstat to ensure the CPU mask includes both CPUs.
 *
 * 4) Waits >10 seconds to trigger a new epoch. Upon re-check, we expect the
 *    CPU mask to have been cleared, as per the new epoch logic you have added.
 *
 * Adjust the timings or loop workloads to suit your environment and kernel.
 ******************************************************************************/

#define _GNU_SOURCE  // Ensure GNU extensions are exposed

#include <sched.h>   // For CPU_ZERO, CPU_SET, etc.
#include <pthread.h> // For pthread_* functions (with GNU extensions)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>

/* A simple struct to hold the values parsed from /proc/self/schedstat. */
typedef struct {
    unsigned long long cpu_time_ns;      // total time on CPU
    unsigned long long runqueue_ns;      // total time waiting in runqueue
    unsigned long long timeslices;       // number of timeslices on current CPU
    char               used_cpus[256];   // bracketed CPU list (epoch mask)
} schedstat_t;

/* Parse /proc/self/schedstat to get the four fields.
 * Return 0 on success, -1 on failure. */
int parse_schedstat(schedstat_t *stat)
{
    FILE *fp = fopen("/proc/self/schedstat", "r");
    if (!fp) {
        perror("fopen(/proc/self/schedstat)");
        return -1;
    }

    /*
     * Typical original /proc/<PID>/schedstat lines might look like:
     *   <cpu_time_ns> <runqueue_ns> <timeslices>
     * But with your modifications, there's now a fourth field in square brackets:
     *   <cpu_time_ns> <runqueue_ns> <timeslices> [0,1]
     * or possibly [0] or [] if no CPUs were used in the current epoch.
     *
     * We need to read the entire line, then parse accordingly.
     */
    char buffer[512];
    if (!fgets(buffer, sizeof(buffer), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* The expected format:
       <cpu_time_ns> <runqueue_ns> <timeslices> [comma-separated-CPU-list]
       We can parse with sscanf if we carefully handle the bracketed part. */
    unsigned long long cpu_time, rq_time, slices;
    char bracket_buf[256];

    /* 
     * One approach: First scan for the three numeric fields plus the bracketed
     * substring. The bracketed substring might contain commas and digits, so
     * use a format specifier that reads until the trailing newline.
     */
    int fields = sscanf(buffer, "%llu %llu %llu %255[^\n]",
                        &cpu_time, &rq_time, &slices, bracket_buf);
    if (fields < 4) {
        fprintf(stderr, "Unexpected schedstat format: %s\n", buffer);
        return -1;
    }

    /* bracket_buf should contain something like "[0,1]" or "[]".
       We'll store it as-is into stat->used_cpus. */
    stat->cpu_time_ns = cpu_time;
    stat->runqueue_ns = rq_time;
    stat->timeslices  = slices;
    strncpy(stat->used_cpus, bracket_buf, sizeof(stat->used_cpus) - 1);
    stat->used_cpus[sizeof(stat->used_cpus) - 1] = '\0';

    return 0;
}

/* Utility: do some CPU-bound work for `seconds` seconds. */
void burn_cpu_for_seconds(int seconds)
{
    fprintf(stderr, "Burning CPU for %d second(s)...\n", seconds);

    /* We'll do a simple busy-loop that checks the time. 
     * This is not perfectly accurate, but sufficient for demonstration. */
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    long long start_ns = (long long)start.tv_sec * 1000000000LL + start.tv_nsec;

    long long elapsed_ns = 0;
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        long long now_ns = (long long)now.tv_sec * 1000000000LL + now.tv_nsec;
        elapsed_ns = now_ns - start_ns;
        if (elapsed_ns >= (long long)seconds * 1000000000LL) {
            break;
        }
        /* Simple arithmetic to keep CPU busy. */
        volatile unsigned long dummy = 0;
        for (int i = 0; i < 100000; i++) {
            dummy += i;
        }
    }
}

/* Thread function that can be pinned to a CPU. */
static void *thread_fn(void *arg)
{
    int cpu_id = *(int *)arg;

    /* Attempt to pin this thread to cpu_id. */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) != 0) {
        fprintf(stderr, "Warning: could not set thread affinity to CPU %d\n", cpu_id);
        perror("pthread_setaffinity_np");
    } else {
        fprintf(stderr, "Thread pinned to CPU %d.\n", cpu_id);
    }

    /* Do some CPU work for 2 seconds. */
    burn_cpu_for_seconds(2);
    return NULL;
}

int main(void)
{
    /* 1. Read initial schedstat. */
    schedstat_t before;
    if (parse_schedstat(&before) == -1) {
        fprintf(stderr, "Error: Could not parse /proc/self/schedstat.\n");
        return 1;
    }
    printf("Initial /proc/self/schedstat:\n");
    printf("  CPU time (ns)  = %llu\n", before.cpu_time_ns);
    printf("  RQ time (ns)   = %llu\n", before.runqueue_ns);
    printf("  Timeslices     = %llu\n", before.timeslices);
    printf("  used_cpus      = %s\n\n", before.used_cpus);

    /* 2. Burn CPU for a while on the current CPU, then re-check. */
    burn_cpu_for_seconds(2);

    schedstat_t after;
    if (parse_schedstat(&after) == -1) {
        fprintf(stderr, "Error: Could not parse /proc/self/schedstat after 1st burn.\n");
        return 1;
    }
    printf("\nAfter 2s CPU-bound on one thread:\n");
    printf("  CPU time (ns)  = %llu (was %llu)\n", after.cpu_time_ns, before.cpu_time_ns);
    printf("  RQ time (ns)   = %llu (was %llu)\n", after.runqueue_ns, before.runqueue_ns);
    printf("  Timeslices     = %llu (was %llu)\n", after.timeslices, before.timeslices);
    printf("  used_cpus      = %s\n\n", after.used_cpus);

    if (after.cpu_time_ns <= before.cpu_time_ns) {
        fprintf(stderr, "WARNING: CPU time did not increase as expected!\n");
    }

    /* 3. Create another thread pinned to a different CPU (if possible). */
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs < 2) {
        fprintf(stderr, "System has <2 CPUs; skipping multi-CPU test.\n");
    } else {
        int second_cpu = 1; // We'll pick CPU #1 for the second thread.

        pthread_t tid;
        pthread_create(&tid, NULL, thread_fn, &second_cpu);

        /* Meanwhile, main thread also does some CPU work on CPU 0. */
        cpu_set_t main_cpuset;
        CPU_ZERO(&main_cpuset);
        CPU_SET(0, &main_cpuset);
        if (pthread_setaffinity_np(pthread_self(), sizeof(main_cpuset), &main_cpuset) != 0) {
            fprintf(stderr, "Warning: could not set main thread affinity to CPU 0\n");
            perror("pthread_setaffinity_np");
        }

        burn_cpu_for_seconds(2);

        pthread_join(tid, NULL);

        schedstat_t after_multi;
        if (parse_schedstat(&after_multi) == -1) {
            fprintf(stderr, "Error: Could not parse /proc/self/schedstat after multi-CPU test.\n");
            return 1;
        }
        printf("\nAfter multi-CPU test (main on CPU0, thread on CPU1):\n");
        printf("  CPU time (ns)  = %llu\n", after_multi.cpu_time_ns);
        printf("  RQ time (ns)   = %llu\n", after_multi.runqueue_ns);
        printf("  Timeslices     = %llu\n", after_multi.timeslices);
        printf("  used_cpus      = %s\n\n", after_multi.used_cpus);
        printf("Expected CPU mask to include [0,1] or similar.\n");
    }

    /* 4. Wait >10 seconds to force an epoch change, then re-check. */
    printf("Sleeping 12 seconds to trigger new epoch...\n");
    sleep(12);

    schedstat_t after_sleep;
    if (parse_schedstat(&after_sleep) == -1) {
        fprintf(stderr, "Error: Could not parse /proc/self/schedstat after sleep.\n");
        return 1;
    }
    printf("\nAfter sleeping >10s to force new epoch:\n");
    printf("  CPU time (ns)  = %llu\n", after_sleep.cpu_time_ns);
    printf("  RQ time (ns)   = %llu\n", after_sleep.runqueue_ns);
    printf("  Timeslices     = %llu\n", after_sleep.timeslices);
    printf("  used_cpus      = %s\n", after_sleep.used_cpus);
    printf("Check if used_cpus is empty or significantly changed, as the epoch should reset it.\n");

    printf("\nTest complete.\n");
    return 0;
}
