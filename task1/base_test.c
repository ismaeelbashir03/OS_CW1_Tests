//your gp must be your second child

#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <sys/wait.h>

#define SYSCALL1 463  // Use the assigned syscall number

int main() {
    pid_t parent = getpid(), gparent = getppid();
    pid_t argpid = 0;

    pid_t pid = fork();  // Fork a new process

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // Call the syscall from the child process
        for (unsigned int n = 0; n < 10; ++n)
            printf("CHILD: %d->%ld\n", n, syscall(SYSCALL1, argpid, n));
        return 0;
    } else {
        // Parent process
        wait(NULL);  // Wait for the child to finish
        printf("PARENT: chld:%d me(par):%d gp:%d\n", pid, parent, gparent);
    }

    return 0;
}
