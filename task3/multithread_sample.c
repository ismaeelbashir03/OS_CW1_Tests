// multithread_sample.c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *worker(void *arg) {
    volatile unsigned long long counter = 0;
    while (1) {
        counter++; // Busy work to keep the CPU busy
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int nthreads = 4;  // Default to 4 threads
    if (argc > 1) {
        nthreads = atoi(argv[1]);
        if(nthreads < 1) nthreads = 1;
    }
    
    pthread_t *threads = malloc(nthreads * sizeof(pthread_t));
    if (!threads) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < nthreads; i++) {
        if (pthread_create(&threads[i], NULL, worker, NULL)) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    
    // Wait for threads (in practice, they run forever)
    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    return 0;
}