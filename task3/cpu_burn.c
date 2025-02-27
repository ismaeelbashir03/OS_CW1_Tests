#include <stdio.h>
#include <unistd.h>

int main(void) {
    // Busy loop for a long time (e.g., 30 seconds)
    unsigned long i;
    for (;;) {
        for (i = 0; i < 1000000UL; i++)
            ; // do nothing
        // Optionally print a heartbeat every few seconds
        // printf("Running...\n");
        // sleep(1);
    }
    return 0;
}
