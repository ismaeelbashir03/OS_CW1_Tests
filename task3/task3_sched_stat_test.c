#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

static void busy_loop_for_seconds(int sec)
{
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        long elapsed_sec = current.tv_sec - start.tv_sec;
        if (elapsed_sec >= sec) {
            break;
        }
        volatile double x = 123.456;
        x = x * x;
    }
}

static void print_schedstat(void)
{
    FILE *fp = fopen("/proc/self/schedstat", "r");
    if (!fp) {
        perror("fopen /proc/self/schedstat");
        return;
    }

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        printf("  /proc/self/schedstat => %s", buffer);
    } else {
        printf("  Could not read schedstat info.\n");
    }
    fclose(fp);
}

int main(void)
{
    pid_t pid = getpid();
    printf("=== Single-Process Schedstat Test ===\n");
    printf("PID = %d\n\n", pid);

    for (int i = 1; i <= 3; i++) {
        printf("[Interval %d] Busy looping for 4 seconds...\n", i);
        busy_loop_for_seconds(4);
        printf("[Interval %d] Reading /proc/self/schedstat after busy loop...\n", i);
        print_schedstat();
        printf("\n");
    }

    printf("Done. If your kernel epoch logic triggered (10 sec on-CPU), you may have seen a reset.\n");
    return 0;
}
