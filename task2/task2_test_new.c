#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <errno.h>
#define SYS_PROPAGATE_NICE 464
/* 

 * Wrapper for the propagate_nice system call.

 */

int propagate_nice(int increment) {

    return syscall(SYS_PROPAGATE_NICE, increment);

}
/* -----------------------------------------------------------------

   Test Case 1: Basic Functionality – Positive Increment with Live Children

   Description:

     - Parent’s niceness increases by 4.

     - Child receives floor(4/2)=2 increment.

     - Grandchild receives floor(2/2)=1 increment.

   Expected:

     - Parent: 0 + 4 = 4.

     - Child: 0 + 2 = 2.

     - Grandchild: 0 + 1 = 1.

------------------------------------------------------------------*/

int test_positive_increment_live_children() {

    printf("\nTest 1: Positive Increment with Live Children\n");

    int status = 0;

    /* Set parent's niceness to 0 */

    if (setpriority(PRIO_PROCESS, 0, 0) == -1) {

        perror("setpriority (parent)");

        return 1;

    }

    int pipe_child[2], pipe_grandchild[2];

    if (pipe(pipe_child) == -1) { perror("pipe_child"); return 1; }

    if (pipe(pipe_grandchild) == -1) { perror("pipe_grandchild"); return 1; }



    pid_t child = fork();

    if (child == -1) {

        perror("fork (child)");

        return 1;

    }

    if (child == 0) {

        /* Child process: fork a grandchild */

        pid_t grandchild = fork();

        if (grandchild == -1) { perror("fork (grandchild)"); exit(1); }

        if (grandchild == 0) {

            /* Grandchild process: wait to let propagation occur */

            sleep(2);

            int nic = getpriority(PRIO_PROCESS, 0);

            write(pipe_grandchild[1], &nic, sizeof(nic));

            exit(0);

        } else {

            /* Child process: wait a bit, then report its niceness */

            sleep(2);

            int nic = getpriority(PRIO_PROCESS, 0);

            write(pipe_child[1], &nic, sizeof(nic));

            wait(NULL); // wait for grandchild

            exit(0);

        }

    } else {

        /* Parent process: let children get ready then call propagate_nice(4) */

        sleep(1);

        int ret = propagate_nice(4);

        if (ret == -1) {

            printf("FAIL: propagate_nice(4) failed, errno=%d\n", errno);

            status = 1;

        }

        wait(NULL); // wait for child to finish

        int parent_nic = getpriority(PRIO_PROCESS, 0);

        int expected_parent = 0 + 4;

        int child_nic, grandchild_nic;

        read(pipe_child[0], &child_nic, sizeof(child_nic));

        read(pipe_grandchild[0], &grandchild_nic, sizeof(grandchild_nic));



        if (parent_nic != expected_parent) {

            printf("FAIL: Parent niceness = %d, expected %d\n", parent_nic, expected_parent);

            status = 1;

        } else {

            printf("PASS: Parent niceness = %d, expected %d\n", parent_nic, expected_parent);

        }

        if (child_nic != 0 + 4/2) {

            printf("FAIL: Child niceness = %d, expected %d\n", child_nic, 2);

            status = 1;

        } else {

            printf("PASS: Child niceness = %d, expected %d\n", child_nic, 2);

        }

        if (grandchild_nic != 0 + (4/2)/2) {

            printf("FAIL: Grandchild niceness = %d, expected %d\n", grandchild_nic, 1);

            status = 1;

        } else {

            printf("PASS: Grandchild niceness = %d, expected %d\n", grandchild_nic, 1);

        }

    }

    return status;

}



/* -----------------------------------------------------------------

   Test Case 2: Increment of 0

   Description: Call propagate_nice(0) on a process with niceness 10.

   Expected:

     - No change to niceness.

     - Return -1 (with errno==EINVAL).

------------------------------------------------------------------*/

int test_increment_of_zero() {

    printf("\nTest 2: Increment of 0\n");

    if (setpriority(PRIO_PROCESS, 0, 10) == -1) {

        perror("setpriority (parent)");

        return 1;

    }

    int ret = propagate_nice(0);

    if (ret != -1) {

        printf("FAIL: propagate_nice(0) returned %d, expected -1\n", ret);

        return 1;

    } else {

        if (errno != EINVAL) {

            printf("FAIL: propagate_nice(0) errno=%d, expected %d (EINVAL)\n", errno, EINVAL);

            return 1;

        }

    }

    int nic = getpriority(PRIO_PROCESS, 0);

    if (nic != 10) {

        printf("FAIL: Parent niceness = %d, expected 10\n", nic);

        return 1;

    }

    printf("PASS: Output: Parent niceness = %d; Expected: 10; propagate_nice(0) returned error as expected\n", nic);

    return 0;

}



