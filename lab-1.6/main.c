#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "my_thread.h"

static void* worker(void* p) {
    uintptr_t id = (uintptr_t)p;
    usleep(1000000);                
    return (void*)(id * 2);
}



static void* worker_finite(void* p) {
    uintptr_t id = (uintptr_t)p;
    printf("[finite] thread %lu: start\n", (unsigned long)id);
    usleep(500000);
    printf("[finite] thread %lu: done\n", (unsigned long)id);
    return (void*)(id * 2);
}

static void* worker_infinite(void* p) {
    uintptr_t id = (uintptr_t)p;
    printf("[infinite] thread %lu: start \n", (unsigned long)id);

    while (1) {
        usleep(100000); 
    }

    return NULL;
}

void demo_join_all(void) {
    const int N = 8;
    mythread_t th[N];

    printf("=== demo_join_all: creating %d threads ===\n", N);

    for (int i = 0; i < N; ++i) {
        mythread_create(&th[i], worker, (void*)(uintptr_t)i);
    }

    for (int i = 0; i < N; ++i) {
        void* ret = NULL;
        mythread_join(th[i], &ret);
        printf("thread %d -> retval=%lu\n",
               i, (unsigned long)(uintptr_t)ret);
    }

    printf("=== demo_join_all: done ===\n\n");
}

void demo_detach_two(void) {
    mythread_t th_finite, th_infinite;

    printf("=== demo_detach_two: create finite + infinite threads ===\n");

    mythread_create(&th_finite,   worker_finite,   (void*)(uintptr_t)1);
    mythread_create(&th_infinite, worker_infinite, (void*)(uintptr_t)2);

    printf("detaching finite thread...\n");
    mythread_detach(th_finite);
    printf("finite thread detached\n");

    printf("detaching infinite thread...\n");
    mythread_detach(th_infinite);
    printf("detaching infinite thread detached\n");
    printf("=== demo_detach_two: done  ===\n");
}

int main(void) {
    printf("===== TEST: JOIN =====\n");
    demo_join_all();

    printf("===== TEST: DETACH =====\n");
    demo_detach_two();
    usleep(2000000);  
    printf("main: finished\n");
    return 0;
}