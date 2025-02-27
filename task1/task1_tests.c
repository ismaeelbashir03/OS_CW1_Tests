#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

#ifndef SYS_ANCESTOR_PID
#define SYS_ANCESTOR_PID 463
#endif

/* Global variable to store the original (main) process's PID.
   This value is copied into children so that we know what the grandparent's
   PID should be in the chain-alive tests. */
static pid_t original_pid;
static int tests_failed = 0;

/*
 HELPER FUNCTIONS FOR PRINTING TEST RESULTS
*/
void check_test(const char *description, long expected, long actual) {
    if (actual == expected) {
        printf("[PASS] %s\n", description);
    } else {
        printf("[FAIL] %s: expected %ld, got %ld\n", description, expected, actual);
        tests_failed++;
    }
}

void check_test_errno(const char *description, int expected_errno, long ret) {
    if (ret == -1 && errno == expected_errno) {
        printf("[PASS] %s: got -1 and errno set to %d as expected.\n", description, expected_errno);
    } else {
        printf("[FAIL] %s: expected -1 with errno %d, got %ld with errno %d\n", description, expected_errno, ret, errno);
        tests_failed++;
    }
}

/*
  BASIC TESTS
 */
void basic_tests() {
    long ret;

    printf("Running basic tests in process %d...\n", getpid());

    /* Test negative PID: should return -EINVAL */
    errno = 0;
    ret = syscall(SYS_ANCESTOR_PID, -5, 0);
    check_test_errno("Negative PID (-5)", EINVAL, ret);

    /* Test using PID==0 for n==0: should return the calling process's PID */
    pid_t mypid = getpid();
    errno = 0;
    ret = syscall(SYS_ANCESTOR_PID, 0, 0);
    check_test("PID==0, n==0 (should return own PID)", mypid, ret);
}

/* Test with an alive chain:
   main process (original_pid) -> child -> grandchild.
   In the grandchild process:
     - n==0 should return its own PID,
     - n==1 should return the child’s PID (its immediate parent),
     - n==2 should return the original (main) process’s PID,
     - n==3 should return the bash process’s PID
     - n==4 should return the login process’s PID
     - n==5 should return the init process’s PID
     - n==6 should return pid 0
     - n==7 should return -ESRCH (no such process)
*/
void test_chain_alive() {
    pid_t child_pid, grandchild_pid;
    int status;

    printf("\n[Chain Alive Test] Starting chain with all ancestors alive...\n");

    child_pid = fork();
    if (child_pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        /* In child process */
        grandchild_pid = fork();
        if (grandchild_pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (grandchild_pid == 0) {
            /* In grandchild process */
            pid_t mypid = getpid();
            pid_t parent_pid = getppid();
            printf("Grandchild process %d: parent = %d, expected grandparent = %d\n", 
                   mypid, parent_pid, original_pid);

            long ret;

            /* Test n == 0 */
            errno = 0;
            ret = syscall(SYS_ANCESTOR_PID, mypid, 0);
            check_test("Alive chain: n==0 (self)", mypid, ret);

            /* Test n == 1 */
            errno = 0;
            ret = syscall(SYS_ANCESTOR_PID, mypid, 1);
            check_test("Alive chain: n==1 (immediate parent)", parent_pid, ret);

            /* Test n == 2 */
            errno = 0;
            ret = syscall(SYS_ANCESTOR_PID, mypid, 2);
            check_test("Alive chain: n==2 (grandparent)", original_pid, ret);

            /* Test n == 3: nshould return the bash process’s PID */
            printf("Skipping test for n==3 (bash process) because it is not guaranteed to be a static pid.\n");

            /* Test n == 4: should return the login process’s PID */
            printf("Skipping test for n==4 (login process) because it is not guaranteed to be a static pid.\n");

            /* Test n == 5: should return the init process’s PID */
            errno = 0;
            ret = syscall(SYS_ANCESTOR_PID, mypid, 5);
            check_test("Alive chain: n==5 (init process)", 1, ret);

            /* Test n == 6: should return pid 0 */
            errno = 0;
            ret = syscall(SYS_ANCESTOR_PID, mypid, 6);
            check_test("Alive chain: n==6 (pid 0)", 0, ret);

            /* Test n == 7: should return -ESRCH (no such process) */
            errno = 0;
            ret = syscall(SYS_ANCESTOR_PID, mypid, 7);
            check_test_errno("Alive chain: n==7 (no such process)", ESRCH, ret);

            exit(EXIT_SUCCESS);
        } else {
            /* In child process: wait for grandchild */
            waitpid(grandchild_pid, &status, 0);
            exit(EXIT_SUCCESS);
        }
    } else {
        /* In main process: wait for child */
        waitpid(child_pid, &status, 0);
        printf("[Chain Alive Test] Completed.\n");
    }
}

/* Test with a broken chain:
   In this test, we create a child which forks a grandchild and then exits,
   so that the grandchild is orphaned. To ensure the main process waits for the
   grandchild’s tests, a pipe is used for synchronization.
*/
void test_chain_broken() {
    int pipefd[2];
    pid_t child_pid, grandchild_pid;
    int status;

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    printf("\n[Chain Broken Test] Starting chain where intermediate parent dies...\n");

    child_pid = fork();
    if (child_pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        /* In child process */
        grandchild_pid = fork();
        if (grandchild_pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (grandchild_pid == 0) {
            /* In grandchild process */
            close(pipefd[0]); // close read end
            /* Sleep briefly to ensure that the immediate parent has exited */
            sleep(3);
            pid_t mypid = getpid();
            printf("Broken chain grandchild process %d running; original expected parent is dead.\n", mypid);

            long ret;
            /* Test n == 0: returns self */
            errno = 0;
            ret = syscall(SYS_ANCESTOR_PID, mypid, 0);
            check_test("Broken chain: n==0 (self)", mypid, ret);

            /* Test n == 1: returns init process */
            errno = 0;
            ret = syscall(SYS_ANCESTOR_PID, mypid, 1);
            check_test("Broken chain: n==1 (init process)", 1, ret);

            /* Test n == 2: returns pid 0 */
            errno = 0;
            ret = syscall(SYS_ANCESTOR_PID, mypid, 2);
            check_test("Broken chain: n==2 (pid 0)", 0, ret);

            /* Test n == 3: returns -ESRCH (no such process) */
            errno = 0;
            ret = syscall(SYS_ANCESTOR_PID, mypid, 3);
            check_test_errno("Broken chain: n==3 (no such process)", ESRCH, ret);

            /* Signal completion via the pipe */
            if (write(pipefd[1], "done", 4) == -1) {
                perror("write");
            }
            close(pipefd[1]);
            exit(EXIT_SUCCESS);
        } else {
            /* In child process: exit immediately so that the parent dies */
            exit(EXIT_SUCCESS);
        }
    } else {
        /* In main process */
        close(pipefd[1]); // close write end
        /* Wait for the broken chain grandchild to signal completion */
        char buf[10];
        if (read(pipefd[0], buf, sizeof(buf)) < 0) {
            perror("read");
        }
        close(pipefd[0]);
        waitpid(child_pid, &status, 0);
        printf("[Chain Broken Test] Completed.\n");
    }
}

int main(void) {
    original_pid = getpid();
    printf("Starting sys_ancestor_pid tests in process %d\n", original_pid);

    basic_tests();
    test_chain_alive();
    test_chain_broken();

    if (tests_failed == 0) {
        printf("\nAll tests PASSED.\n");
        return EXIT_SUCCESS;
    } else {
        printf("\nSome tests FAILED. (Total failures: %d)\n", tests_failed);
        return EXIT_FAILURE;
    }
}