/* -----------------------------------------------------------------

   Test Case 3: Maximum Niceness Clamping

   Description: Parent starts at 18. Call propagate_nice(3).

   Expected:

     - Parent clamps to 19.

     - Child receives floor(3/2)=1 increment; if starting at 18, clamped to 19.

------------------------------------------------------------------*/

int test_maximum_niceness_clamping() {

    printf("\nTest 3: Maximum Niceness Clamping\n");

    if (setpriority(PRIO_PROCESS, 0, 18) == -1) { perror("setpriority (parent)"); return 1; }

    int pipe_child[2];

    if (pipe(pipe_child) == -1) { perror("pipe"); return 1; }

    pid_t child = fork();

    if (child == -1) { perror("fork (child)"); return 1; }

    if (child == 0) {

        sleep(2);

        int nic = getpriority(PRIO_PROCESS, 0);

        write(pipe_child[1], &nic, sizeof(nic));

        exit(0);

    } else {

        sleep(1);

        int ret = propagate_nice(3);

        if (ret == -1) {

            printf("FAIL: propagate_nice(3) failed unexpectedly, errno=%d\n", errno);

            return 1;

        }

        wait(NULL);

        int parent_nic = getpriority(PRIO_PROCESS, 0);

        if (parent_nic != 19) {

            printf("FAIL: Parent niceness = %d, expected 19\n", parent_nic);

            return 1;

        } else {

            printf("PASS: Parent niceness = %d, expected 19\n", parent_nic);

        }

        int child_nic;

        read(pipe_child[0], &child_nic, sizeof(child_nic));

        if (child_nic != 19) {

            printf("FAIL: Child niceness = %d, expected 19\n", child_nic);

            return 1;

        } else {

            printf("PASS: Child niceness = %d, expected 19\n", child_nic);

        }

    }

    return 0;

}



/* -----------------------------------------------------------------

   Test Case 4: Minimum Niceness Clamping

   Description: Parent starts at -20. Call propagate_nice(5).

   Expected: Parent becomes -20 + 5 = -15.

------------------------------------------------------------------*/

int test_minimum_niceness_clamping() {

    printf("\nTest 4: Minimum Niceness Clamping\n");

    if (setpriority(PRIO_PROCESS, 0, -20) == -1) { perror("setpriority (parent)"); return 1; }

    int ret = propagate_nice(5);

    if (ret == -1) {

        printf("FAIL: propagate_nice(5) failed unexpectedly, errno=%d\n", errno);

        return 1;

    }

    int parent_nic = getpriority(PRIO_PROCESS, 0);

    if (parent_nic != -15) {

        printf("FAIL: Parent niceness = %d, expected -15\n", parent_nic);

        return 1;

    } else {

        printf("PASS: Parent niceness = %d, expected -15\n", parent_nic);

    }

    return 0;

}



/* -----------------------------------------------------------------

   Test Case 5: Negative Increment

   Description: Call propagate_nice(-5).

   Expected: Return -1 with errno==EINVAL.

------------------------------------------------------------------*/

int test_negative_increment() {

    printf("\nTest 5: Negative Increment\n");

    int ret = propagate_nice(-5);

    if (ret != -1 || errno != EINVAL) {

        printf("FAIL: propagate_nice(-5) returned %d, errno=%d; expected -1 with errno==EINVAL\n", ret, errno);

        return 1;

    }

    printf("PASS: Output: propagate_nice(-5) returned -1 (errno=%d); Expected: -EINVAL\n", errno);

    return 0;

}



/* -----------------------------------------------------------------

   Test Case 6: Dead Child

   Description: One live child and one dead child.

   Expected:

     - Live child receives floor(4/2)=2 increment.

     - Dead child is ignored.

     - Parent increases by 4.

------------------------------------------------------------------*/

