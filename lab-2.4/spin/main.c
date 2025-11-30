#include <stdio.h>
#include <pthread.h>
#include "spin.h"

#define N_THREADS 4
#define N_ITER    1000000

static long long counter = 0;
static spinlock_t lock;

void* worker(void *arg) {
    (void)arg;

    for (int i = 0; i < N_ITER; ++i) {
        spin_lock(&lock);
        counter++;
        spin_unlock(&lock);
    }

    return NULL;
}

int main(void) {
    pthread_t threads[N_THREADS];
    spin_init(&lock);

    if (spin_trylock(&lock)) {
        printf("trylock: locked\n");
        spin_unlock(&lock);
    } else {
        printf("trylock: busy\n");
    }

    for (int i = 0; i < N_THREADS; ++i) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    for (int i = 0; i < N_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    printf("Expected: %d\n", N_THREADS * N_ITER);
    printf("Actual:   %lld\n", counter);
    return 0;
}
