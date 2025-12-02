#define _GNU_SOURCE 
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include "spin.h"   

#define N_THREADS 16
#define N_ITER    1000000

static long long counter_spin    = 0;
static long long counter_pthread = 0;

static spinlock_t         spin_lock_my;
static pthread_spinlock_t spin_lock_pthread;

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void* worker_spin(void *arg) {
    (void)arg;

    for (int i = 0; i < N_ITER; ++i) {
        spin_lock(&spin_lock_my);
        counter_spin++;
        spin_unlock(&spin_lock_my);
    }

    return NULL;
}

void* worker_pthread_spin(void *arg) {
    (void)arg;

    for (int i = 0; i < N_ITER; ++i) {
        pthread_spin_lock(&spin_lock_pthread);
        counter_pthread++;
        pthread_spin_unlock(&spin_lock_pthread);
    }

    return NULL;
}

int main(void) {
    pthread_t threads[N_THREADS];

    spin_init(&spin_lock_my);
    pthread_spin_init(&spin_lock_pthread, 0);  

    printf("=== spinlock_t (my) ===\n");

    double t0 = now_sec();

    for (int i = 0; i < N_THREADS; ++i)
        pthread_create(&threads[i], NULL, worker_spin, NULL);

    for (int i = 0; i < N_THREADS; ++i)
        pthread_join(threads[i], NULL);

    double t1 = now_sec();

    printf("Expected: %lld\n", (long long)N_THREADS * N_ITER);
    printf("Actual:   %lld\n", counter_spin);
    printf("Time:     %.6f s\n\n", t1 - t0);

    printf("=== pthread_spinlock_t ===\n");

    double t2 = now_sec();

    for (int i = 0; i < N_THREADS; ++i)
        pthread_create(&threads[i], NULL, worker_pthread_spin, NULL);

    for (int i = 0; i < N_THREADS; ++i)
        pthread_join(threads[i], NULL);

    double t3 = now_sec();

    printf("Expected: %lld\n", (long long)N_THREADS * N_ITER);
    printf("Actual:   %lld\n", counter_pthread);
    printf("Time:     %.6f s\n", t3 - t2);

    pthread_spin_destroy(&spin_lock_pthread);
    return 0;
}
