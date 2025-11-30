#include <stdio.h>
#include <pthread.h>
#include "mutex.h"

#define N_THREADS 4
#define N_ITER    1000000

static long long counter = 0;
static futex_mutex_t mtx;

void* worker(void *arg) {
    (void)arg;

    for (int i = 0; i < N_ITER; ++i) {
        fmutex_lock(&mtx);
        counter++;
        fmutex_unlock(&mtx);
    }

    return NULL;
}

int main(void) {
    pthread_t threads[N_THREADS];
    fmutex_init(&mtx);

    if (fmutex_trylock(&mtx)) {
        printf("trylock: locked\n");
        fmutex_unlock(&mtx);
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
