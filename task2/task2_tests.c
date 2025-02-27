/* test_propagate_nice.c

   This file tests the sys_propagate_nice syscall (ID 464)
   as specified in Task 2: Trickle-Down Niceness.

   The syscall takes a single int argument n and behaves as follows:
    - If n is negative, the syscall must fail.
    - When a process calls sys_propagate_nice(n):
         • Its own nice value is increased by n (like the standard nice syscall).
         • Its live descendants have their niceness increased by a value that is halved
           each generation (using integer division).
         • If any descendant in the caller's lineage has exited (i.e. the chain is broken),
           then propagation does not continue past that branch.
    - The syscall returns 0 if at least one process’s nice value is updated; otherwise it
      returns a negative error value.

   This test file covers three scenarios:
    1. Negative increment test.
    2. Live chain test: A chain of Parent → Child → Grandchild remains intact.
       When propagate_nice(4) is called from the parent, expected outcomes are:
         Parent's nice: 0 + 4 = 4
         Child's nice: floor(4/2) = 2
         Grandchild's nice: floor(4/4) = 1
    3. Broken chain test: In a chain where the immediate child exits before propagation,
       only the parent is updated; the orphaned grandchild (now reparented) should remain unchanged.

   To avoid cumulative updates from previous tests, each test resets the parent's (and forked
   children's) nice value to 0 using setpriority(PRIO_PROCESS, 0, 0).

   Compile with:
       gcc -Wall -o test_propagate_nice test_propagate_nice.c

   Note: Ensure your kernel supports the new syscall (ID 464) and that getpriority(2) returns the
         current nice value.
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <errno.h>

#ifndef SYS_PROPAGATE_NICE
#define SYS_PROPAGATE_NICE 464
#endif

static int tests_failed = 0;

/* Helper function to compare expected and actual nice values */
void check_test_value(const char *desc, int expected, int actual) {
    if (expected == actual) {
        printf("[PASS] %s: expected %d, got %d\n", desc, expected, actual);
    } else {
        printf("[FAIL] %s: expected %d, got %d\n", desc, expected, actual);
        tests_failed++;
    }
}

/* Reset the nice value of the calling process to 0 */
void reset_nice() {
    if (setpriority(PRIO_PROCESS, 0, 0) == -1) {
        perror("setpriority");
        exit(EXIT_FAILURE);
    }
}

/* Test 1: Negative increment test.
   The syscall should fail when given a negative value.
*/
void test_negative_increment() {
    printf("\nRunning negative increment test...\n");
    reset_nice();
    errno = 0;
    long ret = syscall(SYS_PROPAGATE_NICE, -1);
    if (ret == -1 && errno != 0) {
        printf("[PASS] Negative increment test: syscall failed as expected (errno = %d).\n", errno);
    } else {
        printf("[FAIL] Negative increment test: expected failure, got %ld\n", ret);
        tests_failed++;
    }
}

