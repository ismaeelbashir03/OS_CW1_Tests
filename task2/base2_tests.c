#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>

#define SYSCALL2 464

int main()
{
    pid_t pid1 = -1, pid2 = -1;

    pid1 = fork();
    if (pid1 == 0) {
        pid2 = fork();
    }

    printf("pid1:%d, pid2:%d, ni:%d\n",
           pid1, pid2, getpriority(PRIO_PROCESS, 0));

    sleep(1);
    if (pid2 == -1)
        syscall(SYSCALL2, 5);
    sleep(1);

    printf("pid1:%d, pid2:%d, ni:%d\n",
           pid1, pid2, getpriority(PRIO_PROCESS, 0));

    return 0;
}
