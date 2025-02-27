#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

// Utility: read and parse /proc/<pid>/schedstat.
// Expected format: <exec_time_ns> <wait_time_ns> <timeslices> [<cpu_list>]
typedef struct {
    unsigned long long exec_time;
    unsigned long long wait_time;
    unsigned long timeslices;
    char cpu_list[256]; // Adjust buffer size if needed
} schedstat_info;

int read_schedstat(pid_t pid, schedstat_info *info) {
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/schedstat", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    // Read the first three numbers and the rest (the CPU list string)
    // Format: "%llu %llu %lu %[^\n]"
    int ret = fscanf(fp, "%llu %llu %lu %[^\n]",
                     &info->exec_time, &info->wait_time, &info->timeslices,
                     info->cpu_list);
    fclose(fp);
    if (ret < 3) {
        fprintf(stderr, "schedstat format error for PID %d\n", pid);
        return -1;
    }
    return 0;
}

// Test 1: Validate that /proc/<pid>/schedstat contains at least three numbers and a CPU list.
void test_format(void) {
    pid_t pid = getpid();
    schedstat_info info;
    assert(read_schedstat(pid, &info) == 0);
    printf("Test Format: exec_time=%llu, wait_time=%llu, timeslices=%lu, cpu_list=%s\n",
           info.exec_time, info.wait_time, info.timeslices, info.cpu_list);
}

// Test 2: Set affinity to a single CPU and verify that the CPU list shows only that CPU in the expected format.
void test_single_cpu_affinity(void) {
    pid_t pid = getpid();
    cpu_set_t mask;
    CPU_ZERO(&mask);
    // Pin to CPU 0 (adjust if your system does not have CPU0 available)
    CPU_SET(0, &mask);
    if (sched_setaffinity(pid, sizeof(mask), &mask) != 0) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }
    sleep(2); // allow some scheduler ticks
    schedstat_info info;
    assert(read_schedstat(pid, &info) == 0);
    // We expect the CPU list to be exactly "[0]"
    if (strcmp(info.cpu_list, "[0]") != 0) {
        fprintf(stderr, "Test Single CPU Affinity failed: cpu_list = %s (expected \"[0]\")\n", info.cpu_list);
        exit(EXIT_FAILURE);
    }
    printf("Test Single CPU Affinity passed: cpu_list = %s\n", info.cpu_list);
    // Reset affinity to all available CPUs
    CPU_ZERO(&mask);
    for (int i = 0; i < sysconf(_SC_NPROCESSORS_ONLN); i++) {
        CPU_SET(i, &mask);
    }
    sched_setaffinity(pid, sizeof(mask), &mask);
}

// Busy work to force scheduling.
void busy_work(int seconds) {
    time_t start = time(NULL);
    volatile unsigned long dummy = 0;
    while (time(NULL) - start < seconds) {
        dummy++;
    }
}

// Test 3: Allow process to run on multiple CPUs (if available) and check if the CPU list is in the expected range format.
void test_multi_cpu_affinity(void) {
    pid_t pid = getpid();
    cpu_set_t mask;
    CPU_ZERO(&mask);
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cpus < 2) {
        printf("Test Multi CPU Affinity skipped (only one CPU available).\n");
        return;
    }
    // Allow running on at least two CPUs: expecting the list to be "[0-1]"
    CPU_SET(0, &mask);
    CPU_SET(1, &mask);
    if (sched_setaffinity(pid, sizeof(mask), &mask) != 0) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }
    // Do busy work to force potential migrations
    busy_work(3);
    schedstat_info info;
    assert(read_schedstat(pid, &info) == 0);
    // Check if cpu_list is exactly "[0-1]"
    if (strcmp(info.cpu_list, "[0-1]") != 0) {
        fprintf(stderr, "Test Multi CPU Affinity failed: cpu_list = %s (expected \"[0-1]\")\n", info.cpu_list);
        exit(EXIT_FAILURE);
    }
    printf("Test Multi CPU Affinity passed: cpu_list = %s\n", info.cpu_list);
    // Reset affinity to all available CPUs
    CPU_ZERO(&mask);
    for (int i = 0; i < num_cpus; i++) {
        CPU_SET(i, &mask);
    }
    sched_setaffinity(pid, sizeof(mask), &mask);
}

// Test 4: Epoch boundary test.
// We assume that after 10 seconds, the epoch resets: used CPU list should be cleared or updated.
// For this test, we perform busy work, then sleep long enough for an epoch rollover,
// then do more work and verify that the CPU list has changed.
void test_epoch_reset(void) {
    pid_t pid = getpid();
    schedstat_info before, after;
    // Do some busy work so that there is nonzero data in the CPU list.
    busy_work(3);
    assert(read_schedstat(pid, &before) == 0);
    printf("Before epoch reset: cpu_list = %s\n", before.cpu_list);
    // Sleep for longer than an epoch (e.g., 12 seconds) to ensure a rollover.
    sleep(12);
    // Do some busy work again to get new scheduling info.
    busy_work(3);
    assert(read_schedstat(pid, &after) == 0);
    printf("After epoch reset: cpu_list = %s\n", after.cpu_list);
    // The used CPU list should have been reset at the start of the new epoch,
    // so the new CPU list may be different from the previous epoch.
    if (strcmp(before.cpu_list, after.cpu_list) == 0) {
        fprintf(stderr, "Test Epoch Reset failed: cpu_list did not change across epochs.\n");
        exit(EXIT_FAILURE);
    }
    printf("Test Epoch Reset passed.\n");
}

int main(void) {
    printf("Running Scheduling History Tests (Task 3)...\n");
    test_format();
    test_single_cpu_affinity();
    test_multi_cpu_affinity();
    test_epoch_reset();
    printf("All tests passed.\n");
    return 0;
}