int test_dead_child() {

    printf("\nTest 6: Dead Child\n");

    if (setpriority(PRIO_PROCESS, 0, 0) == -1) { perror("setpriority (parent)"); return 1; }

    int pipe_live[2];

    if (pipe(pipe_live) == -1) { perror("pipe_live"); return 1; }



    /* Fork a dead child */

    pid_t dead_child = fork();

    if (dead_child == -1) { perror("fork (dead_child)"); return 1; }

    if (dead_child == 0) {

        exit(0);

    }

    /* Fork a live child */

    pid_t live_child = fork();

    if (live_child == -1) { perror("fork (live_child)"); return 1; }

    if (live_child == 0) {

        sleep(2);

        int nic = getpriority(PRIO_PROCESS, 0);

        write(pipe_live[1], &nic, sizeof(nic));

        exit(0);

    }

    sleep(1);

    int ret = propagate_nice(4);

    if (ret == -1) {

        printf("FAIL: propagate_nice(4) failed, errno=%d\n", errno);

        return 1;

    }

    wait(NULL); /* dead child already exited */

    wait(NULL); /* wait for live child */

    int parent_nic = getpriority(PRIO_PROCESS, 0);

    if (parent_nic != 4) {

        printf("FAIL: Parent niceness = %d, expected %d\n", parent_nic, 4);

        return 1;

    } else {

        printf("PASS: Parent niceness = %d, expected %d\n", parent_nic, 4);

    }

    int live_nic;

    read(pipe_live[0], &live_nic, sizeof(live_nic));

    if (live_nic != 2) {

        printf("FAIL: Live child's niceness = %d, expected %d\n", live_nic, 2);

        return 1;

    } else {

        printf("PASS: Live child's niceness = %d, expected %d\n", live_nic, 2);

    }

    return 0;

}



/* -----------------------------------------------------------------

   Test Case 7: No Children

   Description: Call propagate_nice(4) on a process with no children.

   Expected: Parent's niceness increases by 4.

------------------------------------------------------------------*/

int test_no_children() {

    printf("\nTest 7: No Children\n");

    if (setpriority(PRIO_PROCESS, 0, 0) == -1) { perror("setpriority (parent)"); return 1; }

    int ret = propagate_nice(4);

    if (ret == -1) {

        printf("FAIL: propagate_nice(4) failed, errno=%d\n", errno);

        return 1;

    }

    int parent_nic = getpriority(PRIO_PROCESS, 0);

    if (parent_nic != 4) {

        printf("FAIL: Parent niceness = %d, expected %d\n", parent_nic, 4);

        return 1;

    } else {

        printf("PASS: Parent niceness = %d, expected %d\n", parent_nic, 4);

    }

    return 0;

}



/* -----------------------------------------------------------------

   Test Case 8: Multi-Level Hierarchy

   Description: Process P1 → P2 → P3. Call propagate_nice(8) on P1.

   Expected:

     - P1: 0 + 8 = 8.

     - P2: 0 + floor(8/2)=4.

     - P3: 0 + floor(4/2)=2.

------------------------------------------------------------------*/

int test_multi_level_hierarchy() {

    printf("\nTest 8: Multi-Level Hierarchy\n");

    if (setpriority(PRIO_PROCESS, 0, 0) == -1) { perror("setpriority (parent)"); return 1; }

    int pipe_child[2], pipe_grandchild[2];

    if (pipe(pipe_child) == -1) { perror("pipe_child"); return 1; }

    if (pipe(pipe_grandchild) == -1) { perror("pipe_grandchild"); return 1; }



    pid_t child = fork();

    if (child == -1) { perror("fork (child)"); return 1; }

    if (child == 0) {

        /* Child process: fork grandchild */

        pid_t grandchild = fork();

        if (grandchild == -1) { perror("fork (grandchild)"); exit(1); }

        if (grandchild == 0) {

            sleep(2);

            int nic = getpriority(PRIO_PROCESS, 0);

            write(pipe_grandchild[1], &nic, sizeof(nic));

            exit(0);

        } else {

            sleep(2);

            int nic = getpriority(PRIO_PROCESS, 0);

            write(pipe_child[1], &nic, sizeof(nic));

            wait(NULL);

            exit(0);

        }

    } else {

        sleep(1);

        int ret = propagate_nice(8);

        if (ret == -1) {

            printf("FAIL: propagate_nice(8) failed, errno=%d\n", errno);

            return 1;

        }

        wait(NULL);

        int parent_nic = getpriority(PRIO_PROCESS, 0);

        if (parent_nic != 8) {

            printf("FAIL: Parent niceness = %d, expected %d\n", parent_nic, 8);

            return 1;

        } else {

            printf("PASS: Parent niceness = %d, expected %d\n", parent_nic, 8);

        }

        int child_nic, grandchild_nic;

        read(pipe_child[0], &child_nic, sizeof(child_nic));

        read(pipe_grandchild[0], &grandchild_nic, sizeof(grandchild_nic));

        if (child_nic != 4) {

            printf("FAIL: Child niceness = %d, expected %d\n", child_nic, 4);

            return 1;

        } else {

            printf("PASS: Child niceness = %d, expected %d\n", child_nic, 4);

        }

        if (grandchild_nic != 2) {

            printf("FAIL: Grandchild niceness = %d, expected %d\n", grandchild_nic, 2);

            return 1;

        } else {

            printf("PASS: Grandchild niceness = %d, expected %d\n", grandchild_nic, 2);

        }

    }

    return 0;

}