/* Test 2: Live chain test.
   Create a process chain: Parent -> Child -> Grandchild.
   All processes remain alive.
   When the parent calls propagate_nice(4), the expected results are:
       Parent's nice: 0 + 4 = 4
       Child's nice: floor(4/2) = 2
       Grandchild's nice: floor(4/4) = 1

   Pipes are used to send the measured nice values back to the parent.
*/
void test_propagate_nice_live() {
    printf("\nRunning live chain test for propagate_nice...\n");
    reset_nice();  // Reset parent's nice to 0

    int pipe_child[2], pipe_grandchild[2];
    if (pipe(pipe_child) == -1 || pipe(pipe_grandchild) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid_child = fork();
    if (pid_child < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid_child == 0) {
        // Child process
        reset_nice();  // Ensure child's nice is 0
        pid_t pid_grandchild = fork();
        if (pid_grandchild < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid_grandchild == 0) {
            // Grandchild process
            reset_nice();  // Ensure grandchild's nice is 0
            char dummy;
            read(pipe_grandchild[0], &dummy, 1);
            int nice_val = getpriority(PRIO_PROCESS, 0);
            write(pipe_grandchild[1], &nice_val, sizeof(nice_val));
            exit(EXIT_SUCCESS);
        } else {
            // Child process: wait for signal then report its nice value
            char dummy;
            read(pipe_child[0], &dummy, 1);
            int nice_val = getpriority(PRIO_PROCESS, 0);
            write(pipe_child[1], &nice_val, sizeof(nice_val));
            wait(NULL); // wait for grandchild
            exit(EXIT_SUCCESS);
        }
    } else {
        // Parent process
        sleep(1);  // Give children time to set up
        errno = 0;
        long ret = syscall(SYS_PROPAGATE_NICE, 4);
        if (ret != 0) {
            printf("[FAIL] Live chain: propagate_nice returned %ld (expected 0).\n", ret);
            tests_failed++;
        }
        // Signal children to proceed
        char signal = 's';
        write(pipe_child[1], &signal, 1);
        write(pipe_grandchild[1], &signal, 1);
        
        wait(NULL); // Wait for child process to finish

        int child_nice, grandchild_nice;
        read(pipe_child[0], &child_nice, sizeof(child_nice));
        read(pipe_grandchild[0], &grandchild_nice, sizeof(grandchild_nice));
        int parent_nice = getpriority(PRIO_PROCESS, 0);

        // Expected results:
        // Parent: 0 + 4 = 4
        // Child: floor(4/2) = 2
        // Grandchild: floor(4/4) = 1
        check_test_value("Live chain: Parent nice", 4, parent_nice);
        check_test_value("Live chain: Child nice", 2, child_nice);
        check_test_value("Live chain: Grandchild nice", 1, grandchild_nice);
    }
}

/* Test 3: Broken chain test.
   Create a chain: Parent -> Child -> Grandchild, but have the Child exit immediately.
   This should break the descendant chain so that when the Parent calls propagate_nice(2),
   only the Parent is updated while the orphaned Grandchild (now reparented) remains unchanged.
   We use two separate pipes: one for synchronization (pipe_sync) and one for data (pipe_data).
*/
void test_propagate_nice_broken() {
    printf("\nRunning broken chain test for propagate_nice...\n");
    reset_nice();  // Reset parent's nice to 0

    int pipe_sync[2], pipe_data[2];
    if (pipe(pipe_sync) == -1 || pipe(pipe_data) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid_child = fork();
    if (pid_child < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid_child == 0) {
        // Child process: fork grandchild and exit immediately.
        pid_t pid_grandchild = fork();
        if (pid_grandchild < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid_grandchild == 0) {
            // Grandchild process
            reset_nice();  // Ensure grandchild's nice is 0
            char dummy;
            // Wait for parent's signal on pipe_sync.
            read(pipe_sync[0], &dummy, 1);
            int nice_val = getpriority(PRIO_PROCESS, 0);
            // Send the measured nice value through pipe_data.
            write(pipe_data[1], &nice_val, sizeof(nice_val));
            exit(EXIT_SUCCESS);
        } else {
            // Child exits immediately, breaking the chain.
            exit(EXIT_SUCCESS);
        }
    } else {
        // Parent process
        // Wait long enough to ensure Child has exited and reparenting occurs.
        sleep(2);
        errno = 0;
        long ret = syscall(SYS_PROPAGATE_NICE, 2);
        if (ret != 0) {
            printf("[FAIL] Broken chain: propagate_nice returned %ld (expected 0).\n", ret);
            tests_failed++;
        }
        // Signal the grandchild to proceed.
        char signal = 's';
        write(pipe_sync[1], &signal, 1);
        
        // Wait for Child and Grandchild to finish.
        wait(NULL); // wait for Child
        wait(NULL); // wait for Grandchild
        
        int grandchild_nice;
        read(pipe_data[0], &grandchild_nice, sizeof(grandchild_nice));
        int parent_nice = getpriority(PRIO_PROCESS, 0);

        // Expected:
        // Parent: 0 + 2 = 2.
        // Grandchild: should remain unchanged (0) because the chain is broken.
        check_test_value("Broken chain: Parent nice", 2, parent_nice);
        check_test_value("Broken chain: Grandchild nice", 0, grandchild_nice);
    }
}

int main(void) {
    printf("Starting sys_propagate_nice tests\n");

    test_negative_increment();
    test_propagate_nice_live();
    test_propagate_nice_broken();

    if (tests_failed == 0) {
        printf("\nAll tests PASSED.\n");
        return EXIT_SUCCESS;
    } else {
        printf("\nSome tests FAILED. (Total failures: %d)\n", tests_failed);
        return EXIT_FAILURE;
    }
}