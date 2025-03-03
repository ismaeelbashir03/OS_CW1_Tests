#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

#define SYS_ANCESTOR_PID 463

long call_ancestor(pid_t pid, unsigned int n) {
    long ret = syscall(SYS_ANCESTOR_PID, pid, n);
    if (ret < 0) {
        fprintf(stderr, "call_ancestor(pid=%d, n=%u) failed: %s\n",
                pid, n, strerror(errno));
        return ret;
    } else {
        printf("call_ancestor(pid=%d, n=%u) => %ld\n", pid, n, ret);
    }
    return ret;
}

int main(void) {
    pid_t p1 = getpid();
    printf("\n[Parent] P1 PID: %d\n", p1);

    pid_t p2 = fork();
    if (p2 < 0) {
        perror("fork() failed");
        exit(EXIT_FAILURE);
    }

    if (p2 == 0) {
        pid_t p2_pid = getpid();
        printf("[Child]  P2 PID: %d\n", p2_pid);

        pid_t p3 = fork();
        if (p3 < 0) {
            perror("fork() failed");
            exit(EXIT_FAILURE);
        }

        if (p3 == 0) {
            pid_t p3_pid = getpid();
            printf("[Grandchild] P3 PID: %d\n", p3_pid);

            printf("\n-- Testing from Grandchild (P3) --\n");

            call_ancestor(0, 0);
            call_ancestor(0, 1);
            call_ancestor(0, 2);
            call_ancestor(0, 3);
            call_ancestor(-5, 0);
            call_ancestor(0, 999);

            _exit(0);

        } else {
            wait(NULL);
            printf("\n-- Testing from Child (P2) --\n");
            call_ancestor(0, 0);
        }

    } else {
        wait(NULL);
    }

    return 0;
}