/* -----------------------------------------------------------------

   Test Case 9: Increment Halts at Zero

   Description: Call propagate_nice(1) on a process with children.

   Expected:

     - Parent: 0 + 1 = 1.

     - Child: floor(1/2) = 0 (no change).

------------------------------------------------------------------*/

int test_increment_halts_at_zero() {

    printf("\nTest 9: Increment Halts at Zero\n");

    if (setpriority(PRIO_PROCESS, 0, 0) == -1) { perror("setpriority (parent)"); return 1; }

    int pipe_child[2];

    if (pipe(pipe_child) == -1) { perror("pipe_child"); return 1; }

    pid_t child = fork();

    if (child == -1) { perror("fork (child)"); return 1; }

    if (child == 0) {

        sleep(2);

        int nic = getpriority(PRIO_PROCESS, 0);

        write(pipe_child[1], &nic, sizeof(nic));

        exit(0);

    } else {

        sleep(1);

        int ret = propagate_nice(1);

        if (ret == -1) {

            printf("FAIL: propagate_nice(1) failed, errno=%d\n", errno);

            return 1;

        }

        wait(NULL);

        int parent_nic = getpriority(PRIO_PROCESS, 0);

        if (parent_nic != 1) {

            printf("FAIL: Parent niceness = %d, expected %d\n", parent_nic, 1);

            return 1;

        } else {

            printf("PASS: Parent niceness = %d, expected %d\n", parent_nic, 1);

        }

        int child_nic;

        read(pipe_child[0], &child_nic, sizeof(child_nic));

        if (child_nic != 0) {

            printf("FAIL: Child niceness = %d, expected %d\n", child_nic, 0);

            return 1;

        } else {

            printf("PASS: Child niceness = %d, expected %d\n", child_nic, 0);

        }

    }

    return 0;

}



/* -----------------------------------------------------------------

   Test Case 10: Deep Propagation

   Description: Process chain: P1 → P2 → P3 → P4.

   Call propagate_nice(8) on P1.

   Expected:

     - P1: 0 + 8 = 8.

     - P2: 0 + floor(8/2)=4.

     - P3: 0 + floor(4/2)=2.

     - P4: 0 + floor(2/2)=1.

------------------------------------------------------------------*/

int test_deep_propagation() {

    printf("\nTest 10: Deep Propagation\n");

    if (setpriority(PRIO_PROCESS, 0, 0) == -1) { perror("setpriority (parent)"); return 1; }

    int pipe_p2[2], pipe_p3[2], pipe_p4[2];

    if (pipe(pipe_p2)==-1) { perror("pipe_p2"); return 1; }

    if (pipe(pipe_p3)==-1) { perror("pipe_p3"); return 1; }

    if (pipe(pipe_p4)==-1) { perror("pipe_p4"); return 1; }

    pid_t p2 = fork();

    if (p2 == -1) { perror("fork (P2)"); return 1; }

    if (p2 == 0) {

        /* P2: fork P3 */

        pid_t p3 = fork();

        if (p3 == -1) { perror("fork (P3)"); exit(1); }

        if (p3 == 0) {

            /* P3: fork P4 */

            pid_t p4 = fork();

            if (p4 == -1) { perror("fork (P4)"); exit(1); }

            if (p4 == 0) {

                sleep(2);

                int nic = getpriority(PRIO_PROCESS, 0);

                write(pipe_p4[1], &nic, sizeof(nic));

                exit(0);

            } else {

                sleep(2);

                int nic = getpriority(PRIO_PROCESS, 0);

                write(pipe_p3[1], &nic, sizeof(nic));

                wait(NULL);

                exit(0);

            }

        } else {

            sleep(2);

            int nic = getpriority(PRIO_PROCESS, 0);

            write(pipe_p2[1], &nic, sizeof(nic));

            wait(NULL);

            exit(0);

        }

    } else {

        sleep(1);

        int ret = propagate_nice(8);

        if (ret == -1) {

            printf("FAIL: propagate_nice(8) failed, errno=%d\n", errno);

            return 1;

        }

        wait(NULL);

        int parent_nic = getpriority(PRIO_PROCESS, 0);

        if (parent_nic != 8) {

            printf("FAIL: Parent niceness = %d, expected %d\n", parent_nic, 8);

            return 1;

        } else {

            printf("PASS: Parent niceness = %d, expected %d\n", parent_nic, 8);

        }

        int p2_nic, p3_nic, p4_nic;

        read(pipe_p2[0], &p2_nic, sizeof(p2_nic));

        read(pipe_p3[0], &p3_nic, sizeof(p3_nic));

        read(pipe_p4[0], &p4_nic, sizeof(p4_nic));

        if (p2_nic != 4) {

            printf("FAIL: P2 niceness = %d, expected 4\n", p2_nic);

            return 1;

        } else {

            printf("PASS: P2 niceness = %d, expected 4\n", p2_nic);

        }

        if (p3_nic != 2) {

            printf("FAIL: P3 niceness = %d, expected 2\n", p3_nic);

            return 1;

        } else {

            printf("PASS: P3 niceness = %d, expected 2\n", p3_nic);

        }

        if (p4_nic != 1) {

            printf("FAIL: P4 niceness = %d, expected 1\n", p4_nic);

            return 1;

        } else {

            printf("PASS: P4 niceness = %d, expected 1\n", p4_nic);

        }

    }

    return 0;

}



/* -----------------------------------------------------------------

   Test Case 12: No Changes Anywhere

   Description: Process with no children and niceness 19.

   Expected: propagate_nice(0) returns -1.

------------------------------------------------------------------*/

int test_no_changes_anywhere() {

    printf("\nTest 12: No Changes Anywhere\n");

    if (setpriority(PRIO_PROCESS, 0, 19) == -1) { perror("setpriority (parent)"); return 1; }

    int ret = propagate_nice(0);

    if (ret != -1) {

        printf("FAIL: propagate_nice(0) returned %d, expected -1\n", ret);

        return 1;

    }

    printf("PASS: Output: propagate_nice(0) returned -1 as expected; Parent niceness remains %d\n", getpriority(PRIO_PROCESS, 0));

    return 0;

}



/* -----------------------------------------------------------------

   Test Case 13: Partial Success

   Description:

     - Process with one live child (initial niceness 19) and one dead child.

     - Parent starts at 10.

   Expected:

     - Parent: 10 + 2 = 12.

     - Live child remains at 19 (clamped).

------------------------------------------------------------------*/

int test_partial_success() {

    printf("\nTest 13: Partial Success\n");

    if (setpriority(PRIO_PROCESS, 0, 10) == -1) { perror("setpriority (parent)"); return 1; }

    int pipe_live[2];

    if (pipe(pipe_live) == -1) { perror("pipe_live"); return 1; }



    pid_t dead_child = fork();

    if (dead_child == -1) { perror("fork (dead_child)"); return 1; }

    if (dead_child == 0) {

        exit(0);

    }

    pid_t live_child = fork();

    if (live_child == -1) { perror("fork (live_child)"); return 1; }

    if (live_child == 0) {

        /* In live child, set niceness to 19 */

        if (setpriority(PRIO_PROCESS, 0, 19) == -1) { perror("setpriority (live child)"); exit(1); }

        sleep(2);

        int nic = getpriority(PRIO_PROCESS, 0);

        write(pipe_live[1], &nic, sizeof(nic));

        exit(0);

    }

    sleep(1);

    int ret = propagate_nice(2);

    if (ret == -1) {

        printf("FAIL: propagate_nice(2) failed, errno=%d\n", errno);

        return 1;

    }

    wait(NULL); /* dead child */

    wait(NULL); /* live child */

    int parent_nic = getpriority(PRIO_PROCESS, 0);

    if (parent_nic != 12) {

        printf("FAIL: Parent niceness = %d, expected 12\n", parent_nic);

        return 1;

    } else {

        printf("PASS: Parent niceness = %d, expected 12\n", parent_nic);

    }

    int live_nic;

    read(pipe_live[0], &live_nic, sizeof(live_nic));

    if (live_nic != 19) {

        printf("FAIL: Live child's niceness = %d, expected 19\n", live_nic);

        return 1;

    } else {

        printf("PASS: Live child's niceness = %d, expected 19\n", live_nic);

    }

    return 0;

}



/* -----------------------------------------------------------------

   Main: Run all tests

------------------------------------------------------------------*/

int main(void) {

    int total_failures = 0;



    total_failures += test_positive_increment_live_children();

    total_failures += test_increment_of_zero();

    total_failures += test_maximum_niceness_clamping();

    total_failures += test_minimum_niceness_clamping();

    total_failures += test_negative_increment();

    total_failures += test_dead_child();

    total_failures += test_no_children();

    total_failures += test_multi_level_hierarchy();

    total_failures += test_increment_halts_at_zero();

    total_failures += test_deep_propagation();

    total_failures += test_no_changes_anywhere();

    total_failures += test_partial_success();

    printf("\nSummary: %d test(s) failed.\n", total_failures);

    return total_failures;

}